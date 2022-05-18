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
#include <cassert>
#include <queue>
#include <map>

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

	template <typename T, typename Key>
	bool HasKey(const T& data, const Key& key)
	{
		return data.find(key) != data.end();
	}

	template <typename T, typename Key>
	bool HasKeyV(const T& data, const Key& key)
	{
		return std::find_if(data.begin(), data.end(), 
			[key](const auto& targetData)
			{
				return targetData.first == key;
			}) != data.end();
	}

	inline size_t AlignForwardAdjustment(const void* ptr, const size_t alignment)
	{
		const size_t adjustment = alignment - (reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(alignment - 1));
		return (adjustment == alignment) ? 0 : adjustment;
	}
}

namespace sy
{
	template <typename T>
	using OptionalRef = std::optional<std::reference_wrapper<T>>;
}

namespace sy
{
	enum class Entity : uint64_t {};
	constexpr Entity INVALID_ENTITY_HANDLE = static_cast<Entity>(0);
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

	constexpr size_t DEFAULT_CHUNK_SIZE = 16384;		// bytes
	// https://stackoverflow.com/questions/34860366/why-buffers-should-be-aligned-on-64-byte-boundary-for-best-performance
	constexpr size_t DEFAULT_CHUNK_ALIGNMENT = 64;		// bytes
	constexpr size_t DEFAULT_CHUNK_POOL_SIZE = 536870912; // bytes 
	//constexpr size_t DEFAULT_CHUNK_POOL_SIZE = 67108864; // bytes

	struct ComponentRange
	{
		size_t Offset = 0;
		size_t Size = 0;

		static void ComponentCopy(void* destOffset, void* srcOffset, ComponentRange destRange, ComponentRange srcRange)
		{
			assert(destRange.Size == srcRange.Size);
			void* dest = (void*)((uintptr_t)destOffset + destRange.Offset);
			void* src = (void*)((uintptr_t)srcOffset + srcRange.Offset);
			std::memcpy(dest, src, srcRange.Size);
		}
	};

	class ChunkPool
	{
	public:
		class Chunk
		{
		public:
			Chunk(ChunkPool& pool, const std::vector<ComponentInfo>& componentInfos) :
				pool(pool),
				mem(pool.Allocate()),
				next(nullptr),
				currentSize(0)
			{
				if (mem != nullptr)
				{
 					std::memset(mem, 0, DEFAULT_CHUNK_SIZE);
				}

				size_t offset = 0;
				componentRanges.reserve(componentInfos.size());
				for (size_t idx = 0; idx < componentInfos.size(); ++idx)
				{
					const auto& info = componentInfos[idx];
					componentRanges.emplace_back(std::make_pair(info.ID,
						ComponentRange{
						.Offset = offset,
						.Size = info.Size
						}));

					offset += info.Size;
				}
			}

			Chunk(ChunkPool& pool, const std::vector<std::pair<ComponentID, ComponentRange>>& componentRanges) :
				pool(pool),
				mem(pool.Allocate()),
				componentRanges(componentRanges),
				next(nullptr),
				currentSize(0)
			{
			}

			~Chunk()
			{
				pool.Deallocate(mem);
				if (next != nullptr)
				{
					delete next;
					next = nullptr;
				}
			}

			Chunk(const Chunk&) = delete;
			Chunk(Chunk&&) = delete;
			Chunk& operator=(const Chunk&) = delete;
			Chunk& operator=(Chunk&&) = delete;

