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
		size_t Size = 0;
		size_t Alignment = 1;

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

	// https://forum.unity.com/threads/is-it-guaranteed-that-random-access-within-a-16kb-chunk-will-not-cause-cache-miss.709940/
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
		using PoolType = std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>>;
	public:
		Chunk(const size_t sizeOfData, const size_t chunkSize = DEFAULT_CHUNK_SIZE, const size_t chunkAlignment = DEFAULT_CHUNK_ALIGNMENT) :
			mem(_aligned_malloc(chunkSize, chunkAlignment)),
			sizeOfData(sizeOfData),
			allocationPool({}),
			maxNumOfAllocations(DEFAULT_CHUNK_SIZE / sizeOfData),
			sizeOfChunk(chunkSize),
			alignmentOfChunk(chunkAlignment)
		{
			for (size_t allocationIndex = 0; allocationIndex < MaxNumOfAllocations(); ++allocationIndex)
			{
				allocationPool.push(allocationIndex);
			}
		}

		Chunk(Chunk&& rhs) noexcept :
			mem(std::exchange(rhs.mem, nullptr)),
			allocationPool(std::move(rhs.allocationPool)),
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

		Chunk(const Chunk&) = delete;
		Chunk& operator=(const Chunk&) = delete;
		Chunk& operator=(Chunk&& rhs) noexcept
		{
			mem = std::exchange(rhs.mem, nullptr);
			allocationPool = std::move(rhs.allocationPool);
			sizeOfData = std::exchange(rhs.sizeOfData, 0);
			maxNumOfAllocations = std::exchange(rhs.maxNumOfAllocations, 0);
			sizeOfChunk = std::exchange(rhs.sizeOfChunk, 0);
			alignmentOfChunk = std::exchange(rhs.alignmentOfChunk, 0);
			return (*this);
		}

		/** Return index of allocation */
		size_t Allocate()
		{
			assert(!IsFull());
			size_t alloc = allocationPool.top();
			allocationPool.pop();
			return alloc;
		}

		/** Return moved allocation, only if return 0 when numOfAllocation is 1 */
		void Deallocate(size_t at)
		{
			assert(at < MaxNumOfAllocations());
			//assert((std::find(allocationPool.cbegin(), allocationPool.cend(), at) == allocationPool.cend()));
			//allocationPool.push_back(at);
			allocationPool.push(at);
		}

		void* AddressOf(size_t at) const
		{
			assert(NumOfAllocations() > 0);
			return (void*)((uintptr_t)mem + (at * sizeOfData));
		}

		inline bool IsEmpty() const { return allocationPool.size() == MaxNumOfAllocations(); }
		inline bool IsFull() const { return allocationPool.empty(); }
		inline size_t MaxNumOfAllocations() const { return maxNumOfAllocations - 1; }
		inline size_t NumOfAllocations() const { return MaxNumOfAllocations() - allocationPool.size(); }
		inline size_t SizeOfChunk() const { return sizeOfChunk; }
		inline size_t AlignmentOfChunk() const { return alignmentOfChunk; }

	private:
		void* mem;
		PoolType allocationPool;
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

			inline bool IsFailedToAllocate() const { return (ChunkIndex == size_t(-1)) || (AllocationIndexOfEntity == size_t(-1)); }
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
			sizeOfData = rhs.sizeOfData;
			return (*this);
		}

		/** It doesn't call anyof constructor. */
		Allocation Create()
		{
			size_t freeChunkIndex = FreeChunkIndex();
			bool bDoesNotFoundFreeChunk = freeChunkIndex >= chunks.size();
			if (bDoesNotFoundFreeChunk)
			{
				chunks.emplace_back(sizeOfData);
			}

			Chunk& chunk = chunks[freeChunkIndex];
			size_t allocIndex = chunk.Allocate();

			return Allocation{
				.ChunkIndex = freeChunkIndex,
				.AllocationIndexOfEntity = allocIndex
			};
		}

		/** It doesn't call anyof destructor. */
		void Destroy(const Allocation allocation)
		{
			assert(!allocation.IsFailedToAllocate());
			assert(allocation.ChunkIndex < chunks.size());
			chunks[allocation.ChunkIndex].Deallocate(allocation.AllocationIndexOfEntity);
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

		void* AddressOf(const Allocation allocation) const
		{
			const bool bIsValidChunkIndex = allocation.ChunkIndex < chunks.size();
			assert(bIsValidChunkIndex);

			if (bIsValidChunkIndex)
			{
				return chunks[allocation.ChunkIndex].AddressOf(allocation.AllocationIndexOfEntity);
			}

			return nullptr;
		}

		void* AddressOf(const Allocation allocation, const ComponentID componentID) const
		{
			const bool bIsValidChunkIndex = allocation.ChunkIndex < chunks.size();
			assert(bIsValidChunkIndex);

			if (Support(componentID))
			{
				const auto componentAllocInfo = AllocationInfoOfComponent(componentID);
				void* entityAddress = chunks[allocation.ChunkIndex].AddressOf(allocation.AllocationIndexOfEntity);

				return ComponentRange::ComponentAddress(entityAddress, componentAllocInfo.Range);
			}

			return nullptr;
		}

		bool IsChunkFull(const size_t chunkIndex) const
		{
			assert(chunkIndex < chunks.size());
			return chunks[chunkIndex].IsFull();
		}

		inline size_t FreeChunkIndex() const
		{
			size_t freeChunkIndex = 0;
			for (; freeChunkIndex < chunks.size(); ++freeChunkIndex)
			{
				if (!chunks[freeChunkIndex].IsFull())
				{
					break;
				}
			}

			return freeChunkIndex;
		}

		size_t ShrinkToFit()
		{
			// Erase-Remove idiom
			auto reduced = std::erase_if(chunks, [](Chunk& chunk) {
				return chunk.IsEmpty();
				});

			chunks.shrink_to_fit();
			return reduced;
		}

		/** Just memory data copy, it never call any constructor or destructor. */
		static void MoveData(ChunkList& srcChunkList, const Allocation srcAllocation, ChunkList& destChunkList, const Allocation destAllocation)
		{
			bool bIsValid = !srcAllocation.IsFailedToAllocate() && !destAllocation.IsFailedToAllocate();
			assert(bIsValid);

			bIsValid = bIsValid && (srcAllocation.ChunkIndex < srcChunkList.chunks.size() && destAllocation.ChunkIndex < destChunkList.chunks.size());
			assert(bIsValid);

			if (bIsValid)
			{
				void* srcAddress = srcChunkList.AddressOf(srcAllocation);
				void* destAddress = destChunkList.AddressOf(destAllocation);
				const auto& srcComponentAllocInfos = srcChunkList.componentAllocInfos;
				const auto& destComponentAllocInfos = destChunkList.componentAllocInfos;
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

				srcChunkList.Destroy(srcAllocation);
			}
		}

	private:
		std::vector<Chunk> chunks;
		std::vector<ComponentAllocationInfo> componentAllocInfos;
		size_t sizeOfData;

	};

	using ArchetypeType = std::set<ComponentID>;
	class ComponentArchive
	{
	public:
		struct DynamicComponentData
		{
			ComponentInfo Info;
			std::function<void(void*)> DefaultConstructor;
			std::function<void(void*)> Destructor;
		};

		struct ArchetypeData
		{
			ArchetypeType Archetype;
			ChunkList::Allocation Allocation;
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
				return utils::HasKey(foundArchetype.Archetype, componentID);
			}

			return false;
		}

		template <typename T>
		bool Contains(const Entity entity) const
		{
			return Contains(entity, QueryComponentID<T>());
		}

		bool HasArchetypeChunkList(const ArchetypeType& archetype) const
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
				const auto& lhsArchetype= lhsItr->second.Archetype;
				const auto& rhsArchetype = rhsItr->second.Archetype;
				return lhsArchetype == rhsArchetype;
			}

			// If both itrerator is entityLUT.end(), it means those are empty and at same time equal archetype.
			return lhsItr == rhsItr;
		}

		ArchetypeType QueryArchetype(const Entity entity) const
		{
			if (utils::HasKey(archetypeLUT, entity))
			{
				return archetypeLUT.find(entity)->second.Archetype;
			}

			return ArchetypeType();
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

				ArchetypeData& archetypeData = archetypeLUT[entity];
				ArchetypeType& archetype = archetypeData.Archetype;
				ArchetypeType oldArchetype = archetype;
				archetype.insert(componentID);

				size_t chunkList = FindOrCreateChunkList(archetype);
				ChunkList::Allocation newAllocation = ReferenceChunkList(chunkList).Create();
				if (!newAllocation.IsFailedToAllocate() && !oldArchetype.empty())
				{
					size_t oldChunkList = FindOrCreateChunkList(oldArchetype);
					ChunkList::MoveData(ReferenceChunkList(oldChunkList), archetypeLUT[entity].Allocation, ReferenceChunkList(chunkList), newAllocation);
				}
				archetypeData.Allocation = newAllocation;

				result = reinterpret_cast<Component*>(ReferenceChunkList(chunkList).AddressOf(newAllocation, componentID));

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
				ArchetypeData& archetypeData = archetypeLUT[entity];
				ArchetypeType& archetype = archetypeData.Archetype;
				ArchetypeType oldArchetype = archetype;
				archetype.erase(componentID);

				size_t oldChunkList = FindOrCreateChunkList(oldArchetype);
				ChunkList::Allocation oldAllocation = archetypeData.Allocation;
				void* detachComponentPtr = ReferenceChunkList(oldChunkList).AddressOf(oldAllocation, componentID);
				const DynamicComponentData& dynamicComponentData = dynamicComponentDataLUT[componentID];
				dynamicComponentData.Destructor(detachComponentPtr);

				if (!archetype.empty())
				{
					size_t newChunkList = FindOrCreateChunkList(archetype);
					ChunkList::Allocation newAllocation = ReferenceChunkList(newChunkList).Create();
					ChunkList::MoveData(
						ReferenceChunkList(oldChunkList), oldAllocation,
						ReferenceChunkList(newChunkList), newAllocation);
				}
				else
				{
					ReferenceChunkList(oldChunkList).Destroy(oldAllocation);
					archetypeData.Allocation = ChunkList::Allocation();
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
				const auto& archetypeData = archetypeLUT[entity];
				const auto& archetype = archetypeData.Archetype;
				if (!archetype.empty())
				{
					size_t chunkList = FindOrCreateChunkList(archetype);
					ChunkList::Allocation oldAllocation = archetypeData.Allocation;
					for (ComponentID componentID : archetype)
					{
						void* detachComponentPtr = ReferenceChunkList(chunkList).AddressOf(oldAllocation, componentID);
						const DynamicComponentData& dynamicComponentData = dynamicComponentDataLUT[componentID];
						dynamicComponentData.Destructor(detachComponentPtr);
					}

					ReferenceChunkList(chunkList).Destroy(oldAllocation);
				}

				archetypeLUT.erase(entity);
			}
		}

		/** 
		* Trying to degragment 'entire' chunk list and chunks(except not fragmented chunk which is full) 
		* It maybe will nullyfies any references, pointers that acquired from Attach and Get methods.
		*/
		void Defragmentation()
		{
			for (auto& archetypeLUTPair : archetypeLUT)
			{
				const Entity entity = archetypeLUTPair.first;
				auto& archetypeData = archetypeLUTPair.second;
				if (!archetypeData.Archetype.empty() && !archetypeData.Allocation.IsFailedToAllocate())
				{
					size_t chunkListIdx = FindChunkList(archetypeData.Archetype);
					if (chunkListIdx != chunkListLUT.size())
					{
						ChunkList& chunkListRef = ReferenceChunkList(chunkListIdx);
						size_t freeChunkIndex = chunkListRef.FreeChunkIndex();
						if (freeChunkIndex <= archetypeData.Allocation.ChunkIndex)
						{
							ChunkList::Allocation newAllocation = chunkListRef.Create();
							ChunkList::MoveData(
								chunkListRef, archetypeData.Allocation,
								chunkListRef, newAllocation);

							archetypeData.Allocation = newAllocation;
						}
					}
				}
			}
		}

		size_t ShrinkToFit(bool bPerformShrinkAfterDefrag = true)
		{
			if (bPerformShrinkAfterDefrag)
			{
				Defragmentation();
			}

			size_t reduced = 0;
			for (auto& chunkListPair : chunkListLUT)
			{
				reduced += chunkListPair.second.ShrinkToFit();
			}

			return reduced;
		}

	private:
		ComponentArchive() = default;

		size_t FindOrCreateChunkList(const ArchetypeType& archetype)
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

		size_t FindChunkList(const ArchetypeType& archetype)
		{
			size_t idx = 0;
			for (; idx < chunkListLUT.size(); ++idx)
			{
				if (chunkListLUT[idx].first == archetype)
				{
					return idx;
				}
			}

			return idx;
		}

		/**
		* To prevent vector reallocations, always ref chunk list through this method.
		* Do not reference ChunkList directly when exist possibility to chunkListLUT get modified.
		*/
		inline ChunkList& ReferenceChunkList(size_t idx)
		{
			return chunkListLUT[idx].second;
		}

		std::vector<ComponentInfo> RetrieveComponentInfosFromArchetype(const ArchetypeType& archetype) const
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
				const auto& archetypeData = archetypeLUT.find(entity)->second;
				const auto& archetype = archetypeData.Archetype;
				const ChunkList::Allocation& allocation = archetypeData.Allocation;
				for (const auto& archetypeChunkList : chunkListLUT)
				{
					if (archetype == archetypeChunkList.first)
					{
						const ChunkList& chunkList = archetypeChunkList.second;
						return reinterpret_cast<Component*>(chunkList.AddressOf(allocation, componentID));
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
		std::unordered_map<Entity, ArchetypeData> archetypeLUT;
		std::vector<std::pair<ArchetypeType, ChunkList>> chunkListLUT;

	};

	namespace Filter
	{
		static std::vector<Entity> All(const ComponentArchive& archive, const std::vector<Entity>& entities, const ArchetypeType& filter)
		{
			std::vector<Entity> result;
			result.reserve((entities.size() / 2) + 2); /** Conservative reserve */

			for (const Entity entity : entities)
			{
				const ArchetypeType& entityArchetype = archive.QueryArchetype(entity);
				if (!entityArchetype.empty() &&
					std::includes(
						entityArchetype.cbegin(), entityArchetype.cend(),
						filter.cbegin(), filter.cend()))
				{
					result.push_back(entity);
				}
			}

			result.shrink_to_fit();
			return result;
		}

		static std::vector<Entity> Any(const ComponentArchive& archive, const std::vector<Entity>& entities, const ArchetypeType& filter)
		{
			assert(!filter.empty() && "Filter Archetype must contains at least one element.");
			std::vector<Entity> result;
			result.reserve((entities.size() / 2) + 2); /** Conservative reserve */

			for (const Entity entity : entities)
			{
				const ArchetypeType& entityArchetype = archive.QueryArchetype(entity);
				if (!entityArchetype.empty())
				{
					ArchetypeType intersection = {};
					std::set_intersection(
						filter.begin(), filter.end(),
						entityArchetype.begin(), entityArchetype.end(),
						std::inserter(intersection, intersection.end()));

					if (!intersection.empty())
					{
						result.push_back(entity);
					}
				}
			}

			result.shrink_to_fit();
			return result;
		}

		static std::vector<Entity> None(const ComponentArchive& archive, const std::vector<Entity>& entities, const ArchetypeType& filter)
		{
			assert(!filter.empty() && "Filter Archetype must contains at least one element.");
			std::vector<Entity> result;
			result.reserve((entities.size() / 2) + 2); /** Conservative reserve */

			for (const Entity entity : entities)
			{
				const ArchetypeType& entityArchetype = archive.QueryArchetype(entity);
				if (!entityArchetype.empty())
				{
					ArchetypeType intersection = {};
					std::set_intersection(
						filter.begin(), filter.end(),
						entityArchetype.begin(), entityArchetype.end(),
						std::inserter(intersection, intersection.end()));

					if (intersection.empty())
					{
						result.push_back(entity);
					}
				}
			}

			result.shrink_to_fit();
			return result;
		}

		template <ComponentType... Ts>
		/** Entities which has all of given component types. */
		std::vector<Entity> All(const ComponentArchive& archive, const std::vector<Entity>& entities)
		{
			const ArchetypeType filterArchetype = { QueryComponentID<Ts>()... };
			return All(archive, entities, filterArchetype);
		}

		template <ComponentType... Ts>
		/** Entities which has any of given component types. */
		std::vector<Entity> Any(const ComponentArchive& archive, const std::vector<Entity>& entities)
		{
			const ArchetypeType filterArchetype = { QueryComponentID<Ts>()... };
			return Any(archive, entities, filterArchetype);
		}

		template <ComponentType... Ts>
		/** Entities which has none of given component types. */
		std::vector<Entity> None(const ComponentArchive& archive, const std::vector<Entity>& entities)
		{
			const ArchetypeType filterArchetype = { QueryComponentID<Ts>()... };
			return None(archive, entities, filterArchetype);
		}
	}

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