#include "ECS.h"
#include <cassert>
#include <array>
#include <random>
#include <chrono>
using namespace sy;

#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#ifdef _DEBUG
#define new new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#endif

struct Visible : Component
{
	Visible()
	{
		++Alloc;
		//std::cout << "Visible Constructor\t" << this << std::endl;
	}

	virtual ~Visible()
	{
		++Dealloc;
		//std::cout << "Visible Destructor\t" << this << std::endl;
	}

	double ClipDistance = 2022.0519;
	float VisibleDistance = 1234.4321f;
	uint64_t A = 400;
	uint64_t B = 0xffffffff;

	inline static size_t Alloc = 0;
	inline static size_t Dealloc = 0;
};

struct Hittable : Component
{
	float HitDistance = 1000.0f;
	uint32_t HitCount = 3;
	float t = 3.141592f;
};

struct Invisible : Component
{
	uint64_t Duration = 186;
};

DeclareComponent(Visible);
DefineComponent(Visible);

DeclareComponent(Hittable);
DefineComponent(Hittable);

DeclareComponent(Invisible);
DefineComponent(Invisible);

#define TEST_COUNT 1000000

int main()
{
	std::this_thread::sleep_for(std::chrono::seconds(3));
	{
		auto& componentArchive = ComponentArchive::Instance();

		Entity e0 = GenerateEntity();
		Visible* visible = componentArchive.Attach<Visible>(e0);
		assert(visible != nullptr);
		assert(visible == componentArchive.Get<Visible>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);

		// 'visible' pointer will be expired at here
		Hittable* hittable = componentArchive.Attach<Hittable>(e0);
		assert(hittable != nullptr);
		assert(hittable == componentArchive.Get<Hittable>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);
		assert(componentArchive.Attach<Hittable>(e0) == nullptr);

		// Check validation of component value #1
		Visible referenceVisible;
		Hittable referenceHittable;
		visible = componentArchive.Get<Visible>(e0);
		assert(visible != nullptr);
		assert(visible->A == referenceVisible.A);
		assert(visible->B == referenceVisible.B);
		assert(visible->VisibleDistance == referenceVisible.VisibleDistance);
		assert(visible->ClipDistance == referenceVisible.ClipDistance);
		assert(hittable->HitCount == referenceHittable.HitCount);
		assert(hittable->HitDistance == referenceHittable.HitDistance);
		assert(hittable->t == referenceHittable.t);

		referenceVisible.VisibleDistance = 2525.2525f;
		visible->VisibleDistance = 2525.2525f;
		referenceHittable.HitCount = 33333333;
		hittable->HitCount = 33333333;

		// 'visible' pointer and 'hittable' pointer will be expired at here 
		Invisible* invisible = componentArchive.Attach<Invisible>(e0);
		assert(invisible != nullptr);
		assert(invisible == componentArchive.Get<Invisible>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);
		assert(componentArchive.Attach<Hittable>(e0) == nullptr);
		assert(componentArchive.Attach<Invisible>(e0) == nullptr);

		// Check validation of component value #2
		Invisible referenceInvisible;
		visible = componentArchive.Get<Visible>(e0);
		hittable = componentArchive.Get<Hittable>(e0);
		assert(visible != nullptr);
		assert(visible->A == referenceVisible.A);
		assert(visible->B == referenceVisible.B);
		assert(visible->VisibleDistance == referenceVisible.VisibleDistance);
		assert(visible->ClipDistance == referenceVisible.ClipDistance);
		assert(hittable->HitCount == referenceHittable.HitCount);
		assert(hittable->HitDistance == referenceHittable.HitDistance);
		assert(hittable->t == referenceHittable.t);
		assert(invisible->Duration == referenceInvisible.Duration);

		// Interation test
		std::vector<Entity> entities;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution dist(0, 99);

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

			Visible* visiblePtr = componentArchive.Attach<Visible>(newEntity);
			visiblePtr->A = count + 0xffffff;
			visiblePtr->B = count + 0xf0f0f0;
			visiblePtr->ClipDistance = 10000.5555f;

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
			Visible& vRef = (*componentArchive.Get<Visible>(entities[count]));
			bool condition0 = vRef.A == (count + 0xffffff);
			bool condition1 = vRef.B == (count + 0xf0f0f0);
			bool condition2 = vRef.ClipDistance == 10000.5555f;

			assert(condition0);
			assert(condition1);
			assert(condition2);
		}
		end = std::chrono::steady_clock::now();
		std::cout << "Validation: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() << " ns" << std::endl;

		ComponentArchive::DestroyInstance();
	}

	std::cout << "Num of Alloc(Visible Component) : " << Visible::Alloc << std::endl;
	assert(Visible::Alloc == Visible::Dealloc);
	_CrtDumpMemoryLeaks();
	return 0;
}