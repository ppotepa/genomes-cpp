#pragma once
#include <genomes/api/Api.hpp>
namespace genomes::destruction {
inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(host, foundation::stable_id("module.destruction"),
        {foundation::stable_id("core"), foundation::stable_id("module.world")},
        {foundation::stable_id("destruction.fields")}, [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = foundation::stable_id("destruction.simulation");
            system.reads = {foundation::stable_id("world.snapshot")};
            system.writes = {foundation::stable_id("destruction.fields")};
            return registry.declareSystem(context.module(), std::move(system));
        });
}
} // namespace genomes::destruction
