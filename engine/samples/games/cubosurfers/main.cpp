#include <glm/common.hpp>
#include <imgui.h>

#include <cubos/engine/assets/plugin.hpp>
#include <cubos/engine/collisions/colliding_with.hpp>
#include <cubos/engine/defaults/plugin.hpp>
#include <cubos/engine/input/plugin.hpp>
#include <cubos/engine/render/lights/environment.hpp>
#include <cubos/engine/render/voxels/palette.hpp>
#include <cubos/engine/scene/plugin.hpp>
#include <cubos/engine/settings/plugin.hpp>
#include <cubos/engine/tools/plugin.hpp>
#include <cubos/engine/utils/free_camera/plugin.hpp>
#include <cubos/engine/voxels/plugin.hpp>

#include "armour.hpp"
#include "cubos/core/tel/logging.hpp"
#include "cubos/engine/imgui/plugin.hpp"
#include "cubos/engine/prelude.hpp"
#include "obstacle.hpp"
#include "player.hpp"
#include "spawner.hpp"

using namespace cubos::engine;

static const Asset<Scene> SceneAsset = AnyAsset("/assets/scenes/main.cubos");
static const Asset<VoxelPalette> PaletteAsset = AnyAsset("/assets/main.pal");
static const Asset<InputBindings> InputBindingsAsset = AnyAsset("/assets/input.bind");

static float score = 0;

static void restartGame(Commands& cmds, const Assets& assets, DeltaTime& dt, Query<Entity> all);

int main(int argc, char** argv)
{
    Cubos cubos{argc, argv};

    cubos.plugin(defaultsPlugin);
    cubos.plugin(freeCameraPlugin);
    cubos.plugin(toolsPlugin);
    cubos.plugin(spawnerPlugin);
    cubos.plugin(obstaclePlugin);
    cubos.plugin(armourPlugin);
    cubos.plugin(playerPlugin);

    cubos.startupSystem("configure settings").before(settingsTag).call([](Settings& settings) {
        settings.setString("assets.app.osPath", APP_ASSETS_PATH);
        settings.setString("assets.builtin.osPath", BUILTIN_ASSETS_PATH);
    });

    cubos.startupSystem("set the palette, environment, input bindings and spawn the scene")
        .tagged(assetsTag)
        .call([](Commands commands, const Assets& assets, RenderPalette& palette, Input& input,
                 RenderEnvironment& environment) {
            palette.asset = PaletteAsset;
            environment.ambient = {0.1F, 0.1F, 0.1F};
            environment.skyGradient[0] = {0.2F, 0.4F, 0.8F};
            environment.skyGradient[1] = {0.6F, 0.6F, 0.8F};
            input.bind(*assets.read(InputBindingsAsset));
            commands.spawn(assets.read(SceneAsset)->blueprint);
        });

    cubos.system("restart the game on input")
        .call([](Commands cmds, const Assets& assets, DeltaTime& dt, const Input& input, Query<Entity> all) {
            if (input.justPressed("restart"))
            {
                restartGame(cmds, assets, dt, all);
            }
        });

    cubos.system("detect player vs obstacle collisions")
        .call([](Query<Player&, const CollidingWith&, const Obstacle&, Entity> collisions, Commands cmds, const Assets& assets,
                 DeltaTime& dt, Query<Entity> all) {
            for (auto [player, collidingWith, obstacle, ent] : collisions)
            {
                CUBOS_INFO("Player collided with an obstacle!");

                if (player.armoured)
                {
                    CUBOS_INFO("Player is armoured and will not take damage");
                    player.armoured = false;
                    
                    // Destroy the obstacle to avoid multiple collisions
                    cmds.destroy(ent);
                    
                }
                else
                {
                    CUBOS_INFO("Restarting game!");
                    restartGame(cmds, assets, dt, all);
                }
            }
        });

    cubos.system("speedup the game gradually").call([](DeltaTime& dt) {
        const float growthRate = 0.02F;
        const float maxScale = 2.6F;
        dt.scale *= (1 + growthRate * dt.value()); // % increase per second
        dt.scale = glm::clamp(dt.scale, 1.0F, maxScale);
    });

    cubos.system("update and show score on an ImGui window").tagged(imguiTag).call([](const DeltaTime& dt) {
        // Update score
        score += dt.value();

        // ImGui Window
        ImGui::Begin("Score");
        ImGui::Text("%d", (int)score);
        ImGui::End();
    });
    
    cubos.system("detect player vs armour collisions")
        .call([](Commands cmds, Query<Player&, const CollidingWith&, const Armour&, Entity> collisions) {
            for (auto [player, collidingWith, armour, ent] : collisions)
            {
                CUBOS_INFO("Player collided with an armour power-up!");
                player.armoured = true;
                cmds.destroy(ent);
            }
        });

    cubos.run();
}

static void restartGame(Commands& cmds, const Assets& assets, DeltaTime& dt, Query<Entity> all)
{
    CUBOS_INFO("Restarting Game!");
    dt.scale = 1.0F;
    score = 0;
    for (auto [ent] : all)
    {
        cmds.destroy(ent);
    }

    cmds.spawn(assets.read(SceneAsset)->blueprint);
}
