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
	}

	virtual ~Visible()
	{
		++Dealloc;
	}

	double ClipDistance = 2022.0519;
	float VisibleDistance = 1234.4321f;
	uint64_t A = 400;
	uint64_t B = 0xffffffff;
	std::vector<std::tuple<int, int>> compund;

	inline static size_t Alloc = 0;
	inline static size_t Dealloc = 0;
};

struct Hittable : Component
{
	Hittable()
	{
		++Alloc;
	}

	virtual ~Hittable()
	{
		++Dealloc;
	}

	float HitDistance = 1000.0f;
	uint64_t HitCount = 3;
	float t = 3.141592f;

	inline static size_t Alloc = 0;
	inline static size_t Dealloc = 0;
};

struct Invisible : Component
{
	Invisible()
	{
		++Alloc;
	}

	virtual ~Invisible()
	{
		++Dealloc;
	}

	uint64_t Duration = 186;

	inline static size_t Alloc = 0;
	inline static size_t Dealloc = 0;
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
	constexpr ComponentID visibeID = QueryComponentID<Visible>();
	constexpr ComponentID hittableID = QueryComponentID<Hittable>();
	constexpr ComponentID invisibleID = QueryComponentID<Invisible>();

	size_t visibleAllocCount = 0;
	size_t hittableAllocCount = 0;
	size_t invisibleAllocCount = 0;

