#include "player.hpp"

#include <cubos/core/ecs/reflection.hpp>
#include <cubos/core/reflection/external/primitives.hpp>

#include <cubos/engine/assets/plugin.hpp>
#include <cubos/engine/input/plugin.hpp>
#include <cubos/engine/render/voxels/grid.hpp>
#include <cubos/engine/render/voxels/load.hpp>
#include <cubos/engine/render/voxels/plugin.hpp>
#include <cubos/engine/transform/plugin.hpp>

using namespace cubos::engine;

CUBOS_REFLECT_IMPL(Player)
{
    return cubos::core::ecs::TypeBuilder<Player>("Player")
        .withField("speed", &Player::speed)
        .withField("laneWidth", &Player::laneWidth)
        .withField("armoured", &Player::armoured)
        .withField("hasJetpack", &Player::hasJetpack)
        .withField("jetpackTimer", &Player::jetpackTimer)
        .withField("jetpackDuration", &Player::jetpackDuration)
        .withField("targetY", &Player::targetY)
        .withField("jetpackHeight", &Player::jetpackHeight)
        .withField("verticalSpeed", &Player::verticalSpeed)
        .build();
}

static const Asset<VoxelGrid> BasePlayerAsset = AnyAsset("57d1b886-8543-4b8b-8f78-d911e9c4f896");
static const Asset<VoxelGrid> JetpackPlayerAsset = AnyAsset("c7263b46-be18-47c2-b3ef-05592b2e9dec");
static const Asset<VoxelGrid> ShieldPlayerAsset = AnyAsset("4892c2f3-10b3-4ca7-9de3-822b77a0ba7e");

void playerPlugin(Cubos& cubos)
{
    cubos.depends(inputPlugin);
    cubos.depends(transformPlugin);
    cubos.depends(renderVoxelsPlugin);

    cubos.component<Player>();

    cubos.system("move player").call([](Input& input, const DeltaTime& dt, Query<Player&, Position&> players) {
        for (auto [player, position] : players)
        {
            if (player.hasJetpack)
            {
                player.jetpackTimer -= dt.value();

                if (player.jetpackTimer <= 0.0F)
                {
                    // Timer expired
                    CUBOS_INFO("Jetpack expired");
                    player.hasJetpack = false;
                    player.targetY = 0.0F;
                }
                else
                {
                    player.targetY = player.jetpackHeight;
                }
            }
            else
            {
                // Not jetpacking
                player.targetY = 0.0F;
            }

            if (input.pressed("left") && player.lane == player.targetLane)
            {
                player.targetLane = glm::clamp(player.lane - 1, -1, 1);
            }

            if (input.pressed("right") && player.lane == player.targetLane)
            {
                player.targetLane = glm::clamp(player.lane + 1, -1, 1);
            }

            if (player.lane != player.targetLane)
            {
                auto sourceX = static_cast<float>(-player.lane) * player.laneWidth;
                auto targetX = static_cast<float>(-player.targetLane) * player.laneWidth;
                float currentT = (position.vec.x - sourceX) / (targetX - sourceX);
                float newT = glm::min(1.0F, currentT + dt.value() * player.speed);
                position.vec.x = glm::mix(sourceX, targetX, newT);

                if (!player.hasJetpack)
                {
                    player.targetY = glm::sin(currentT * glm::pi<float>()) * 2.0F;
                }

                if (newT >= 1.0F)
                {
                    player.lane = player.targetLane;

                    if (!player.hasJetpack)
                    {
                        player.targetY = 0.0F;
                    }
                }
            }

            // Interpolate between current Y and target Y
            position.vec.y = glm::mix(position.vec.y, player.targetY, dt.value() * player.verticalSpeed);
        }
    });

    cubos.system("update player model").call([](Commands cmds, Query<Entity, const Player&, RenderVoxelGrid&> players) {
        for (auto [ent, player, renderGrid] : players)
        {
            Asset<VoxelGrid> targetAsset = renderGrid.asset;
            if (player.hasJetpack)
            {
                targetAsset = JetpackPlayerAsset;
            }
            else if (player.armoured)
            {
                targetAsset = ShieldPlayerAsset;
            }
            else
            {
                targetAsset = BasePlayerAsset;
            }

            // Only change the asset if needed
            if (targetAsset != renderGrid.asset)
            {
                renderGrid.asset = targetAsset;
                CUBOS_INFO("Player model updated to {}", targetAsset);
                cmds.add(ent, LoadRenderVoxels{});
            }
        }
    });
}