			bool Create(Entity entity)
			{
				if (IsFull())
				{
					if (next == nullptr)
					{
						next = CloneEmpty();
					}

					return next->Create(entity);
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

			// Use this function only for operate to transfer data between superset and subset.
			bool TransferEntityTo(Entity target, Chunk& destChunk)
			{
				if (utils::HasKey(entityLUT, target))
				{
					if (!destChunk.Contains(target))
					{
						Chunk& actualSrcChunk = (*this);
						Chunk& actualDestChunk = destChunk.AttachToWithChunkResult(target);
						for (auto& componentInfo : actualSrcChunk.componentRanges)
						{
							ComponentID targetComponentID = componentInfo.first;
							if (actualDestChunk.Supports(componentInfo.first))
							{
								ComponentRange srcComponentRange = componentInfo.second;
								ComponentRange destComponentRange = actualDestChunk.QueryComponentRange(targetComponentID);

								void* destOffset = actualDestChunk.OffsetAddressOf(target);
								void* srcOffset = actualSrcChunk.OffsetAddressOf(target);
								ComponentRange::ComponentCopy(destOffset, srcOffset, destComponentRange, srcComponentRange);
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
						return next->TransferEntityTo(target, destChunk);
					}
				}

				return false;
			}

			bool Remove(Entity entity)
			{
				if (!utils::HasKey(entityLUT, entity))
				{
					if (next != nullptr)
					{
						return next->Remove(entity);
					}
				}
				else
				{
					const size_t sizeOfData = SizeOfData();
					void* dest = OffsetAddressOf(entity);
					void* src = OffsetAddressOfBack();
					std::memcpy(dest, src, sizeOfData);
 					std::memset(src, 0, sizeOfData);

					const size_t targetEntityOffset = OffsetOf(entity);
					entityLUT.erase(entity);

					for (auto& entityOffsetPair : entityLUT)
					{
						Entity entity = entityOffsetPair.first;
						size_t offset = entityOffsetPair.second;
						if (offset == (currentSize - 1))
						{
							entityLUT[entity] = targetEntityOffset;
							break;
						}
					}

					--currentSize;
					return true;
				}

				return false;
			}

			template <typename T>
			OptionalRef<T> Reference(const Entity fromEntity)
			{
				if (Supports<T>())
				{
					if (utils::HasKey(entityLUT, fromEntity))
					{
						T* component = reinterpret_cast<T*>(OffsetAddressOf(fromEntity, QueryComponentID<T>()));
						return std::ref(*component);
					}
					else if (next != nullptr)
					{
						return next->Reference<T>(fromEntity);
					}
				}

				return std::nullopt;
			}

			template <typename T>
			OptionalRef<const T> Reference(const Entity fromEntity) const
			{
				if (Supports<T>())
				{
					if (utils::HasKey(entityLUT, fromEntity))
					{
						T* component = reinterpret_cast<T*>(OffsetAddressOf(fromEntity, QueryComponentID<T>()));
						return std::cref(*component);
					}
					else if (next != nullptr)
					{
						return next->Reference<T>(fromEntity);
					}
				}

				return std::nullopt;
			}

			inline size_t SizeOfData() const
			{
				const auto& range = componentRanges[componentRanges.size() - 1].second;
				return range.Size + range.Offset;
			}
			size_t CurrentSize() const { return currentSize; }
			inline size_t MaxNumOfComponents() const { return (DEFAULT_CHUNK_SIZE / SizeOfData()) - 1; }
			bool IsEmpty() const { return currentSize == 0; }
			bool IsFull() const { return currentSize == MaxNumOfComponents(); }
			bool Supports(ComponentID componentID) const { return componentID != INVALID_COMPONET_ID && utils::HasKeyV(componentRanges, componentID); }
			template <typename T>
			bool Supports() const { return Supports(QueryComponentID<T>()); }
			bool Contains(Entity entity) const
			{
				assert(mem != nullptr);
				return utils::HasKey(entityLUT, entity) || ((next != nullptr) ? next->Contains(entity) : false);
			}

			ComponentRange QueryComponentRange(ComponentID componentID) const
			{
				if (Supports(componentID))
				{
					return std::find_if(componentRanges.begin(), componentRanges.end(),
						[componentID](const auto& targetData)
						{
							return targetData.first == componentID;
						})->second;
				}

				return ComponentRange();
			}

			template <typename T>
			ComponentRange QueryComponentRange() const
			{
				return QueryComponentRange(QueryComponentID<T>());
			}

		private:
			Chunk* CloneEmpty() const
			{
				return new Chunk(pool, componentRanges);
			}

			Chunk& AttachToWithChunkResult(Entity entity)
			{
				if (IsFull())
				{
					if (next == nullptr)
					{
						next = CloneEmpty();
					}

					return next->AttachToWithChunkResult(entity);
				}
				else
				{
					if (!utils::HasKey(entityLUT, entity))
					{
						entityLUT[entity] = currentSize;
						++currentSize;
						return (*this);
					}
				}

				return (*this);
			}

			size_t OffsetOf(Entity entity) const
			{
				return entityLUT.find(entity)->second * SizeOfData();
			}

			void* OffsetAddressOf(Entity entity) const
			{
				return (void*)((uintptr_t)mem + OffsetOf(entity));
			}

			size_t OffsetOf(Entity entity, ComponentID componentID) const
			{
				const ComponentRange componentRange = QueryComponentRange(componentID);
				return OffsetOf(entity) + componentRange.Offset;
			}

			void* OffsetAddressOf(Entity entity, ComponentID componentID) const
			{
				return (void*)((uintptr_t)mem + OffsetOf(entity, componentID));
			}

			void* OffsetAddressOfBack() const
			{
				return (void*)((uintptr_t)mem + (SizeOfData() * (currentSize - 1)));
			}

		private:
			ChunkPool& pool;
			void* mem;
			std::unordered_map<Entity, size_t> entityLUT;
			std::vector<std::pair<ComponentID, ComponentRange>> componentRanges;
			size_t currentSize;
			Chunk* next;

		};

	public:
		ChunkPool() :
			mem(_aligned_malloc(DEFAULT_CHUNK_POOL_SIZE, DEFAULT_CHUNK_ALIGNMENT))
		{
			if (mem != nullptr)
			{
				std::memset(mem, 0, DEFAULT_CHUNK_POOL_SIZE);
			}

			for (size_t offset = 0; (offset + DEFAULT_CHUNK_SIZE) < DEFAULT_CHUNK_POOL_SIZE; offset += DEFAULT_CHUNK_SIZE)
			{
				void* rawPointer = (void*)((uintptr_t)mem + offset);
				freeChunks.push(rawPointer);
			}
		}

		~ChunkPool()
		{
			if (mem != nullptr)
			{
				_aligned_free(mem);
			}
		}

		void* Allocate()
		{
			void* dest = freeChunks.front();
			freeChunks.pop();
			return dest;
		}

		void Deallocate(void* chunk)
		{
			if (chunk != nullptr)
			{
				freeChunks.push(chunk);
			}
		}

	private:
		void* mem;
		std::queue<void*> freeChunks;

	};

	using Chunk = ChunkPool::Chunk;
	class ComponentArchive
	{
	public:
		using Archetype = std::set<ComponentID>;

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
		void Archive()
		{
			componentInfoLUT[QueryComponentID<T>()] = ComponentInfo::Generate<T>();
		}

		bool Attach(const ComponentID componentID, const Entity toEntity)
		{
			if (componentID != INVALID_COMPONET_ID)
			{
				if (!utils::HasKey(archetypeLUT, toEntity))
				{
					archetypeLUT[toEntity] = { };
				}

				auto& archetype = archetypeLUT[toEntity];
				if (!utils::HasKey(archetype, componentID))
				{
					const auto oldArchetype= archetype;
					archetype.insert(componentID);
					auto& newChunk = FindOrCreateArchetypeChunk(archetype);
					if (!oldArchetype.empty())
					{
						// Move Data from old chunk to target(new) chunk
						auto& oldChunk = FindOrCreateArchetypeChunk(oldArchetype);
						// subset to superset
						return oldChunk.TransferEntityTo(toEntity, newChunk);
					}

					// Entity was empty, so there a no data to transfer.
					return newChunk.Create(toEntity);
				}

			}

			return false;
		}

		template <typename T>
		bool Attach(const Entity toEntity)
		{
			return Attach(QueryComponentID<T>(), toEntity);
		}

		bool Detach(const ComponentID componentID, const Entity fromEntity)
		{
			if (componentID != INVALID_COMPONET_ID)
			{
				if (Contains(fromEntity, componentID))
				{
					auto& archetype = archetypeLUT[fromEntity];
					if (utils::HasKey(archetype, componentID))
					{
						const auto oldArchetype = archetype;
						archetype.erase(componentID);
						auto& oldChunk = FindOrCreateArchetypeChunk(oldArchetype);
						if (!archetype.empty())
						{
							auto& targetChunk = FindOrCreateArchetypeChunk(archetype);
							// Move Data from old chunk to target(new) chunk
							// superset to subset
							return oldChunk.TransferEntityTo(fromEntity, targetChunk);
						}

						// If empty archetype
						return oldChunk.Remove(fromEntity);
					}
				}
			}

			return false;
		}

		template <typename T>
		bool Detach(const Entity fromEntity)
		{
			return Detach(QueryComponentID<T>(), fromEntity);
		}

		template <typename T>
		OptionalRef<T> Reference(const Entity fromEntity)
		{
			if (Contains<T>(fromEntity))
			{
				Chunk& chunk = FindOrCreateArchetypeChunk(archetypeLUT[fromEntity]);
				return chunk.Reference<T>(fromEntity);
			}

			return std::nullopt;
		}

		template <typename T>
		OptionalRef<const T> Reference(const Entity fromEntity) const
		{
			if (Contains<T>(fromEntity))
			{
				Chunk& chunk = FindOrCreateArchetypeChunk(archetypeLUT[fromEntity]);
				return chunk.Reference<T>(fromEntity);
			}

			return std::nullopt;
		}

		bool Contains(const Entity entity, const ComponentID componentID) const
		{
			auto foundArchetypeItr = archetypeLUT.find(entity);
			if (foundArchetypeItr != archetypeLUT.end())
			{
				const auto& foundArchetype = (foundArchetypeItr->second);
				return utils::HasKey(foundArchetype, componentID);
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
			auto lhsItr = archetypeLUT.find(lhs);
			auto rhsItr = archetypeLUT.find(rhs);
			if (lhsItr != archetypeLUT.end() && rhsItr != archetypeLUT.end())
			{
				const auto& lhsArchetype = lhsItr->second;
				const auto& rhsArchetype = rhsItr->second;
				return lhsArchetype == rhsArchetype;
			}

			// If both itrerator is entityLUT.end(), it means those are empty and at same time equal archetype.
			return lhsItr == rhsItr;
		}

		size_t NumOfArchetype() const { return archetypeChunkLUT.size(); }

		Archetype QueryArchetype(const Entity entity) const
		{
			if (utils::HasKey(archetypeLUT, entity))
			{
				return archetypeLUT.find(entity)->second;
			}

			return Archetype();
		}

		template <typename T>
		bool SupportComponent() const
		{
			return utils::HasKey(componentInfoLUT, QueryComponentID<T>());
		}

		bool SupportComponent(ComponentID componentID) const
		{
			return utils::HasKey(componentInfoLUT, componentID);
		}

	private:
		ComponentArchive() = default;

		Chunk& FindOrCreateArchetypeChunk(const Archetype& archetype)
		{
			for (auto& chunkListPair : archetypeChunkLUT)
			{
				if (archetype == chunkListPair.first)
				{
					return *chunkListPair.second;
				}
			}

			archetypeChunkLUT.emplace_back(
				std::make_pair(
					archetype,
					new Chunk(chunkPool, RetrieveComponentInfoFromArchetype(archetype))));

			return *archetypeChunkLUT.back().second;
		}

		std::vector<ComponentInfo> RetrieveComponentInfoFromArchetype(const Archetype& archetype) const
		{
			std::vector<ComponentInfo> res{ };
			res.reserve(archetype.size());
			for (ComponentID componentID : archetype)
			{
				res.emplace_back(componentInfoLUT.find(componentID)->second);
			}

			return res;
		}

	private:
		ChunkPool chunkPool;
		std::unordered_map<ComponentID, ComponentInfo> componentInfoLUT;
		std::unordered_map<Entity, Archetype> archetypeLUT;
		std::vector<std::pair<Archetype, ChunkPool::Chunk*>> archetypeChunkLUT;

	};

	std::vector<Entity> FilterAll(const std::vector<Entity>& entities, std::vector<ComponentID> components)
	{
		const ComponentArchive& componentArchive = ComponentArchive::Get();
		std::vector<Entity> result;
		result.reserve(entities.size());
		for (const Entity entity : entities)
		{
			bool bIsSufficient = true;
			ComponentArchive::Archetype archetype = componentArchive.QueryArchetype(entity);
			for (ComponentID componentID : components)
			{
				if (!utils::HasKey(archetype, componentID))
				{
					bIsSufficient = false;
					break;
				}
			}

			if (bIsSufficient)
			{
				result.emplace_back(entity);
			}
		}

		return result;
	}

	template <typename... T>
	inline std::vector<Entity> FilterAll(const std::vector<Entity>& entities)
	{
		auto initList = { QueryComponentID<T>()... };
		return FilterAll(entities, std::vector<ComponentID>{ initList });
	}
}

#define COMPONENT_TYPE_HASH(x) sy::utils::ELFHash(#x)

#define DeclareComponent(ComponentType) \
struct ComponentType##Registeration \
{ \
	ComponentType##Registeration() \
	{ \
		auto& archive = sy::ComponentArchive::Get(); \
		archive.Archive<ComponentType>(); \
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