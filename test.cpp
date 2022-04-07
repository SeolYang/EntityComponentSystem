#include "ecs.h"
#include <cassert>

struct TestComponent
{
	float value = 0.0f;
};

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

	assert(componentPool.CheckValidatioBetween(newEntity, comp));

	componentPool.Remove(newEntity);
	assert(!componentPool.CheckValidationOf(comp));
	assert(!componentPool.CheckValidatioBetween(newEntity, comp));

	return 0;
}