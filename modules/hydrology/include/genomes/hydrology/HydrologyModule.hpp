#pragma once
#include <genomes/api/Api.hpp>
namespace genomes::hydrology {
inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(host, foundation::stable_id("module.hydrology"),
        {foundation::stable_id("core"), foundation::stable_id("module.world")},
        {foundation::stable_id("hydrology.fields")}, [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = foundation::stable_id("hydrology.simulation");
            system.reads = {foundation::stable_id("world.snapshot")};
            system.writes = {foundation::stable_id("hydrology.fields")};
            return registry.declareSystem(context.module(), std::move(system));
        });
}
} // namespace genomes::hydrology
