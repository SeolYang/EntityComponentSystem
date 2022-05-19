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
#include <functional>
#include <mutex>

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
}

namespace sy
{
	template <typename T>
	using OptionalRef = std::optional<std::reference_wrapper<T>>;
}

namespace sy
{
	struct Component
	{
		virtual ~Component() = default;
	};

	template <typename T>
	concept ComponentType = std::is_base_of_v<Component, T>;

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

	template <ComponentType T>
	constexpr ComponentID QueryComponentID()
	{
		return INVALID_COMPONET_ID;
	}

	template <ComponentType T>
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

	// Bigger chunk size = lower level of indirection
	constexpr size_t DEFAULT_CHUNK_SIZE = 16384;
	// https://stackoverflow.com/questions/34860366/why-buffers-should-be-aligned-on-64-byte-boundary-for-best-performance
	constexpr size_t DEFAULT_CHUNK_ALIGNMENT = 64;

	struct ComponentRange
	{
		size_t Offset = 0;
		size_t Size = 0;

		static void ComponentCopy(void* destEntityAddress, void* srcEntityAddress, ComponentRange destRange, ComponentRange srcRange)
		{
			assert(destRange.Size == srcRange.Size);
			void* dest = (void*)((uintptr_t)destEntityAddress + destRange.Offset);
			void* src = (void*)((uintptr_t)srcEntityAddress + srcRange.Offset);
			std::memcpy(dest, src, srcRange.Size);
		}

		static void* ComponentAddress(void* entityOffsetAddress, ComponentRange range)
		{
			return (void*)((uintptr_t)entityOffsetAddress + range.Offset);
		}
	};

	class Chunk
	{
	public:
		Chunk(const size_t sizeOfData, const size_t chunkSize = DEFAULT_CHUNK_SIZE, const size_t chunkAlignment = DEFAULT_CHUNK_ALIGNMENT) :
			mem(_aligned_malloc(chunkSize, chunkAlignment)),
			sizeOfData(sizeOfData),
			maxNumOfAllocations(DEFAULT_CHUNK_SIZE / sizeOfData),
			sizeOfChunk(chunkSize),
			alignmentOfChunk(chunkAlignment)
		{
			for (size_t allocationIndex = 0; allocationIndex < MaxNumOfAllocations(); ++allocationIndex)
			{
				allocationPool.push_back(allocationIndex);
			}
		}

		Chunk(Chunk&& rhs) noexcept :
			mem(std::exchange(rhs.mem, nullptr)),
			allocationPool(std::move(allocationPool)),
			sizeOfData(std::exchange(rhs.sizeOfData, 0)),
			maxNumOfAllocations(std::exchange(rhs.maxNumOfAllocations, 0)),
			sizeOfChunk(std::exchange(rhs.sizeOfChunk, 0)),
			alignmentOfChunk(std::exchange(rhs.alignmentOfChunk, 0))
		{
		}

		~Chunk()
		{
			if (mem != nullptr)
			{
				_aligned_free(mem);
				mem = nullptr;
			}
		}

		Chunk& operator=(Chunk&& rhs) noexcept
		{
			mem = std::exchange(rhs.mem, nullptr);
			allocationPool = std::move(allocationPool);
			sizeOfData = std::exchange(rhs.sizeOfData, 0);
			maxNumOfAllocations = std::exchange(rhs.maxNumOfAllocations, 0);
			sizeOfChunk = std::exchange(rhs.sizeOfChunk, 0);
			alignmentOfChunk = std::exchange(rhs.alignmentOfChunk, 0);
			return (*this);
		}

		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;

		/** Return index of allocation */
		size_t Allocate()
		{
			assert(!IsFull());
			size_t alloc = allocationPool.front();
			allocationPool.pop_front();
			return alloc;
		}