	std::this_thread::sleep_for(std::chrono::seconds(2));
	{
		auto& componentArchive = ComponentArchive::Instance();

		Entity e0 = GenerateEntity();
		Visible* visible = componentArchive.Attach<Visible>(e0);
		assert(visible != nullptr);
		assert(visible == componentArchive.Get<Visible>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);
		++visibleAllocCount;

		// 'visible' pointer will be expired at here
		Hittable* hittable = componentArchive.Attach<Hittable>(e0);
		assert(hittable != nullptr);
		assert(hittable == componentArchive.Get<Hittable>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);
		assert(componentArchive.Attach<Hittable>(e0) == nullptr);
		++hittableAllocCount;

		// Check validation of component value #1
		Visible referenceVisible;
		Hittable referenceHittable;
		++visibleAllocCount;
		++hittableAllocCount;
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
		++invisibleAllocCount;
		assert(invisible != nullptr);
		assert(invisible == componentArchive.Get<Invisible>(e0));
		assert(componentArchive.Attach<Visible>(e0) == nullptr);
		assert(componentArchive.Attach<Hittable>(e0) == nullptr);
		assert(componentArchive.Attach<Invisible>(e0) == nullptr);

		// Check validation of component value #2
		Invisible referenceInvisible;
		++invisibleAllocCount;
		visible = componentArchive.Get<Visible>(e0);
		hittable = componentArchive.Get<Hittable>(e0);
		assert(visible != nullptr);
		assert(hittable != nullptr);
		assert(visible->A == referenceVisible.A);
		assert(visible->B == referenceVisible.B);
		assert(visible->VisibleDistance == referenceVisible.VisibleDistance);
		assert(visible->ClipDistance == referenceVisible.ClipDistance);
		assert(hittable->HitCount == referenceHittable.HitCount);
		assert(hittable->HitDistance == referenceHittable.HitDistance);
		assert(hittable->t == referenceHittable.t);
		assert(invisible->Duration == referenceInvisible.Duration);

		componentArchive.Detach<Visible>(e0);
		hittable = componentArchive.Get<Hittable>(e0);
		invisible = componentArchive.Get<Invisible>(e0);
		visible = componentArchive.Get<Visible>(e0);
		assert(visible == nullptr);
		assert(hittable != nullptr);
		assert(invisible != nullptr);
		assert(hittable->HitCount == referenceHittable.HitCount);
		assert(hittable->HitDistance == referenceHittable.HitDistance);
		assert(hittable->t == referenceHittable.t);
		assert(invisible->Duration == referenceInvisible.Duration);

		referenceHittable = Hittable();
		referenceInvisible = Invisible();
		referenceVisible = Visible();
		++hittableAllocCount;
		++invisibleAllocCount;
		++visibleAllocCount;

		std::vector<Entity> entities;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution dist(0, 99);

		// Random Generation

		auto begin = std::chrono::steady_clock::now();
		for (size_t count = 0; count < TEST_COUNT; ++count)
		{
			Hittable* hittable = nullptr;
			Visible* visible = nullptr;
			Invisible* invisible = nullptr;
			Entity newEntity = GenerateEntity();
			entities.emplace_back(newEntity);
			auto randomNumber = dist(gen);
			switch (randomNumber % 6)
			{
			default:
				visible = componentArchive.Attach<Visible>(newEntity);
				visible->A = count + 0xffffff;
				visible->B = count + 0xf0f0f0;
				visible->ClipDistance = 10000.5555f;
				visibleAllocCount += (visible != nullptr ? 1 : 0);
			case 2:
			case 3:
				hittable = componentArchive.Attach<Hittable>(newEntity);
				if (hittable != nullptr)
				{
					++hittableAllocCount;
					hittable->HitCount = ~static_cast<uint64_t>(newEntity);
				}
				break;

			case 4:
				invisible = componentArchive.Attach<Invisible>(newEntity);
				invisibleAllocCount += (invisible != nullptr ? 1 : 0);
				break;
			}

			randomNumber = dist(gen);
			switch (randomNumber % 8)
			{
			case 0:
			case 1:
				invisible = componentArchive.Attach<Invisible>(newEntity);
				invisibleAllocCount += (invisible != nullptr ? 1 : 0);
				break;
			case 2:
				hittable = componentArchive.Attach<Hittable>(newEntity);
				if (hittable != nullptr)
				{
					++hittableAllocCount;
					hittable->HitCount = ~static_cast<uint64_t>(newEntity);
				}
				break;
			}

			randomNumber = dist(gen);
			switch (randomNumber % 16)
			{
			case 4:
				invisible = componentArchive.Attach<Invisible>(newEntity);
				invisibleAllocCount += (invisible != nullptr ? 1 : 0);
				break;
			case 6:
				hittable = componentArchive.Attach<Hittable>(newEntity);
				if (hittable != nullptr)
				{
					++hittableAllocCount;
					hittable->HitCount = ~static_cast<uint64_t>(newEntity);
				}
				break;
			}
		}
		auto end = std::chrono::steady_clock::now();
		std::cout << "Random generation takes " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;

		/******************************************************************/
		/* Linear Access & Data Validation */
		begin = std::chrono::steady_clock::now();
		for (size_t count = 0; count < TEST_COUNT; ++count)
		{
			const size_t idx = count;
			const Entity entity = entities[idx];

			Visible* visible = componentArchive.Get<Visible>(entity);
			if (visible != nullptr)
			{
				bool condition0 = visible->A == (idx + 0xffffff);
				bool condition1 = visible->B == (idx + 0xf0f0f0);
				bool condition2 = visible->ClipDistance == 10000.5555f;
				bool condition3 = visible->VisibleDistance == referenceVisible.VisibleDistance;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Visible at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			Hittable* hittable = (componentArchive.Get<Hittable>(entity));
			if (hittable != nullptr)
			{
				bool condition0 = hittable->HitCount == ~static_cast<uint64_t>(entity);
				bool condition1 = hittable->HitDistance == referenceHittable.HitDistance;
				bool condition2 = hittable->t == referenceHittable.t;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Hittable at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			Invisible* invisible = (componentArchive.Get<Invisible>(entity));
			if (invisible != nullptr)
			{
				bool condition0 = invisible->Duration == referenceInvisible.Duration;
				if (!condition0)
				{
					std::cout << "Failed to checks validation of Invisible at " << idx << std::endl;
				}

				assert(condition0);
			}
		}
		end = std::chrono::steady_clock::now();
		std::cout << "Linear Access & Validation takes " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;

		/******************************************************************/
		/* Random Access & Data Validation */
		begin = std::chrono::steady_clock::now();
		std::uniform_int_distribution<size_t> accessDist(0, entities.size()-1);
		for (size_t count = 0; count < TEST_COUNT; ++count)
		{
			const size_t idx = accessDist(gen);
			const Entity entity = entities[idx];

			Visible* visible = componentArchive.Get<Visible>(entity);
			if (visible != nullptr)
			{
				bool condition0 = visible->A == (idx + 0xffffff);
				bool condition1 = visible->B == (idx + 0xf0f0f0);
				bool condition2 = visible->ClipDistance == 10000.5555f;
				bool condition3 = visible->VisibleDistance == referenceVisible.VisibleDistance;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Visible at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			Hittable* hittable = (componentArchive.Get<Hittable>(entity));
			if (hittable != nullptr)
			{
				bool condition0 = hittable->HitCount == ~static_cast<uint64_t>(entity);
				bool condition1 = hittable->HitDistance == referenceHittable.HitDistance;
				bool condition2 = hittable->t == referenceHittable.t;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Hittable at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			Invisible* invisible = (componentArchive.Get<Invisible>(entity));
			if (invisible != nullptr)
			{
				bool condition0 = invisible->Duration == referenceInvisible.Duration;
				if (!condition0)
				{
					std::cout << "Failed to checks validation of Invisible at " << idx << std::endl;
				}

				assert(condition0);
			}
		}
		end = std::chrono::steady_clock::now();
		std::cout << "Random Access & Validation takes " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;

		std::cout << std::endl << std::endl;
		auto filteredVisible = FilterAll<Visible>(componentArchive, entities);
		std::cout << "Num of filtered Visible : " << filteredVisible.size() << std::endl;
		auto filteredHittable = FilterAll<Hittable>(componentArchive, entities);
		std::cout << "Num of filtered Hittable : " << filteredHittable.size() << std::endl;
		auto filteredInvisible = FilterAll<Invisible>(componentArchive, entities);
		std::cout << "Num of filtered Invisible : " << filteredInvisible.size() << std::endl;

		/** Attach Order Invariant */
		auto filteredVisibleHittable = FilterAll<Visible, Hittable>(componentArchive, entities);
		auto filteredHittableVisible = FilterAll<Hittable, Visible>(componentArchive, entities);
		std::cout << "Num of filtered Visible-Hittable : " << filteredVisibleHittable.size() << std::endl;
		std::cout << "Num of filtered Hittable-Visible : " << filteredHittableVisible.size() << std::endl;
		std::cout << "Attach Order Invariant : " << (filteredHittableVisible.size() == filteredVisibleHittable.size()) << std::endl;
		assert(filteredHittableVisible.size() == filteredVisibleHittable.size());

		auto filteredVisibleInvisible = FilterAll<Visible, Invisible>(componentArchive, entities);
		std::cout << "Num of filtered Visible-Invisible : " << filteredVisibleHittable.size() << std::endl;

		auto filteredHittableInvisible = FilterAll<Hittable, Invisible>(componentArchive, entities);
		std::cout << "Num of filtered Hittable-Invisible : " << filteredVisibleHittable.size() << std::endl;

		auto filteredVisInvHit = FilterAll<Hittable, Visible, Invisible>(componentArchive, entities);
		std::cout << "Num of filtered Hittable-Visible-Invisible : " << filteredVisInvHit.size() << std::endl;

		ComponentArchive::DestroyInstance();
	}

	std::cout << std::endl << std::endl;
	std::cout << "Num of actual generation (Visible Component) : " << visibleAllocCount << std::endl;
	std::cout << "Num of call constructor of (Visible Component) : " << Visible::Alloc << " (times)" << std::endl;
	std::cout << "Num of call destructor of (Visible Component) : " << Visible::Dealloc << " (times)" << std::endl;
	assert(visibleAllocCount == Visible::Alloc);
	assert(Visible::Alloc == Visible::Dealloc);

	std::cout << "Num of actual generation (Hittable Component) : " << hittableAllocCount << std::endl;
	std::cout << "Num of call constructor of (Hittable Component) : " << Hittable::Alloc << " (times)" << std::endl;
	std::cout << "Num of call destructor of (Hittable Component) : " << Hittable::Dealloc << " (times)" << std::endl;
	assert(hittableAllocCount == Hittable::Alloc);
	assert(Hittable::Alloc == Hittable::Dealloc);

	std::cout << "Num of actual generation (Invisible Component) : " << invisibleAllocCount << std::endl;
	std::cout << "Num of call constructor of (Invisible Component) : " << Invisible::Alloc << " (times)" << std::endl;
	std::cout << "Num of call destructor of (Invisible Component) : " << Invisible::Dealloc << " (times)" << std::endl;
	assert(invisibleAllocCount == Invisible::Alloc);
	assert(Invisible::Alloc == Invisible::Dealloc);

	/** Check Memory Leaks */
	_CrtDumpMemoryLeaks();
	return 0;
}