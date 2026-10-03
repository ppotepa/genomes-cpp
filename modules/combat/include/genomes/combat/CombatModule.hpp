#pragma once

#include <genomes/api/Api.hpp>

namespace genomes::combat {

inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(
        host, foundation::stable_id("module.combat"),
        {foundation::stable_id("core"), foundation::stable_id("module.world")},
        {foundation::stable_id("combat.events")},
        [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiOperationDescriptor attack{};
            attack.id = foundation::stable_id("combat.attack");
            attack.arguments = {api::ValueType::UnsignedInteger,
                                api::ValueType::UnsignedInteger, api::ValueType::Bytes};
            return registry.declareCommand(context.module(), std::move(attack));
        });
}

} // namespace genomes::combat
