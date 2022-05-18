#include "ECS.h"
#include <cassert>
#include <array>
using namespace sy;

struct Visible
{
	double ClipDistance = 0.0;
	float VisibleDistance = 0.0f;
	float A = 0.0f;
	float B = 0.0f;
};

struct Hittable
{
	float HitDistance = 0;
	uint32_t HitCount = 0;
	float t = 0;
};

struct Invisible
{
	double Duration = 0.0;
};

DeclareComponent(Visible);
DefineComponent(Visible);

DeclareComponent(Hittable);
DefineComponent(Hittable);

DeclareComponent(Invisible);
DefineComponent(Invisible);

int main()
{
	auto& componentArchive = ComponentArchive::Get();

	auto e0 = GenerateEntity();
	componentArchive.AttachTo<Visible>(e0);

	Visible& e0Visible_0 = (*componentArchive.Retrieve<Visible>(e0));
	e0Visible_0.VisibleDistance = 1000.0f;
	e0Visible_0.B = 5022.0f;
	std::cout << "E0 V0: " << &e0Visible_0 << std::endl;

	auto e1 = GenerateEntity();
	componentArchive.AttachTo<Visible>(e1);
	Visible& e1Visible_0 = (*componentArchive.Retrieve<Visible>(e1));
	e1Visible_0.B = 9999.0f;
	std::cout << "E1 V0: " << &e1Visible_0 << std::endl;

	/*********************************/

	componentArchive.AttachTo<Hittable>(e0);
	Visible& e0Visible_1 = (*componentArchive.Retrieve<Visible>(e0));
	std::cout << "E0 V1: " << &e0Visible_1 << std::endl;
	Visible& e1Visible_1 = (*componentArchive.Retrieve<Visible>(e1));
	std::cout << "E1 V1: " << &e1Visible_1 << std::endl;
	Hittable& e0Hittable = (*componentArchive.Retrieve<Hittable>(e0));
	e0Hittable.t = 3.0f;
	componentArchive.AttachTo<Hittable>(e1);
	componentArchive.AttachTo<Invisible>(e0);
	componentArchive.DetachFrom<Visible>(e1);

	const Visible& e0Visible_2 = (*componentArchive.Retrieve<Visible>(e0));
	const Hittable& e0Hittable_1 = (*componentArchive.Retrieve<Hittable>(e0));
	const Invisible& e0Invisible_0 = (*componentArchive.Retrieve<Invisible>(e0));

	return 0;
}