		/** Return moved allocation, only if return 0 when numOfAllocation is 1 */
		void Deallocate(size_t at)
		{
			assert(at < MaxNumOfAllocations());
			assert((std::find(allocationPool.cbegin(), allocationPool.cend(), at) == allocationPool.cend()));
			allocationPool.push_back(at);
		}

		void* AddressOf(size_t at) const
		{
			assert(NumOfAllocations() > 0);
			return (void*)((uintptr_t)mem + (at * sizeOfData));
		}

		inline bool IsFull() const { return allocationPool.empty(); }
		inline size_t MaxNumOfAllocations() const { return maxNumOfAllocations - 1; }
		inline size_t NumOfAllocations() const { return MaxNumOfAllocations() - allocationPool.size(); }
		inline size_t SizeOfChunk() const { return sizeOfChunk; }
		inline size_t AlignmentOfChunk() const { return alignmentOfChunk; }

	private:
		void* mem;
		std::deque<size_t> allocationPool;
		size_t sizeOfData;
		size_t maxNumOfAllocations;
		size_t sizeOfChunk;
		size_t alignmentOfChunk;

	};

	class ChunkList
	{
	public:
		struct Allocation
		{
			size_t ChunkIndex = size_t(-1);
			size_t AllocationIndexOfEntity = size_t(-1);
		};

		struct ComponentAllocationInfo
		{
			ComponentRange Range;
			ComponentID ID = INVALID_COMPONET_ID;
		};

	public:
		ChunkList(const std::vector<ComponentInfo>& componentInfos)
		{
			size_t offset = 0;
			for (const ComponentInfo& info : componentInfos)
			{
				componentAllocInfos.emplace_back(ComponentAllocationInfo
					{
						.Range = ComponentRange
						{
							.Offset = offset,
							.Size = info.Size
						},
						.ID = info.ID
					});
				offset += info.Size;
			}

			sizeOfData = offset;
		}

		ChunkList(ChunkList&& rhs) noexcept :
			chunks(std::move(rhs.chunks)),
			componentAllocInfos(std::move(rhs.componentAllocInfos)),
			allocationLUT(std::move(rhs.allocationLUT)),
			sizeOfData(rhs.sizeOfData)
		{
		}

		~ChunkList() = default;

		ChunkList(const ChunkList&) = delete;
		ChunkList& operator=(const ChunkList&) = delete;

		ChunkList& operator=(ChunkList&& rhs) noexcept
		{
			chunks = std::move(rhs.chunks);
			componentAllocInfos = std::move(rhs.componentAllocInfos);
			allocationLUT = std::move(rhs.allocationLUT);
			sizeOfData = rhs.sizeOfData;
			return (*this);
		}

		/** It doesn't call anyof constructor. */
		void* Create(Entity entity)
		{
			if (!utils::HasKey(allocationLUT, entity))
			{
				size_t freeChunkIndex = 0;
				for (; freeChunkIndex < chunks.size(); ++freeChunkIndex)
				{
					if (!chunks[freeChunkIndex].IsFull())
					{
						break;
					}
				}

				bool bDoesNotFoundFreeChunk = freeChunkIndex >= chunks.size();
				if (bDoesNotFoundFreeChunk)
				{
					chunks.emplace_back(sizeOfData);
				}

				Chunk& chunk = chunks[freeChunkIndex];
				size_t allocIndex = chunk.Allocate();

				allocationLUT[entity] = Allocation{
					.ChunkIndex = freeChunkIndex,
					.AllocationIndexOfEntity = allocIndex
				};

				return AddressOf(entity);
			}

			return nullptr;
		}

		/** It doesn't call anyof destructor. */
		void Destroy(Entity entity)
		{
			if (utils::HasKey(allocationLUT, entity))
			{
				const auto& allocation = allocationLUT[entity];
				chunks[allocation.ChunkIndex].Deallocate(allocation.AllocationIndexOfEntity);
				allocationLUT.erase(entity);
			}
		}

		bool HasEntity(const Entity entity) const
		{
			return utils::HasKey(allocationLUT, entity);
		}

