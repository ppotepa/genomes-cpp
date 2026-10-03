#pragma once
#include <genomes/api/Api.hpp>
namespace genomes::ballistics {
inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(host, foundation::stable_id("module.ballistics"),
        {foundation::stable_id("core"), foundation::stable_id("module.combat"),
         foundation::stable_id("module.weapons")},
        {foundation::stable_id("ballistics.projectiles")}, [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = foundation::stable_id("ballistics.simulation");
            system.reads = {foundation::stable_id("world.snapshot")};
            system.writes = {foundation::stable_id("ballistics.projectiles")};
            return registry.declareSystem(context.module(), std::move(system));
        });
}
} // namespace genomes::ballistics
