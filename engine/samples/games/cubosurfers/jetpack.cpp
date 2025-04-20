#include "jetpack.hpp"

#include <cubos/core/ecs/reflection.hpp>
#include <cubos/core/reflection/external/glm.hpp>
#include <cubos/core/reflection/external/primitives.hpp>
#include <cubos/core/reflection/external/string.hpp>

#include <cubos/engine/assets/plugin.hpp>
#include "cubos/engine/transform/plugin.hpp"

using namespace cubos::engine;

CUBOS_REFLECT_IMPL(Jetpack)
{
    return cubos::core::ecs::TypeBuilder<Jetpack>("Jetpack")
        .withField("velocity", &Jetpack::velocity)
        .withField("killZ", &Jetpack::killZ)
        .build();
}

void jetpackPlugin(Cubos& cubos)
{
    cubos.depends(assetsPlugin);
    cubos.depends(transformPlugin);

    cubos.component<Jetpack>();

    cubos.system("move Jetpacks")
        .call([](Commands cmds, const DeltaTime& dt, Query<Entity, const Jetpack&, Position&> jetpacks) {
            for (auto [ent, jetpack, position] : jetpacks)
            {
                position.vec += jetpack.velocity * dt.value();
                position.vec.y = glm::abs(glm::sin(position.vec.z * 0.15F)) * 1.5F;

                if (position.vec.z < jetpack.killZ)
                {
                    cmds.destroy(ent);
                }
            }
        });
}
