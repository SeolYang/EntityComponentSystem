#include "ecs.h"
#include "ecs_hierarchy.h"
#include <cassert>

struct TestComponent
{
	float value = 0.0f;
};

namespace PrintHierarchySystem
{
	void PrintSubtree(const sy::ecs::ComponentPool<sy::ecs::HierarchyComponent>& pool, size_t rootIdx = 0, int depth = 0)
	{
		auto entityOpt = pool.GetEntity(rootIdx);
		if (entityOpt.has_value())
		{
			const auto& rootEntity = entityOpt.value();
			for (int itr = 0; itr < depth; ++itr)
			{
				std::cout << '-';
			}
			std::cout << " ID : " << rootEntity << std::endl;

			for (size_t childIdx = rootIdx + 1; childIdx < pool.Size(); ++childIdx)
			{
				if (pool[childIdx].parentEntity == rootEntity)
				{
					PrintSubtree(pool, childIdx, depth + 1);
				}
			}
		}
	}

	void PrintHierarchy(const sy::ecs::ComponentPool<sy::ecs::HierarchyComponent>& pool)
	{
		std::cout << "ROOT" << std::endl;
		for (size_t idx = 0; idx < pool.Size(); ++idx)
		{
			if (pool[idx].parentEntity == sy::ecs::INVALID_ENTITY_HANDLE)
			{
				PrintSubtree(pool, idx, 1);
			}
		}

		std::cout << std::endl;
	}
}

int main()
{
	sy::ecs::ComponentPool<TestComponent> componentPool;
	assert(!componentPool.Create(sy::ecs::INVALID_ENTITY_HANDLE).has_value());

	auto newEntity = sy::ecs::GenerateEntity();
	auto newCompOpt = componentPool.Create(newEntity);
	assert(newCompOpt.has_value());
	assert(!componentPool.Create(newEntity).has_value());

	auto& newComp = newCompOpt.value().get();
	assert(newComp.value == 0.0f);
	newComp.value = 2.0f;

	assert(componentPool.Contains(newEntity));
	auto compOpt = componentPool.GetComponent(newEntity);
	assert(compOpt.has_value());

	auto& comp = compOpt.value().get();
	assert(comp.value == 2.0f);

	assert(componentPool.Size() == 1);

	assert(componentPool.CheckValidationOf(comp));
	assert(componentPool.CheckRelationBetween(newEntity, comp));

	componentPool.Remove(newEntity);
	assert(!componentPool.CheckValidationOf(comp));
	assert(!componentPool.CheckRelationBetween(newEntity, comp));
	/////////////////////////////////////////////
	sy::ecs::ComponentPool<sy::ecs::HierarchyComponent> hierarchyPool;

	std::vector<sy::ecs::Entity> entities;
	sy::ecs::Entity root = sy::ecs::GenerateEntity();
	hierarchyPool.Create(root);

	for (int idx = 0; idx < 6; ++idx)
	{
		entities.push_back(sy::ecs::GenerateEntity());
		hierarchyPool.Create(entities[idx]);
	}
	
	std::cout << "- Hierarchy -" << std::endl;
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(entities[4], entities[3]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(entities[0], entities[2]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(root, entities[0]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(root, entities[4]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(entities[0], entities[1]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);
	hierarchyPool.Attach(entities[3], entities[5]);
	PrintHierarchySystem::PrintHierarchy(hierarchyPool);

	// hierarchyPool.Attach(entities[3], entities[0]); => Circular Dependency

	return 0;
}