		std::vector<ComponentAllocationInfo> ComponentAllocationInfos() const { return componentAllocInfos; }

		ComponentAllocationInfo AllocationInfoOfComponent(const ComponentID componentID) const
		{
			auto found = std::find_if(componentAllocInfos.cbegin(), componentAllocInfos.cend(), [componentID](const ComponentAllocationInfo& info)
				{
					return componentID == info.ID;
				});

			return (*found);
		}

		bool Support(const ComponentID componentID) const
		{
			auto found = std::find_if(componentAllocInfos.cbegin(), componentAllocInfos.cend(), [componentID](const ComponentAllocationInfo& info)
				{
					return componentID == info.ID;
				});

			return found != componentAllocInfos.cend();
		}

		void* AddressOf(const Entity entity) const
		{
			if (HasEntity(entity))
			{
				const auto& allocation = allocationLUT.find(entity)->second;
				return chunks[allocation.ChunkIndex].AddressOf(allocation.AllocationIndexOfEntity);
			}

			return nullptr;
		}

		void* AddressOf(const Entity entity, const ComponentID componentID) const
		{
			if (Support(componentID) && HasEntity(entity))
			{
				const auto& allocation = allocationLUT.find(entity)->second;
				const auto componentAllocInfo = AllocationInfoOfComponent(componentID);
				void* entityAddress = chunks[allocation.ChunkIndex].AddressOf(allocation.AllocationIndexOfEntity);

				return ComponentRange::ComponentAddress(entityAddress, componentAllocInfo.Range);
			}

			return nullptr;
		}

		/** Just memory data copy, it never call any constructor or destructor. */
		static void MoveEntity(const Entity entity, ChunkList& src, ChunkList& dest)
		{
			bool bValidation = src.HasEntity(entity) && dest.HasEntity(entity);
			assert(bValidation);

			if (bValidation)
			{
				void* destAddress = dest.AddressOf(entity);
				void* srcAddress = src.AddressOf(entity);
				const auto& srcComponentAllocInfos = src.componentAllocInfos;
				const auto& destComponentAllocInfos = dest.componentAllocInfos;
				for (const auto& srcComponentAllocInfo : srcComponentAllocInfos)
				{
					for (const auto& destComponentAllocInfo : destComponentAllocInfos)
					{
						if (srcComponentAllocInfo.ID == destComponentAllocInfo.ID)
						{
							ComponentRange::ComponentCopy(destAddress, srcAddress, destComponentAllocInfo.Range, srcComponentAllocInfo.Range);
						}
					}
				}

				src.Destroy(entity);
			}
		}

	private:
		std::vector<Chunk> chunks;
		std::vector<ComponentAllocationInfo> componentAllocInfos;
		std::unordered_map<Entity, Allocation> allocationLUT;
		size_t sizeOfData;

	};

	class ComponentArchive
	{
	public:
		using Archetype = std::set<ComponentID>;

		struct DynamicComponentData
		{
			ComponentInfo Info;
			std::function<void(void*)> DefaultConstructor;
			std::function<void(void*)> Destructor;
		};

	public:
		ComponentArchive(const ComponentArchive&) = delete;
		ComponentArchive(ComponentArchive&&) = delete;
		ComponentArchive& operator=(const ComponentArchive&) = delete;
		ComponentArchive& operator=(ComponentArchive&&) = delete;

		~ComponentArchive()
		{
			std::vector<Entity> remainEntities;
			remainEntities.reserve(archetypeLUT.size());
			for (auto& entityArchetypePair : archetypeLUT)
			{
				remainEntities.emplace_back(entityArchetypePair.first);
			}

			for (const Entity entity : remainEntities)
			{
				Destroy(entity);
			}
		}

		static ComponentArchive& Instance()
		{
			std::call_once(instanceCreationOnceFlag, []()
				{
					instance.reset(new ComponentArchive);
				});

			return *instance;
		}
		
