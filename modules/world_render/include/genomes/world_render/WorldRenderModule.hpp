#pragma once
#include <genomes/api/Api.hpp>
namespace genomes::world_render {
inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(host, foundation::stable_id("module.world_render"),
        {foundation::stable_id("core"), foundation::stable_id("module.world")},
        {foundation::stable_id("world.presentation")}, [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = foundation::stable_id("world.extraction");
            system.reads = {foundation::stable_id("world.snapshot")};
            system.writes = {foundation::stable_id("world.presentation")};
            system.lane = jobs::ExecutionLane::Render;
            return registry.declareSystem(context.module(), std::move(system));
        });
}
} // namespace genomes::world_render
