#pragma once
#include <cstdint>
#include <optional>
#include <utility>
#include <random>
#include <thread>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <atomic>
#include <set>
#include <unordered_set>

namespace sy::utils
{
	/**
	* ELF Hash function
	* https://www.partow.net/programming/hashfunctions/index.html#StringHashing
	*/
	constexpr uint32_t ELFHash(const char* str)
	{
		unsigned int hash = 0;
		unsigned int x = 0;
		for (unsigned int idx = 0; idx < sizeof(str); ++idx)
		{
			hash = (hash << 4) + (*str);
			x = hash & 0xF0000000L;
			if (x != 0)
			{
				hash ^= (x >> 24);
			}

			hash &= ~x;
		}

		return hash;
	}
}

namespace sy
{
	template <typename T>
	using OptionalRef = std::optional<std::reference_wrapper<T>>;

	constexpr size_t DEFAULT_CHUNK_SIZE = 16384;
	constexpr size_t DEFAULT_CHUNK_ALIGNMENT = 64;

	enum class Entity : uint64_t {};
	constexpr Entity INVALID_ENTITY_HANDLE = static_cast<Entity>(0);
	constexpr size_t DEFAULT_COMPONENT_POOL_SIZE = 16;
	constexpr bool USE_RANDOM_NUM_FOR_ENTITY_HANDLE = false;
	inline Entity GenerateEntity()
	{
		using EntityUnderlyingType = std::underlying_type_t<Entity>;
		if (USE_RANDOM_NUM_FOR_ENTITY_HANDLE)
		{
			static thread_local std::mt19937_64 generator(
				std::hash<std::thread::id>{}(std::this_thread::get_id()));

			std::uniform_int_distribution<EntityUnderlyingType> dist(std::numeric_limits<EntityUnderlyingType>::min() + 1, std::numeric_limits<EntityUnderlyingType>::max());
			return static_cast<Entity>(dist(generator));
		}

		static std::atomic<EntityUnderlyingType> handle = 1;
		return static_cast<Entity>(handle++);
	}

	using ComponentID = uint32_t;
	constexpr ComponentID INVALID_COMPONET_ID = 0;

	template <typename T>
	constexpr ComponentID QueryComponentID()
	{
		return INVALID_COMPONET_ID;
	}

	template <typename T>
	constexpr ComponentID QueryComponentID(const T&)
	{
		return QueryComponentID<T>;
	}

	struct ComponentInfo
	{
		ComponentID ID = INVALID_COMPONET_ID;
		std::string Name;
		size_t Size;
		size_t Alignment;

		template <typename T>
		static ComponentInfo Generate()
		{
			ComponentInfo result{
				.ID = QueryComponentID<T>(),
				.Name = typeid(T).name(),
				.Size = sizeof(T),
				.Alignment = alignof(T) };

			return result;
		}
	};

	class Chunk
	{
	public:
		Chunk(const std::vector<ComponentInfo>& componentInfos) :
			mem(_aligned_malloc(DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_ALIGNMENT)),
			next(nullptr),
			currentSize(0)
		{
			if (mem != nullptr)
			{
				std::memset(mem, 0, DEFAULT_CHUNK_SIZE);
			}

			size_t offset = 0;
			for (const auto& info : componentInfos)
			{
				componentOffsetAndSize[info.ID] = std::make_pair(offset, info.Size);
				offset += info.Size;
			}

			sizeOfData = offset;
			// Save one slot for temporary data.
			maxNumOfComponents = (DEFAULT_CHUNK_SIZE / sizeOfData) - 1;
		}

		~Chunk()
		{
			if (next != nullptr)
			{
				delete next;
				next = nullptr;
			}

			if (mem != nullptr)
			{
				_aligned_free(mem);
				mem = nullptr;
			}
		}

		Chunk(const Chunk&) = delete;
		Chunk(Chunk&&) = delete;
		Chunk& operator=(const Chunk&) = delete;
		Chunk& operator=(Chunk&&) = delete;

