#pragma once
#include <cstdint>
#include <optional>
#include <utility>
#include <random>
#include <thread>
#include <vector>
#include <unordered_map>

namespace sy::ecs
{
	using Entity = uint64_t;
	constexpr Entity INVALID_ENTITY_HANDLE = 0;

	inline Entity GenerateEntity()
	{
		static thread_local std::mt19937_64 generator(
			std::hash<std::thread::id>{}(std::this_thread::get_id()));

		std::uniform_int_distribution<Entity> dist(std::numeric_limits<Entity>::min()+1, std::numeric_limits<Entity>::max());
		return dist(generator);
	}

	template <typename Component>
	class ComponentLUT
	{
	public:
		using RefWrapper = std::reference_wrapper<Component>;
		using ConstRefWrapper = std::reference_wrapper<const Component>;

	public:
		explicit ComponentLUT(size_t reservedSize = 0)
		{
			components.reserve(reservedSize);
			entities.reserve(reservedSize);
			lut.reserve(reservedSize);
		}

		inline bool Contains(Entity entity) const
		{
			return (lut.find(entity) != lut.end());
		}

		std::optional<RefWrapper> Create(Entity entity)
		{
			if (entity != INVALID_ENTITY_HANDLE)
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

		std::optional<ConstRefWrapper> GetComponent(Entity entity) const
		{
			bool bHasRelatedComponent = Contains(entity);
			if (bHasRelatedComponent)
			{
				size_t targetComponentIdx = lut[entity];
				return components[targetComponentIdx];
			}

			return std::nullopt;
		}

		std::optional<RefWrapper> GetComponent(Entity entity)
		{
			bool bHasRelatedComponent = Contains(entity);
			if (bHasRelatedComponent)
			{
				size_t targetComponentIdx = lut[entity];
				return components[targetComponentIdx];
			}

			return std::nullopt;
		}

		inline void Clear()
		{
			components.clear();
			entities.clear();
			lut.clear();
		}

	private:
		std::vector<Component> components;
		std::vector<Entity> entities;
		std::unordered_map<Entity, size_t> lut;

	};
}