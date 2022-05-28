#include "ECS_MT.h"
//#include "ECS_Before_Memory_Optimizations.h"
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

struct Tag
{
};

struct Visible : Component
{
	Visible() noexcept
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
	Hittable() noexcept
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
	Invisible() noexcept
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

static std::chrono::milliseconds LinearDataValidation(const ComponentArchive& componentArchive, const std::vector<Entity> entities, const Visible& referenceVisible, const Hittable& referenceHittable, const Invisible& referenceInvisible)
{
	const auto begin = std::chrono::steady_clock::now();
	for (size_t count = 0; count < TEST_COUNT; ++count)
	{
		const size_t idx = count;
		const Entity entity = entities.at(idx);

		if (entity != INVALID_ENTITY_HANDLE)
		{
			const Visible* visible = componentArchive.Get<Visible>(entity);
			if (visible != nullptr)
			{
				const bool condition0 = visible->A == (idx + 0xffffff);
				const bool condition1 = visible->B == (idx + 0xf0f0f0);
				const bool condition2 = visible->ClipDistance == 10000.5555f;
				const bool condition3 = visible->VisibleDistance == referenceVisible.VisibleDistance;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Visible at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			const Hittable* hittable = (componentArchive.Get<Hittable>(entity));
			if (hittable != nullptr)
			{
				const bool condition0 = hittable->HitCount == ~static_cast<uint64_t>(entity);
				const bool condition1 = hittable->HitDistance == referenceHittable.HitDistance;
				const bool condition2 = hittable->t == referenceHittable.t;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Hittable at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			const Invisible* invisible = (componentArchive.Get<Invisible>(entity));
			if (invisible != nullptr)
			{
				const bool condition0 = invisible->Duration == referenceInvisible.Duration;
				if (!condition0)
				{
					std::cout << "Failed to checks validation of Invisible at " << idx << std::endl;
				}

				assert(condition0);
			}
		}
	}

	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
}

static std::chrono::milliseconds RandomDataValidation(const ComponentArchive& componentArchive, const std::vector<Entity> entities, const Visible& referenceVisible, const Hittable& referenceHittable, const Invisible& referenceInvisible)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	const std::uniform_int_distribution<size_t> accessDist(0, entities.size() - 1);

	const auto begin = std::chrono::steady_clock::now();
	for (size_t count = 0; count < TEST_COUNT; ++count)
	{
		const size_t idx = accessDist(gen);
		const Entity entity = entities.at(idx);
		if (entity != INVALID_ENTITY_HANDLE)
		{
			const Visible* visible = componentArchive.Get<Visible>(entity);
			if (visible != nullptr)
			{
				const bool condition0 = visible->A == (idx + 0xffffff);
				const bool condition1 = visible->B == (idx + 0xf0f0f0);
				const bool condition2 = visible->ClipDistance == 10000.5555f;
				const bool condition3 = visible->VisibleDistance == referenceVisible.VisibleDistance;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Visible at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			const Hittable* hittable = (componentArchive.Get<Hittable>(entity));
			if (hittable != nullptr)
			{
				const bool condition0 = hittable->HitCount == ~static_cast<uint64_t>(entity);
				const bool condition1 = hittable->HitDistance == referenceHittable.HitDistance;
				const bool condition2 = hittable->t == referenceHittable.t;

				if (!(condition0 && condition1 && condition2))
				{
					std::cout << "Failed to checks validation of Hittable at " << idx << std::endl;
				}

				assert(condition0);
				assert(condition1);
				assert(condition2);
			}

			const Invisible* invisible = (componentArchive.Get<Invisible>(entity));
			if (invisible != nullptr)
			{
				const bool condition0 = invisible->Duration == referenceInvisible.Duration;
				if (!condition0)
				{
					std::cout << "Failed to checks validation of Invisible at " << idx << std::endl;
				}

				assert(condition0);
			}
		}
	}

	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
}

int main()
{
	constexpr ComponentID visibeID = QueryComponentID<Visible>();
	constexpr ComponentID hittableID = QueryComponentID<Hittable>();
	constexpr ComponentID invisibleID = QueryComponentID<Invisible>();

	const std::string red = "\033[1;31m";
	const std::string green = "\033[1;32m";
	const std::string yellow = "\033[1;33m";
	const std::string reset = "\033[0m";

	size_t visibleAllocCount = 0;
	size_t hittableAllocCount = 0;
	size_t invisibleAllocCount = 0;

	std::this_thread::sleep_for(std::chrono::seconds(2));
	{
		auto& componentArchive = ComponentArchive::Instance();
		/******************************************************************/
		/* Test API basis by manually */
		const Entity e0 = GenerateEntity();
		bool result = componentArchive.Attach<Visible>(e0);
		assert(result);
		Visible* visible = componentArchive.Get<Visible>(e0);
		const auto visibleHandle = componentArchive.GetHandle<Visible>(e0);
		assert(visible != nullptr);
		assert(visible == componentArchive.Get<Visible>(e0));
		assert(!componentArchive.Attach<Visible>(e0));
		++visibleAllocCount;

		// 'visible' pointer will be expired at here
		result = componentArchive.Attach<Hittable>(e0);
		assert(result);
		Hittable* hittable = componentArchive.Get<Hittable>(e0);
		assert(hittable != nullptr);
		assert(hittable == componentArchive.Get<Hittable>(e0));
		assert(!componentArchive.Attach<Visible>(e0));
		assert(!componentArchive.Attach<Hittable>(e0));
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

		// Visible Handle will be not expired even after attach new component.
		assert(visible->VisibleDistance == visibleHandle->VisibleDistance);
		assert(visibleHandle.IsValid());
		referenceHittable.HitCount = 33333333;
		hittable->HitCount = 33333333;

		// 'visible' pointer and 'hittable' pointer will be expired at here 
		result = componentArchive.Attach<Invisible>(e0);
		assert(result);
		Invisible* invisible = componentArchive.Get<Invisible>(e0);
		++invisibleAllocCount;
		assert(invisible != nullptr);
		assert(invisible == componentArchive.Get<Invisible>(e0));
		assert(!componentArchive.Attach<Visible>(e0));
		assert(!componentArchive.Attach<Hittable>(e0));
		assert(!componentArchive.Attach<Invisible>(e0));

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
		assert(!visibleHandle.IsValid()); /** Visible Handle is no longer valid. */
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
		entities.reserve(TEST_COUNT);
		std::random_device rd;
		std::mt19937 gen(rd());
		const std::uniform_int_distribution dist(0, 99);

		/******************************************************************/
		/* Initial Random Generations & Linear, Random Access Data Validation */
		std::cout << yellow << "* Initial Random Generations & Linear, Random Access Data Validation" << reset << std::endl;
		auto begin = std::chrono::steady_clock::now();
		size_t generatedEntity = 0;
		size_t attachedComponent = 0;
		size_t allocatedComponentSize = 0;

		const auto generateVisible = 
			[&componentArchive, &generatedEntity, &attachedComponent, &visibleAllocCount, &allocatedComponentSize](const size_t count, const Entity entity) 
		{
			if (componentArchive.Attach<Visible>(entity))
			{
				Visible* visible = componentArchive.Get<Visible>(entity);
				if (visible != nullptr)
				{
					visible->A = count + 0xffffff;
					visible->B = count + 0xf0f0f0;
					visible->ClipDistance = 10000.5555f;
					++visibleAllocCount;
					++attachedComponent;
					allocatedComponentSize += sizeof(Visible);
				}
			}

		};

		const auto generateHittable =
			[&componentArchive, &generatedEntity, &attachedComponent, &hittableAllocCount, &allocatedComponentSize](const size_t count, const Entity entity)
		{
			if (componentArchive.Attach<Hittable>(entity))
			{
				Hittable* hittable = componentArchive.Get<Hittable>(entity);
				if (hittable != nullptr)
				{
					++hittableAllocCount;
					++attachedComponent;
					hittable->HitCount = ~static_cast<uint64_t>(entity);
					allocatedComponentSize += sizeof(Hittable);
				}
			}
		};

		const auto generateInvisible =
			[&componentArchive, &generatedEntity, &attachedComponent, &invisibleAllocCount, &allocatedComponentSize](const size_t count, const Entity entity)
		{
			if (componentArchive.Attach<Invisible>(entity))
			{
				const Invisible* invisible = componentArchive.Get<Invisible>(entity);
				if (invisible != nullptr)
				{
					++invisibleAllocCount;
					++attachedComponent;
					allocatedComponentSize += sizeof(Invisible);
				}
			}
		};

		for (size_t count = 0; count < TEST_COUNT; ++count)
		{
			Entity newEntity = GenerateEntity();
			entities.emplace_back(newEntity);
			++generatedEntity;

			/** New Component Generation Distribution */
			auto randomNumber = dist(gen);
			switch (randomNumber % 4)
			{
			default: // 50%
				generateVisible(count, newEntity);
				break;

			case 3: // 25%
				generateHittable(count, newEntity);
				break;

			case 4: // 25%
				generateInvisible(count, newEntity);
				break;
			}

			randomNumber = dist(gen);
			switch (randomNumber % 8)
			{
			case 0: // 10%
				generateVisible(count, newEntity);
				break;

			case 1: // 20%
			case 2:
				generateHittable(count, newEntity);
				break;

			case 3: // 20%
			case 4:
				generateInvisible(count, newEntity);
				break;
			default:
				break;
			}

			randomNumber = dist(gen);
			switch (randomNumber % 8)
			{
			case 0: // 25%
			case 1:
				generateVisible(count, newEntity);
				break;
			case 2:
			case 3:
			case 4: // 37.5%
				generateHittable(count, newEntity);
				break;

			case 6: // 37.5%
			case 7:
			case 5:
				generateInvisible(count, newEntity);
				break;
			default:
				break;
			}
		}
		auto end = std::chrono::steady_clock::now();
		std::cout << "** Random generation takes " << green << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << reset << " ms" << std::endl;
		std::cout << "** Num of generated entities : " << green << generatedEntity << reset << std::endl;
		std::cout << "** Num of attached components : " << green << attachedComponent << reset << std::endl;
		std::cout << "** Allocated Data Size (not include allocated chunks, only measure final archetype size) -> " << green << (allocatedComponentSize/1024/1024.0) << reset << " MB" << std::endl;
		std::cout << "** Average Data per Entity -> " << green << (allocatedComponentSize / generatedEntity) / 1024.0 << reset << " KB" << std::endl;

		auto elapsedTime = LinearDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Random Generation - Linear Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;

		elapsedTime = RandomDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Random Generation - Random Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;

		/******************************************************************/
		/* Filtering Methods (All, Any, None) tests */
		std::cout << std::endl << std::endl << yellow << "* Filtering Methods Tests" << reset << std::endl;
		begin = std::chrono::steady_clock::now();
		auto filteredVisible = Filter::All<Visible>(componentArchive, entities);
		std::cout << "** Num of filtered Visible : " << filteredVisible.size() << std::endl;
		auto filteredHittable = Filter::All<Hittable>(componentArchive, entities);
		std::cout << "** Num of filtered Hittable : " << filteredHittable.size() << std::endl;
		auto filteredInvisible = Filter::All<Invisible>(componentArchive, entities);
		std::cout << "** Num of filtered Invisible : " << filteredInvisible.size() << std::endl;

		/* Attach Order Invariant */
		auto filteredVisibleHittable = Filter::All<Visible, Hittable>(componentArchive, entities);
		auto filteredHittableVisible = Filter::All<Hittable, Visible>(componentArchive, entities);
		std::cout << "** Num of filtered Visible-Hittable : " << filteredVisibleHittable.size() << std::endl;
		std::cout << "** Num of filtered Hittable-Visible : " << filteredHittableVisible.size() << std::endl;
		assert(filteredHittableVisible.size() == filteredVisibleHittable.size());

		auto filteredVisibleInvisible = Filter::All<Visible, Invisible>(componentArchive, entities);
		std::cout << "** Num of filtered Visible-Invisible : " << filteredVisibleHittable.size() << std::endl;

		auto filteredHittableInvisible = Filter::All<Hittable, Invisible>(componentArchive, entities);
		std::cout << "** Num of filtered Hittable-Invisible : " << filteredVisibleHittable.size() << std::endl;

		auto filteredVisInvHit = Filter::All<Hittable, Visible, Invisible>(componentArchive, entities);
		std::cout << "** Num of filtered Hittable-Visible-Invisible : " << filteredVisInvHit.size() << std::endl;

		auto filteredAny = Filter::Any<Visible, Hittable, Invisible>(componentArchive, entities);
		assert(entities.size() == filteredAny.size());
		filteredAny = Filter::Any<Visible, Hittable>(componentArchive, entities);

		auto filteredNone = Filter::None<Visible, Hittable, Invisible>(componentArchive, entities);
		assert(filteredNone.size() == 0);
		end = std::chrono::steady_clock::now();
		std::cout << "** Attach order invariation : ";
		if (filteredHittableVisible.size() == filteredVisibleHittable.size())
		{
			std::cout << green << "True" << reset << std::endl;
		}
		else
		{
			std::cout << red << "False" << reset << std::endl;
		}
		std::cout << "** All Filtering tests takes " << green << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << reset << " ms" << std::endl;

		/******************************************************************/
		/* Random Destroy Tests */
		std::cout << std::endl << std::endl << yellow << "* Random Entity Destroy Tests" << reset << std::endl;
		const std::uniform_int_distribution<size_t> accessDist(0, entities.size() - 1);
		begin = std::chrono::steady_clock::now();
		size_t destroyedCount = 0;
		for (size_t count = 0; count < (TEST_COUNT / 2); ++count)
		{
			const size_t idx = accessDist(gen);
			const Entity entity = entities.at(idx);

			if (entity != INVALID_ENTITY_HANDLE)
			{
				componentArchive.Destroy(entity);
				entities.at(idx) = INVALID_ENTITY_HANDLE;
				++destroyedCount;
			}
		}
		end = std::chrono::steady_clock::now();
		std::cout << "** Destroy takes " << green << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << reset << " ms" << std::endl;
		std::cout << "** Num of destroyed entities -> " << red << destroyedCount << reset << std::endl;

		elapsedTime = LinearDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Random Destroy - Linear Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;

		elapsedTime = RandomDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Random Destroy - Random Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;

		/******************************************************************/
		/* Defragmentation Tests */
		std::cout << std::endl << std::endl << yellow << "* Defragmentation & ShrinkToFit Tests" << reset << std::endl;
		begin = std::chrono::steady_clock::now();
		const auto reduced = componentArchive.ShrinkToFit();
		end = std::chrono::steady_clock::now();
		std::cout << "** Defragmentation & ShrinkToFit takes " << green << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << reset << " ms" << std::endl;
		std::cout << "** Reduced Chunks -> " << red << reduced << reset << std::endl;
		std::cout << "** Freed up Chunks Memory (estimation) -> " << green << ((reduced*16)/1024.0) << reset << " MB" << std::endl;

		/** Check data validation after defragmentation */
		elapsedTime = LinearDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Defragmentation - Linear Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;

		elapsedTime = RandomDataValidation(componentArchive, entities, referenceVisible, referenceHittable, referenceInvisible);
		std::cout << "** Defragmentation - Random Access & Validation takes " << green << elapsedTime.count() << reset << " ms" << std::endl;
	}

	std::cout << std::endl << std::endl << yellow << "* RAII Validation" << reset << std::endl;
	ComponentArchive::DestroyInstance();
	std::cout << "** Num of actual generation (Visible Component) : " << visibleAllocCount << " (times)" << std::endl;
	std::cout << "** Num of call constructor of (Visible Component) : " << Visible::Alloc << " (times)" << std::endl;
	std::cout << "** Num of call destructor of (Visible Component) : " << Visible::Dealloc << " (times)" << std::endl;
	assert(visibleAllocCount == Visible::Alloc);
	assert(Visible::Alloc == Visible::Dealloc);

	std::cout << "** Num of actual generation (Hittable Component) : " << hittableAllocCount << " (times)" << std::endl;
	std::cout << "** Num of call constructor of (Hittable Component) : " << Hittable::Alloc << " (times)" << std::endl;
	std::cout << "** Num of call destructor of (Hittable Component) : " << Hittable::Dealloc << " (times)" << std::endl;
	assert(hittableAllocCount == Hittable::Alloc);
	assert(Hittable::Alloc == Hittable::Dealloc);

	std::cout << "** Num of actual generation (Invisible Component) : " << invisibleAllocCount << " (times)" << std::endl;
	std::cout << "** Num of call constructor of (Invisible Component) : " << Invisible::Alloc << " (times)" << std::endl;
	std::cout << "** Num of call destructor of (Invisible Component) : " << Invisible::Dealloc << " (times)" << std::endl;
	assert(invisibleAllocCount == Invisible::Alloc);
	assert(Invisible::Alloc == Invisible::Dealloc);

	/** Check Memory Leaks */
	_CrtDumpMemoryLeaks();
	return 0;
}