		bool AttachTo(Entity entity)
		{
			if (IsFull())
			{
				if (next == nullptr)
				{
					next = new Chunk(componentOffsetAndSize, sizeOfData, maxNumOfComponents);
				}

				return next->AttachTo(entity);
			}
			else
			{
				if (!Contains(entity))
				{
					entityLUT[entity] = currentSize;
					++currentSize;
					return true;
				}
			}

			return false;
		}

		// Only if, for transfer data between superset and subset.
		bool TransferToOtherChunk(Entity target, Chunk& destChunk)
		{
			if (entityLUT.find(target) != entityLUT.end())
			{
				if (!destChunk.Contains(target))
				{
					Chunk& actualSrcChunk = (*this);
					Chunk& actualDestChunk = destChunk.AttachToWithChunkResult(target);
					for (auto& componentInfo : actualSrcChunk.componentOffsetAndSize)
					{
						if (actualDestChunk.Supports(componentInfo.first))
						{
							size_t srcComponentOffset = componentInfo.second.first;
							size_t srcComponentSize = componentInfo.second.second;
							const auto& destComponentInfo = actualDestChunk.componentOffsetAndSize[componentInfo.first];

							void* dest = (void*)((uintptr_t)actualDestChunk.mem + destComponentInfo.first);
							void* src = (void*)((uintptr_t)actualSrcChunk.mem + srcComponentOffset);
							std::memcpy(dest, src, srcComponentSize);
						}
					}

					actualSrcChunk.Remove(target);
					return true;
				}
			}
			else
			{
				if (next != nullptr)
				{
					return next->TransferToOtherChunk(target, destChunk);
				}
			}

			return false;
		}

		bool Remove(Entity entity)
		{
			if (entityLUT.find(entity) == entityLUT.end())
			{
				if (next != nullptr)
				{
					return next->Remove(entity);
				}
			}
			else
			{
				const size_t offsetIdx = entityLUT[entity];
				const size_t offset = sizeOfData * offsetIdx;
				void* dest = reinterpret_cast<void*>(((uintptr_t)mem) + offset);
				void* src = reinterpret_cast<void*>(((uintptr_t)mem) + (sizeOfData * (currentSize - 1)));
				std::memcpy(dest, src, sizeOfData);
				std::memset(src, 0, sizeOfData);

				entityLUT.erase(entity);
				const size_t newOffsetForReplacedEntity = offsetIdx;
				Entity replacedEntity = INVALID_ENTITY_HANDLE;
				for (auto& entityPair : entityLUT)
				{
					if (entityPair.second == (currentSize - 1))
					{
						replacedEntity = entityPair.first;
						break;
					}
				}
				entityLUT[replacedEntity] = newOffsetForReplacedEntity;

				--currentSize;
				return true;
			}

			return false;
		}

		template <typename T>
		OptionalRef<T> Retrieve(const Entity entity)
		{
			if (Supports<T>())
			{
				if (entityLUT.find(entity) != entityLUT.end())
				{
					const size_t offset = sizeOfData * entityLUT[entity];
					const size_t componentOffset = componentOffsetAndSize[QueryComponentID<T>()].first;
					T* component = reinterpret_cast<T*>(((uintptr_t)mem) + offset + componentOffset);
					return std::ref(*component);
				}
				else if (next != nullptr)
				{
					return next->Retrieve<T>(entity);
				}
			}

			return std::nullopt;
		}

		size_t SizeOfData() const { return sizeOfData; }
		size_t CurrentSize() const { return currentSize; }
		bool IsEmpty() const { return currentSize == 0; }
		bool IsFull() const { return currentSize == maxNumOfComponents; }
		bool Supports(ComponentID componentID) const { return componentOffsetAndSize.find(componentID) != componentOffsetAndSize.end(); }
		template <typename T>
		bool Supports() const { return Supports(QueryComponentID<T>()); }
		bool Contains(Entity entity) const 
		{ 
			return entityLUT.find(entity) != entityLUT.end() || ((next != nullptr) ? next->Contains(entity) : false);
		}

