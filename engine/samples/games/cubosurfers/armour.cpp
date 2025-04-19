#include "armour.hpp"

#include <cubos/core/ecs/reflection.hpp>
#include <cubos/core/reflection/external/glm.hpp>
#include <cubos/core/reflection/external/primitives.hpp>
#include <cubos/core/reflection/external/string.hpp>

#include <cubos/engine/assets/plugin.hpp>
#include "cubos/engine/transform/plugin.hpp"

using namespace cubos::engine;

CUBOS_REFLECT_IMPL(Armour)
{
    return cubos::core::ecs::TypeBuilder<Armour>("Armour")
        .withField("velocity", &Armour::velocity)
        .withField("killZ", &Armour::killZ)
        .build();
}

void armourPlugin(Cubos& cubos)
{
    cubos.depends(assetsPlugin);
    cubos.depends(transformPlugin);

    cubos.component<Armour>();

    cubos.system("move Armours")
        .call([](Commands cmds, const DeltaTime& dt, Query<Entity, const Armour&, Position&> armours) {
            for (auto [ent, armour, position] : armours)
            {
                position.vec += armour.velocity * dt.value();
                position.vec.y = glm::abs(glm::sin(position.vec.z * 0.15F)) * 1.5F;

                if (position.vec.z < armour.killZ)
                {
                    cmds.destroy(ent);
                }
            }
        });
}