		static void DestroyInstance()
		{
			std::call_once(instanceDestructionOnceFlag, []()
				{
					delete instance.release();
				});
		}

		template <ComponentType T>
		void Archive()
		{
			dynamicComponentDataLUT[QueryComponentID<T>()] = DynamicComponentData{
				.Info = ComponentInfo::Generate<T>(),
				.DefaultConstructor = [](void* ptr) { new (ptr) T(); },
				.Destructor = [](void* ptr) { reinterpret_cast<T*>(ptr)->~T(); }
			};
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

		bool HasArchetypeChunkList(const Archetype& archetype) const
		{
			for (const auto& chunkListPair : chunkListLUT)
			{
				if (chunkListPair.first == archetype)
				{
					return true;
				}
			}

			return false;
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

		Archetype QueryArchetype(const Entity entity) const
		{
			if (utils::HasKey(archetypeLUT, entity))
			{
				return archetypeLUT.find(entity)->second;
			}

			return Archetype();
		}

		/** Return nullptr, if component is already exist or failed to attach. */
		Component* Attach(const Entity entity, ComponentID componentID, bool bCallDefaultConstructor = true)
		{
			Component* result = nullptr;
			if (!Contains(entity, componentID))
			{
				if (!utils::HasKey(archetypeLUT, entity))
				{
					archetypeLUT[entity] = {};
				}

				Archetype& archetype = archetypeLUT[entity];
				Archetype oldArchetype = archetype;
				archetype.insert(componentID);

				size_t chunkList = FindOrCreateChunkList(archetype);
				void* entityAddress = ReferenceChunkList(chunkList).Create(entity);
				if (entityAddress != nullptr && !oldArchetype.empty())
				{
					size_t oldChunkList = FindOrCreateChunkList(oldArchetype);
					ChunkList::MoveEntity(entity, ReferenceChunkList(oldChunkList), ReferenceChunkList(chunkList));
				}

				result = reinterpret_cast<Component*>(ReferenceChunkList(chunkList).AddressOf(entity, componentID));

				if (bCallDefaultConstructor)
				{
					const DynamicComponentData& dynamicComponentData = dynamicComponentDataLUT[componentID];
					dynamicComponentData.DefaultConstructor(result);
				}

				return result;
			}

			return nullptr;
		}

		template <ComponentType T, typename... Args>
		T* Attach(const Entity entity, Args&&... args)
		{
			bool bShoudCallDefaultConstructor = (sizeof...(Args) == 0);
			Component* result = Attach(entity, QueryComponentID<T>(), bShoudCallDefaultConstructor);
			if (result != nullptr)
			{
				if (!bShoudCallDefaultConstructor)
				{
					return new (result) T(std::forward<Args>(args)...);
				}
			}

			return static_cast<T*>(result);
		}

		void Detach(const Entity entity, const ComponentID componentID)
		{
			if (Contains(entity, componentID))
			{
				Archetype& archetype = archetypeLUT[entity];
				Archetype oldArchetype = archetype;
				archetype.erase(componentID);

				size_t oldChunkList = FindOrCreateChunkList(oldArchetype);
				void* detachComponentPtr = ReferenceChunkList(oldChunkList).AddressOf(entity, componentID);
				const DynamicComponentData& dynamicComponentData = dynamicComponentDataLUT[componentID];
				dynamicComponentData.Destructor(detachComponentPtr);

				if (!archetype.empty())
				{
					size_t newChunkList = FindOrCreateChunkList(archetype);
					ReferenceChunkList(newChunkList).Create(entity);
					ChunkList::MoveEntity(entity,
						ReferenceChunkList(oldChunkList),
						ReferenceChunkList(newChunkList));
				}
				else
				{
					ReferenceChunkList(oldChunkList).Destroy(entity);
				}
			}
		}

		template <ComponentType T>
		void Detach(const Entity entity)
		{
			Detach(entity, QueryComponentID<T>());
		}

		template <ComponentType T>
		T* Get(const Entity entity) const
		{
			return static_cast<T*>(Get(entity, QueryComponentID<T>()));
		}

		void Destroy(const Entity entity)
		{
			if (utils::HasKey(archetypeLUT, entity))
			{
				const auto& archetype = archetypeLUT[entity];
				if (!archetype.empty())
				{
					size_t chunkList = FindOrCreateChunkList(archetype);
					for (ComponentID componentID : archetype)
					{
						void* detachComponentPtr = ReferenceChunkList(chunkList).AddressOf(entity, componentID);
						const DynamicComponentData& dynamicComponentData = dynamicComponentDataLUT[componentID];
						dynamicComponentData.Destructor(detachComponentPtr);
					}

					ReferenceChunkList(chunkList).Destroy(entity);
				}

				archetypeLUT.erase(entity);
			}
		}

	private:
		ComponentArchive() = default;

		size_t FindOrCreateChunkList(const Archetype& archetype)
		{
			size_t idx = 0;
			for (; idx < chunkListLUT.size(); ++idx)
			{
				if (chunkListLUT[idx].first == archetype)
				{
					return idx;
				}
			}

			chunkListLUT.emplace_back(archetype, ChunkList(RetrieveComponentInfosFromArchetype(archetype)));
			return idx;
		}

		/**
		* To prevent vector reallocations, always ref chunk list through this method.
		* And, Do not reference ChunkList directly!!!
		*/
		inline ChunkList& ReferenceChunkList(size_t idx)
		{
			return chunkListLUT[idx].second;
		}

		std::vector<ComponentInfo> RetrieveComponentInfosFromArchetype(const Archetype& archetype) const
		{
			std::vector<ComponentInfo> res{ };
			res.reserve(archetype.size());
			for (ComponentID componentID : archetype)
			{
				const auto& foundComponentDynamicData = dynamicComponentDataLUT.find(componentID)->second;
				res.emplace_back(foundComponentDynamicData.Info);
			}

			return res;
		}

		Component* Get(const Entity entity, const ComponentID componentID) const
		{
			if (Contains(entity, componentID))
			{
				const auto& archetype = archetypeLUT.find(entity)->second;
				for (const auto& archetypeChunkList : chunkListLUT)
				{
					if (archetype == archetypeChunkList.first)
					{
						const ChunkList& chunkList = archetypeChunkList.second;
						return reinterpret_cast<Component*>(chunkList.AddressOf(entity, componentID));
					}
				}
			}

			return nullptr;
		}

	private:
		static inline std::unique_ptr<ComponentArchive> instance;
		static inline std::once_flag instanceCreationOnceFlag;
		static inline std::once_flag instanceDestructionOnceFlag;
		std::unordered_map<ComponentID, DynamicComponentData> dynamicComponentDataLUT;
		std::unordered_map<Entity, Archetype> archetypeLUT;
		std::vector<std::pair<Archetype, ChunkList>> chunkListLUT;

	};
}

#define COMPONENT_TYPE_HASH(x) sy::utils::ELFHash(#x)

#define DeclareComponent(ComponentType) \
struct ComponentType##Registeration \
{ \
	ComponentType##Registeration() \
	{ \
	auto& archive = sy::ComponentArchive::Instance(); \
	archive.Archive<ComponentType>(); \
	}	\
	private: \
		static ComponentType##Registeration registeration; \
}; \
template <> \
constexpr sy::ComponentID sy::QueryComponentID<ComponentType>() \
{	\
	constexpr uint32_t genID = COMPONENT_TYPE_HASH(ComponentType); \
	static_assert(genID != 0 && "Generated Component ID is not valid."); \
	return static_cast<sy::ComponentID>(genID);	\
}\

#define DefineComponent(ComponentType) ComponentType##Registeration ComponentType##Registeration::registeration;