	private:
		Chunk(const std::unordered_map<ComponentID, std::pair<size_t, size_t>>& componentOffsetAndSize,
			size_t sizeOfData,
			size_t maxNumOfComponents) :
			mem(_aligned_malloc(DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_ALIGNMENT)),
			componentOffsetAndSize(componentOffsetAndSize),
			next(nullptr),
			sizeOfData(sizeOfData),
			maxNumOfComponents(maxNumOfComponents),
			currentSize(0)
		{
		}

		Chunk& AttachToWithChunkResult(Entity entity)
		{
			if (IsFull())
			{
				if (next == nullptr)
				{
					next = new Chunk(componentOffsetAndSize, sizeOfData, maxNumOfComponents);
				}

				return next->AttachToWithChunkResult(entity);
			}
			else
			{
				if (!Contains(entity))
				{
					entityLUT[entity] = currentSize;
					++currentSize;
					return (*this);
				}
			}

			return (*this);
		}

		size_t OffsetOfEntity(Entity entity) const
		{
			return entityLUT.find(entity)->second * sizeOfData;
		}

		size_t OffsetOfEntityComponent(Entity entity, ComponentID componentID) const
		{
			return OffsetOfEntity(entity) + componentOffsetAndSize.find(componentID)->first;
		}

	private:
		void* mem;
		std::unordered_map<ComponentID, std::pair<size_t, size_t>> componentOffsetAndSize;
		std::unordered_map<Entity, size_t> entityLUT;
		Chunk* next;
		size_t sizeOfData;
		size_t maxNumOfComponents;
		size_t currentSize;

	};

	class ComponentArchive
	{
	public:
		~ComponentArchive()
		{
			for (auto& chunkPair : archetypeChunkLUT)
			{
				if (chunkPair.second != nullptr)
				{
					delete chunkPair.second;
				}
			}
		}

		ComponentArchive(const ComponentArchive&) = delete;
		ComponentArchive(ComponentArchive&&) = delete;
		ComponentArchive& operator=(const ComponentArchive&) = delete;
		ComponentArchive& operator=(ComponentArchive&&) = delete;

		static ComponentArchive& Get()
		{
			static std::unique_ptr<ComponentArchive> instance(new ComponentArchive);
			return *instance;
		}

		template <typename T>
		void Archiving()
		{
			componentInfoLUT[QueryComponentID<T>()] = ComponentInfo::Generate<T>();
		}

		bool AttachTo(const Entity entity, const ComponentID componentID)
		{
			if (componentID != INVALID_COMPONET_ID)
			{
				if (entityLUT.find(entity) == entityLUT.end())
				{
					entityLUT[entity] = { };
				}

				auto& componentSet = entityLUT[entity];
				if (componentSet.find(componentID) == componentSet.end())
				{
					const auto oldComponentSet = componentSet;
					componentSet.insert(componentID);
					auto& targetChunk = FindOrCreateArchetypeChunkIfDoesNotExist(componentSet);
					if (!oldComponentSet.empty())
					{
						// Move Data from old chunk to target(new) chunk
						auto& oldChunk = FindOrCreateArchetypeChunkIfDoesNotExist(oldComponentSet);
						// subset to superset
						return oldChunk.TransferToOtherChunk(entity, targetChunk);
					}

					return targetChunk.AttachTo(entity);
				}

			}

			return false;
		}

		template <typename T>
		bool AttachTo(const Entity entity)
		{
			return AttachTo(entity, QueryComponentID<T>());
		}

		bool DetachFrom(const Entity entity, const ComponentID componentID)
		{
			// @TODO
			if (componentID != INVALID_COMPONET_ID)
			{
				if (Contains(entity, componentID))
				{
					auto& componentSet = entityLUT[entity];
					if (componentSet.find(componentID) != componentSet.end())
					{
						const auto oldComponentSet = componentSet;
						componentSet.erase(componentID);
						auto& oldChunk = FindOrCreateArchetypeChunkIfDoesNotExist(oldComponentSet);
						auto& targetChunk = FindOrCreateArchetypeChunkIfDoesNotExist(componentSet);
						if (!componentSet.empty())
						{
							// Move Data from old chunk to target(new) chunk
							// superset to subset
							return oldChunk.TransferToOtherChunk(entity, targetChunk);
						}

						return oldChunk.Remove(entity);
					}
				}
			}

			return false;
		}

