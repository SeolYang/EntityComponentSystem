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

			bool bIsExistEntities = lutParentItr != lut.end() && lutChildItr != lut.end();
			bool bIsNotSameEntities = parent != child;
			if (bIsExistEntities && bIsNotSameEntities)
			{
				size_t parentOffset = lutParentItr->second;
				size_t childOffset = lutChildItr->second;

				if (components[childOffset].parentEntity != parent)
				{
					std::vector<HierarchyComponent> tempComps;
					std::vector<Entity> tempEntities;
					InjectSubtree(parent, childOffset, tempComps, tempEntities);

					size_t moveBegin = 0;
					size_t moveEnd = 0;
					size_t moveTo = 0;
					size_t moveAt = 0;
					{
						if (childOffset < parentOffset)
						{
							moveBegin = childOffset + tempComps.size();
							moveTo = childOffset;

							for (moveEnd = moveBegin; moveEnd < components.size(); ++moveEnd)
							{
								if (components[moveEnd].parentEntity == INVALID_ENTITY_HANDLE)
								{
									break;
								}
							}
							++moveEnd;

							moveAt = moveEnd - 1;
						}
						else if (childOffset > parentOffset)
						{
							moveBegin = parentOffset + 1;
							moveEnd = moveBegin + (childOffset - parentOffset - 1);
							moveTo = moveBegin + tempComps.size();

							moveAt = moveBegin;
						}

						MoveElementBlock(moveBegin, moveEnd, moveTo);
						MoveElementBlockFrom(std::move(tempComps), std::move(tempEntities), moveAt);
						UpdateLUT();
					}
				}
			}
		}

		void Detach(Entity target)
		{
			if (auto targetItr = lut.find(target); targetItr != lut.end())
			{
				size_t rootIdx = targetItr->second;

				std::vector<HierarchyComponent> componentSubTree;
				std::vector<Entity> entitySubTree;
				InjectSubtree(INVALID_ENTITY_HANDLE, rootIdx, componentSubTree, entitySubTree);

				size_t injectedRange = entitySubTree.size();
				MoveElementBlock(rootIdx + injectedRange, Size(), rootIdx);
				MoveElementBlockFrom(std::move(componentSubTree), std::move(entitySubTree), rootIdx + injectedRange);
				UpdateLUT();
			}
		}

	private:
		size_t InjectSubtree(Entity parent, size_t rootIdx, std::vector<HierarchyComponent>& tempComps, std::vector<Entity>& tempEntities)
		{
			if (rootIdx >= 0 && rootIdx < components.size())
			{
				Entity rootEntity = entities[rootIdx];
				entities[rootIdx] = INVALID_ENTITY_HANDLE;

				components[rootIdx].parentEntity = parent;
				tempComps.emplace_back(std::move(components[rootIdx]));
				tempEntities.emplace_back(rootEntity);

				size_t numSubTreeElements = 1;
				for (size_t childIdx = rootIdx + 1; childIdx < components.size(); ++childIdx)
				{
					if (components[childIdx].parentEntity == rootEntity)
					{
						numSubTreeElements += InjectSubtree(rootEntity, childIdx, tempComps, tempEntities);
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