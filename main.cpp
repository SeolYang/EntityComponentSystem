#include "ECS.h"
#include <cassert>
#include <array>
#include <random>
#include <chrono>

struct Visible
{
	double ClipDistance = 0.0;
	float VisibleDistance = 0.0f;
	uint64_t A = 0;
	uint64_t B = 0;
};

struct Hittable
{
	float HitDistance = 0;
	uint32_t HitCount = 0;
	float t = 0;
};

struct Invisible
{
	uint64_t Duration = 0;
};

DeclareComponent(Visible);
DefineComponent(Visible);

DeclareComponent(Hittable);
DefineComponent(Hittable);

DeclareComponent(Invisible);
DefineComponent(Invisible);

#define TEST_COUNT 500000

int main()
{
	using namespace sy;
	auto& componentArchive = ComponentArchive::Get();

	auto e0 = GenerateEntity();
	componentArchive.Attach<Visible>(e0);

	Visible& e0Visible_0 = *(componentArchive.Reference<Visible>(e0, Visible{
		.VisibleDistance = 1000.0f,
		.B = 5022
		}));
	std::cout << "E0 V0: " << &e0Visible_0 << std::endl;

	auto e1 = GenerateEntity();
	componentArchive.Attach<Visible>(e1);
	Visible& e1Visible_0 = *(componentArchive.Reference<Visible>(e1));
	e1Visible_0.B = 9999;
	std::cout << "E1 V0: " << &e1Visible_0 << std::endl;

	/*********************************/

	componentArchive.Attach<Hittable>(e0);
	Visible& e0Visible_1 = (*componentArchive.Reference<Visible>(e0));
	Visible& e1Visible_1 = (*componentArchive.Reference<Visible>(e1));
	Hittable& e0Hittable = (*componentArchive.Reference<Hittable>(e0));
	e0Hittable.t = 3.0f;
	componentArchive.Attach<Hittable>(e1);
	componentArchive.Attach<Invisible>(e0);

	std::vector<Entity> entities;
	entities.emplace_back(e0);
	entities.emplace_back(e1);

	auto filtered = FilterAll<Hittable, Visible>(entities);
	filtered = FilterAll<Hittable>(entities);
	filtered = FilterAll<Visible>(entities);
	filtered = FilterAll<Invisible>(entities);
	filtered = FilterAll<Invisible, Visible, Hittable>(entities);

	componentArchive.Detach<Visible>(e1);

	filtered = FilterAll<Visible>(entities);

	const Visible& e0Visible_2 = (*componentArchive.Reference<Visible>(e0));
	const Hittable& e0Hittable_1 = (*componentArchive.Reference<Hittable>(e0));
	const Invisible& e0Invisible_0 = (*componentArchive.Reference<Invisible>(e0));

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution dist(0, 99);
	entities.clear();

	auto begin = std::chrono::steady_clock::now();
	for (size_t count = 0; count < TEST_COUNT; ++count)
	{
		Entity newEntity = GenerateEntity();
		entities.emplace_back(newEntity);
		auto randomNumber = dist(gen);
		switch (randomNumber % 4)
		{
		default:
			break;
		case 2:
			componentArchive.Attach<Hittable>(newEntity);
			break;

		case 3:
			componentArchive.Attach<Invisible>(newEntity);
			break;
		}

		componentArchive.Attach<Visible>(newEntity);
		Visible& vRef = (*componentArchive.Reference<Visible>(newEntity,
			Visible{
				.A = count + 0xffffff,
				.B = (count + 0xf0f0f0)
			}));

		randomNumber = dist(gen);
		switch (randomNumber % 2)
		{
		default:
		case 0:
			componentArchive.Attach<Invisible>(newEntity);
			break;
		case 1:
			componentArchive.Attach<Hittable>(newEntity);
			break;
		}

		randomNumber = dist(gen);
		switch (randomNumber % 8)
		{
		default:
		case 4:
			componentArchive.Attach<Invisible>(newEntity);
			break;
		case 6:
			componentArchive.Attach<Hittable>(newEntity);
			break;
		}
	}
	auto end = std::chrono::steady_clock::now();
	std::cout << "Generation: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;

	begin = std::chrono::steady_clock::now();
	for (size_t count = 0; count < TEST_COUNT; ++count)
	{
		Visible& vRef = (*componentArchive.Reference<Visible>(entities[count]));
		assert(vRef.A == (count + 0xffffff));
		assert(vRef.B == (count + 0xf0f0f0));
	}
	end = std::chrono::steady_clock::now();
	std::cout << "Validation: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;

	return 0;
}