		template <typename T>
		bool DetachFrom(const Entity entity)
		{
			return DetachFrom(entity, QueryComponentID<T>());
		}

		template <typename T>
		OptionalRef<T> Retrieve(const Entity entity)
		{
			if (Contains<T>(entity))
			{
				Chunk& chunk = FindOrCreateArchetypeChunkIfDoesNotExist(entityLUT[entity]);
				return chunk.Retrieve<T>(entity);
			}

			return std::nullopt;
		}

		bool Contains(const Entity entity, const ComponentID componentID) const
		{
			auto entityLUTItr = entityLUT.find(entity);
			if (entityLUTItr != entityLUT.end())
			{
				const auto& componentSetOfEntity = (entityLUTItr->second);
				return (componentSetOfEntity.find(componentID) != componentSetOfEntity.end());
			}

			return false;
		}

		template <typename T>
		bool Contains(const Entity entity) const
		{
			return Contains(entity, QueryComponentID<T>());
		}

		bool IsSameArchetype(const Entity lhs, const Entity rhs) const
		{
			auto lhsItr = entityLUT.find(lhs);
			auto rhsItr = entityLUT.find(rhs);
			if (lhsItr != entityLUT.end() && rhsItr != entityLUT.end())
			{
				return lhsItr->second == rhsItr->second;
			}

			// If both itrerator is entityLUT.end(), it means those are empty and at same time equal archetype.
			return lhsItr == rhsItr;
		}

		size_t NumOfArchetype() const { return archetypeChunkLUT.size(); }

	private:
		ComponentArchive() = default;

		Chunk& FindOrCreateArchetypeChunkIfDoesNotExist(const std::set<ComponentID>& componentSet)
		{
			for (auto& chunkListPair : archetypeChunkLUT)
			{
				if (componentSet == chunkListPair.first)
				{
					return *chunkListPair.second;
				}
			}

			archetypeChunkLUT.emplace_back(
				std::make_pair(
					componentSet, 
					new Chunk(RetrieveComponentInfoFromSet(componentSet))));

			return *archetypeChunkLUT.back().second;
		}

		std::vector<ComponentInfo> RetrieveComponentInfoFromSet(const std::set<ComponentID>& componentSet) const
		{
			std::vector<ComponentInfo> res{ };
			res.reserve(componentSet.size());
			for (ComponentID componentID : componentSet)
			{
				res.emplace_back(componentInfoLUT.find(componentID)->second);
			}

			return res;
		}

	private:
		std::unordered_map<ComponentID, ComponentInfo> componentInfoLUT;
		std::unordered_map<Entity, std::set<ComponentID>> entityLUT;
		std::vector<std::pair<std::set<ComponentID>, Chunk*>> archetypeChunkLUT;

	};
}

#define COMPONENT_TYPE_HASH(x) sy::utils::ELFHash(#x)

#define DeclareComponent(ComponentType) \
struct ComponentType##Registeration \
{ \
	ComponentType##Registeration() \
	{ \
		auto& archive = sy::ComponentArchive::Get(); \
		archive.Archiving<ComponentType>(); \
	}	\
private: \
	static ComponentType##Registeration registeration; \
};\
template <> \
constexpr sy::ComponentID sy::QueryComponentID<ComponentType>() \
{	\
	constexpr uint32_t genID = COMPONENT_TYPE_HASH(ComponentType); \
	static_assert(genID != 0 && "Generated Component ID is not valid."); \
	return static_cast<sy::ComponentID>(genID);	\
}

#define DefineComponent(ComponentType) ComponentType##Registeration ComponentType##Registeration::registeration;