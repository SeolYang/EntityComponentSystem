#include "ecs.h"
#include <cassert>

struct TestComponent
{
	float value = 0.0f;
};

int main()
{
	sy::ecs::ComponentLUT<TestComponent> compLUT;
	assert(!compLUT.Create(sy::ecs::INVALID_ENTITY_HANDLE).has_value());

	auto newEntity = sy::ecs::GenerateEntity();
	auto newCompOpt = compLUT.Create(newEntity);
	assert(newCompOpt.has_value());
	assert(!compLUT.Create(newEntity).has_value());

	auto& newComp = newCompOpt.value().get();
	assert(newComp.value == 0.0f);
	newComp.value = 2.0f;

	assert(compLUT.Contains(newEntity));
	auto compOpt = compLUT.GetComponent(newEntity);
	assert(compOpt.has_value());

	auto& comp = compOpt.value().get();
	assert(comp.value == 2.0f);

	return 0;
}