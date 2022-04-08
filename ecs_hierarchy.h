#pragma once
#include "ecs.h"
#include <iostream>

namespace sy::ecs
{
	struct HierarchyComponent
	{
		Entity parentEntity = INVALID_ENTITY_HANDLE;
	};

	template <>
	class ComponentPool<HierarchyComponent> :
		public ComponentPoolBase<HierarchyComponent>
	{
	public:
		explicit ComponentPool(size_t reservedSize = DEFAULT_COMPONENT_POOL_SIZE) :
			ComponentPoolBase<HierarchyComponent>(reservedSize)
		{
		}

		void Attach(Entity parent, Entity child)
		{
			auto lutParentItr = lut.find(parent);
			auto lutChildItr = lut.find(child);
			if (lutParentItr != lutChildItr)
			{
				if (lutParentItr != lut.end() && lutChildItr != lut.end())
				{
					size_t parentOffset = lutParentItr->second;
					size_t childOffset = lutChildItr->second;

					components[childOffset].parentEntity = parent;

					std::vector<HierarchyComponent> tempComps;
					std::vector<Entity> tempEntities;
					InjectSubtree(childOffset, tempComps, tempEntities);

					size_t copyBegin = 0;
					size_t copyEnd = 0;
					size_t copyTo = 0;
					{
						if (childOffset < parentOffset)
						{
							copyBegin = childOffset + tempComps.size();
							copyTo = childOffset;

							for (copyEnd = copyBegin; copyEnd < components.size(); ++copyEnd)
							{
								if (components[copyEnd].parentEntity == INVALID_ENTITY_HANDLE)
								{
									break;
								}
							}

							++copyEnd;
							std::copy(
								components.begin() + copyBegin,
								components.begin() + copyEnd,
								components.begin() + copyTo);

							std::copy(
								entities.begin() + copyBegin,
								entities.begin() + copyEnd,
								entities.begin() + copyTo);

							std::copy(
								tempComps.begin(),
								tempComps.end(),
								components.begin() + copyEnd - 1);

							std::copy(
								tempEntities.begin(),
								tempEntities.end(),
								entities.begin() + copyEnd - 1);
						}
						else if (childOffset > parentOffset)
						{
							copyBegin = parentOffset + 1;
							copyEnd = copyBegin + (childOffset - parentOffset - 1);
							copyTo = copyBegin + tempComps.size();
							std::copy(
								components.begin() + copyBegin,
								components.begin() + copyEnd,
								components.begin() + copyTo);

							std::copy(
								entities.begin() + copyBegin,
								entities.begin() + copyEnd,
								entities.begin() + copyTo);

							std::copy(
								tempComps.begin(),
								tempComps.end(),
								components.begin() + copyBegin);

							std::copy(
								tempEntities.begin(),
								tempEntities.end(),
								entities.begin() + copyBegin);
						}

						for (size_t idx = 0; idx < entities.size(); ++idx)
						{
							lut[entities[idx]] = idx;
						}
					}
				}
			}
		}

	private:
		size_t InjectSubtree(size_t rootIdx, std::vector<HierarchyComponent>& tempComps, std::vector<Entity>& tempEntities)
		{
			if (rootIdx >= 0 && rootIdx < components.size())
			{
				tempComps.emplace_back(std::move(components[rootIdx]));
				tempEntities.emplace_back(entities[rootIdx]);

				size_t numSubTreeElements = 1;
				for (size_t childIdx = rootIdx + 1; childIdx < components.size(); ++childIdx)
				{
					if (components[childIdx].parentEntity == entities[rootIdx])
					{
						numSubTreeElements += InjectSubtree(childIdx, tempComps, tempEntities);
						childIdx = rootIdx + numSubTreeElements - 1;

					}
					else
					{
						break;
					}
				}

				return numSubTreeElements;
			}

			return 0;
		}


	};
}