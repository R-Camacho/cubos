#include "plugin.hpp"
#include <algorithm>

#include <cubos/engine/collisions/collision_layers.hpp>
#include <cubos/engine/collisions/collision_mask.hpp>
#include <cubos/engine/collisions/plugin.hpp>
#include <cubos/engine/collisions/shapes/box.hpp>
#include <cubos/engine/collisions/shapes/capsule.hpp>
#include <cubos/engine/fixed_step/plugin.hpp>
#include <cubos/engine/transform/local_to_world.hpp>
#include <cubos/engine/transform/plugin.hpp>

#include "../interface/plugin.hpp"
#include "collider_aabb.hpp"
#include "collision_group.hpp"
#include "sweep_and_prune.hpp"

CUBOS_DEFINE_TAG(cubos::engine::collisionsAABBUpdateTag);
CUBOS_DEFINE_TAG(cubos::engine::collisionsBroadMarkersTag);
CUBOS_DEFINE_TAG(cubos::engine::collisionsBroadSweepTag);
CUBOS_DEFINE_TAG(cubos::engine::collisionsBroadCleanTag);
CUBOS_DEFINE_TAG(cubos::engine::collisionsBroadTag);

void cubos::engine::broadPhaseCollisionsPlugin(Cubos& cubos)
{
    cubos.depends(transformPlugin);
    cubos.depends(fixedStepPlugin);
    cubos.depends(interfaceCollisionsPlugin);

    cubos.component<ColliderAABB>();

    cubos.relation<PotentiallyCollidingWith>();

    cubos.resource<BroadPhaseSweepAndPrune>();

    cubos.tag(collisionsAABBUpdateTag).tagged(fixedStepTag);
    cubos.tag(collisionsBroadMarkersTag).tagged(fixedStepTag);
    cubos.tag(collisionsBroadSweepTag).tagged(fixedStepTag);
    cubos.tag(collisionsBroadCleanTag).tagged(fixedStepTag);
    cubos.tag(collisionsBroadTag).tagged(fixedStepTag);

    cubos.observer("add new Colliders to SweepAndPrune")
        .onAdd<ColliderAABB>()
        .call([](Query<Entity> query, BroadPhaseSweepAndPrune& sweepAndPrune) {
            for (auto [entity] : query)
            {
                sweepAndPrune.addEntity(entity);
            }
        });

    cubos.observer("remove old Colliders from SweepAndPrune")
        .onRemove<ColliderAABB>()
        .call([](Query<Entity> query, BroadPhaseSweepAndPrune& sweepAndPrune) {
            for (auto [entity] : query)
            {
                sweepAndPrune.removeEntity(entity);
            }
        });

    cubos.system("update collider AABBs")
        .tagged(collisionsAABBUpdateTag)
        .after(transformUpdateTag)
        .call([](Query<const LocalToWorld&, ColliderAABB&> query) {
            for (auto [localToWorld, colliderAABB] : query)
            {
                // Get the 4 points of the collider.
                glm::vec3 corners[4];
                colliderAABB.localAABB.box().corners4(corners);

                // Pack the 3 points of the collider into a matrix.
                auto points = glm::mat4{glm::vec4{corners[0], 1.0F}, glm::vec4{corners[1], 1.0F},
                                        glm::vec4{corners[2], 1.0F}, glm::vec4{corners[3], 1.0F}};

                // Transforms collider space to world space.
                auto transform = localToWorld.mat;

                // Only want scale and rotation, extract translation and remove it.
                auto translation = glm::vec3{transform[3]};
                transform[3] = glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};

                // Rotate and scale corners.
                auto rotatedCorners = glm::mat4x3{transform * points};

                // Get the max of the rotated corners.
                auto max = glm::max(glm::abs(rotatedCorners[0]), glm::abs(rotatedCorners[1]));
                max = glm::max(max, glm::abs(rotatedCorners[2]));
                max = glm::max(max, glm::abs(rotatedCorners[3]));

                // Add the collider's margin.
                max += glm::vec3{colliderAABB.margin};

                // Set the AABB.
                colliderAABB.worldAABB.min(translation - max);
                colliderAABB.worldAABB.max(translation + max);
            };
        });

    cubos.system("update sweep and prune markers")
        .tagged(collisionsBroadMarkersTag)
        .after(collisionsAABBUpdateTag)
        .call([](Query<const ColliderAABB&> query, BroadPhaseSweepAndPrune& sweepAndPrune) {
            // TODO: This is parallelizable.
            for (glm::length_t axis = 0; axis < 3; axis++)
            {
                // TODO: Should use insert sort to leverage spatial coherence.
                std::sort(sweepAndPrune.markersPerAxis[axis].begin(), sweepAndPrune.markersPerAxis[axis].end(),
                          [axis, &query](const BroadPhaseSweepAndPrune::SweepMarker& a,
                                         const BroadPhaseSweepAndPrune::SweepMarker& b) {
                              auto aMatch = query.at(a.entity);
                              auto bMatch = query.at(b.entity);
                              if (aMatch && bMatch)
                              {
                                  auto [aColliderAABB] = *aMatch;
                                  auto [bColliderAABB] = *bMatch;
                                  auto aPos = a.isMin ? aColliderAABB.worldAABB.min() : aColliderAABB.worldAABB.max();
                                  auto bPos = b.isMin ? bColliderAABB.worldAABB.min() : bColliderAABB.worldAABB.max();
                                  return aPos[axis] < bPos[axis];
                              }
                              return true;
                          });
            }
        });

    cubos.system("collisions sweep")
        .tagged(collisionsBroadSweepTag)
        .after(collisionsBroadMarkersTag)
        .call([](BroadPhaseSweepAndPrune& sweepAndPrune) {
            // TODO: This is parallelizable.
            for (glm::length_t axis = 0; axis < 3; axis++)
            {
                sweepAndPrune.activePerAxis[axis].clear();
                sweepAndPrune.sweepOverlapMaps[axis].clear();

                for (auto& marker : sweepAndPrune.markersPerAxis[axis])
                {
                    if (marker.isMin)
                    {
                        for (const auto& other : sweepAndPrune.activePerAxis[axis])
                        {
                            sweepAndPrune.sweepOverlapMaps[axis][marker.entity].push_back(other);
                        }

                        sweepAndPrune.activePerAxis[axis].insert(marker.entity);
                    }
                    else
                    {
                        sweepAndPrune.activePerAxis[axis].erase(marker.entity);
                    }
                }
            }
        });

    cubos.system("clean PotentiallyCollidingWith relations")
        .tagged(collisionsBroadCleanTag)
        .before(collisionsBroadTag)
        .call([](Commands cmds, Query<Entity, PotentiallyCollidingWith&, Entity> query) {
            for (auto [entity, collidingWith, other] : query)
            {
                cmds.unrelate<PotentiallyCollidingWith>(entity, other);
            }
        });

    cubos.system("create PotentiallyCollidingWith relations")
        .tagged(collisionsBroadTag)
        .after(collisionsBroadSweepTag)
        .call([](Commands cmds, Query<const ColliderAABB&, const CollisionLayers&, const CollisionMask&> query,
                 const BroadPhaseSweepAndPrune& sweepAndPrune) {
            for (glm::length_t axis = 0; axis < 3; axis++)
            {
                for (const auto& [entity, overlaps] : sweepAndPrune.sweepOverlapMaps[axis])
                {
                    auto match = query.at(entity);
                    if (!match)
                    {
                        continue;
                    }
                    auto [colliderAABB, layers, mask] = *match;

                    for (const auto& other : overlaps)
                    {
                        auto otherMatch = query.at(other);
                        if (!otherMatch)
                        {
                            continue;
                        }
                        auto [otherCollider, otherLayers, otherMask] = *otherMatch;

                        if (((layers.value & otherMask.value) == 0U) && ((otherLayers.value & mask.value) == 0U))
                        {
                            continue;
                        }

                        switch (axis)
                        {
                        case 0: // X
                            if (colliderAABB.worldAABB.overlapsY(otherCollider.worldAABB) &&
                                colliderAABB.worldAABB.overlapsZ(otherCollider.worldAABB))
                            {
                                cmds.relate(entity, other, PotentiallyCollidingWith{});
                            }
                            break;
                        case 1: // Y
                            if (colliderAABB.worldAABB.overlapsX(otherCollider.worldAABB) &&
                                colliderAABB.worldAABB.overlapsZ(otherCollider.worldAABB))
                            {
                                cmds.relate(entity, other, PotentiallyCollidingWith{});
                            }
                            break;
                        case 2: // Z
                            if (colliderAABB.worldAABB.overlapsX(otherCollider.worldAABB) &&
                                colliderAABB.worldAABB.overlapsY(otherCollider.worldAABB))
                            {
                                cmds.relate(entity, other, PotentiallyCollidingWith{});
                            }
                            break;
                        }
                    }
                }
            }
        });
}
