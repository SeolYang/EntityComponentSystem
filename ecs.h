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

namespace sy::ecs
{
	using Entity = uint64_t;
	constexpr Entity INVALID_ENTITY_HANDLE = 0;
	constexpr size_t DEFAULT_COMPONENT_POOL_SIZE = 16;

	constexpr bool USE_RANDOM_NUM_FOR_ENTITY_HANDLE = false;

	inline Entity GenerateEntity()
	{
		if (USE_RANDOM_NUM_FOR_ENTITY_HANDLE)
		{
			static thread_local std::mt19937_64 generator(
				std::hash<std::thread::id>{}(std::this_thread::get_id()));

			std::uniform_int_distribution<Entity> dist(std::numeric_limits<Entity>::min() + 1, std::numeric_limits<Entity>::max());
			return dist(generator);
		}

		static std::atomic<Entity> handle = 1;
		return handle++;
	}

	template <typename Component>
	class ComponentPoolBase
	{
	public:
		using RefWrapper = std::reference_wrapper<Component>;
		using ConstRefWrapper = std::reference_wrapper<const Component>;
		using ComponentRetType = std::optional<RefWrapper>;
		using ConstComponentRetType = std::optional<ConstRefWrapper>;

	public:
		explicit ComponentPoolBase(size_t reservedSize = DEFAULT_COMPONENT_POOL_SIZE)
		{
			components.reserve(reservedSize);
			entities.reserve(reservedSize);
			lut.reserve(reservedSize);
		}

		Component& operator[](size_t idx)
		{
			return components[idx];
		}

		const Component& operator[](size_t idx) const
		{
			return components[idx];
		}

		inline bool Contains(Entity entity) const
		{
			return (lut.find(entity) != lut.end());
		}

		ComponentRetType Create(Entity entity)
		{
			bool bIsValidEntityHandle = entity != INVALID_ENTITY_HANDLE;
			if (bIsValidEntityHandle)
			{
				bool bIsDuplicatedEntity = Contains(entity);
				if (!bIsDuplicatedEntity)
				{
					lut[entity] = components.size();
					components.emplace_back();
					entities.push_back(entity);

					return (components.back());
				}
			}

			return std::nullopt;
		}

		void Remove(Entity entity)
		{
			bool bIsValidEntityHandle = entity != INVALID_ENTITY_HANDLE;
			if (bIsValidEntityHandle)
			{
				auto lutItr = lut.find(entity);
				if (lutItr != lut.end())
				{
					size_t targetComponentIdx = lutItr->second;
					
					bool bIsLatestComponent = targetComponentIdx == (components.size() - 1);
					if (!bIsLatestComponent)
					{
						components[targetComponentIdx] = std::move(components.back());
						entities[targetComponentIdx] = entities.back();
						lut[entities[targetComponentIdx]] = targetComponentIdx;
					}

					components.pop_back();
					entities.pop_back();
					lut.erase(lutItr);
				}
			}
		}

		ConstComponentRetType GetComponent(Entity entity) const
		{
			bool bIsValidEntityHandle = entity != INVALID_ENTITY_HANDLE;
			if (bIsValidEntityHandle)
			{
				bool bHasRelatedComponent = Contains(entity);
				if (bHasRelatedComponent)
				{
					size_t targetComponentIdx = lut[entity];
					return ConstComponentRetType(components[targetComponentIdx]);
				}
			}

			return std::nullopt;
		}

		ComponentRetType GetComponent(Entity entity)
		{
			bool bIsValidEntityHandle = entity != INVALID_ENTITY_HANDLE;
			if (bIsValidEntityHandle)
			{
				bool bHasRelatedComponent = Contains(entity);
				if (bHasRelatedComponent)
				{
					size_t targetComponentIdx = lut[entity];
					return components[targetComponentIdx];
				}
			}

			return std::nullopt;
		}

		const std::vector<Component>& GetComponents() const
		{
			return components;
		}

		std::vector<Component>& GetComponents()
		{
			return components;
		}

		std::optional<Entity> GetEntity(size_t componentIdx) const
		{
			return (componentIdx < entities.size()) ?
				std::optional<Entity>(entities[componentIdx]) :
				std::nullopt;
		}

		size_t Size() const
		{
			return components.size();
		}

		inline void Clear()
		{
			components.clear();
			entities.clear();
			lut.clear();
		}

		inline bool CheckRelationBetween(Entity entity, const Component& component) const
		{
			if (!components.empty())
			{
				if (entity != INVALID_ENTITY_HANDLE)
				{
					auto lutItr = lut.find(entity);
					if (lutItr != lut.end())
					{
						return &components[lutItr->second] == &component;
					}
				}

			}

			return false;
		}

		inline bool CheckValidationOf(const Component& component) const
		{
			auto foundItr = std::find_if(components.begin(), components.end(), [&](const Component& validComponent)
				{
					return (&validComponent == &component);
				});

			return foundItr != components.end();
		}

	protected:
		std::vector<Component> components;
		/* Entities[idx] == Entities[LUT[Entities[idx]]] **/
		std::vector<Entity> entities; 
		std::unordered_map<Entity, size_t> lut;

	};

	template <typename Component>
	class ComponentPool : public ComponentPoolBase<Component>
	{
	public:
		explicit ComponentPool(size_t reservedSize = DEFAULT_COMPONENT_POOL_SIZE) :
			ComponentPoolBase<Component>(reservedSize)
		{
		}
	};
}