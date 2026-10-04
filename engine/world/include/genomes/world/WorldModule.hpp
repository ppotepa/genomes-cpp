#pragma once

#include <genomes/api/Api.hpp>

namespace genomes::world {

inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(
        host, foundation::stable_id("module.world"),
        {foundation::stable_id("core")},
        {foundation::stable_id("world.snapshot"), foundation::stable_id("world.resources")},
        [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiOperationDescriptor set{};
            set.id = foundation::stable_id("world.set");
            set.arguments = {api::ValueType::String, api::ValueType::Bytes};
            auto result = registry.declareCommand(context.module(), std::move(set));
            if (!result) return result;
            api::ApiOperationDescriptor regenerate{};
            regenerate.id = foundation::stable_id("world.regenerate");
            regenerate.arguments = {api::ValueType::Bytes};
            regenerate.lane = jobs::ExecutionLane::Main;
            regenerate.deterministic = true;
            result = registry.declareCommand(context.module(), std::move(regenerate));
            if (!result) return result;
            api::ApiOperationDescriptor query{};
            query.id = foundation::stable_id("world.query");
            query.arguments = {api::ValueType::String};
            return registry.declareQuery(context.module(), std::move(query));
        });
}

} // namespace genomes::world
