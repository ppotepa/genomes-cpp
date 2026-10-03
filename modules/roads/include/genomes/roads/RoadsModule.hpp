#pragma once
#include <genomes/api/Api.hpp>
namespace genomes::roads {
inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(host, foundation::stable_id("module.roads"),
        {foundation::stable_id("core"), foundation::stable_id("module.world")},
        {foundation::stable_id("roads.network")}, [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = foundation::stable_id("roads.simulation");
            system.reads = {foundation::stable_id("world.snapshot")};
            system.writes = {foundation::stable_id("roads.network")};
            return registry.declareSystem(context.module(), std::move(system));
        });
}
} // namespace genomes::roads
