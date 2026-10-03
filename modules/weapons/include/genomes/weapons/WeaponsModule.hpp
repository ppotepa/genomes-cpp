#pragma once

#include <genomes/api/Api.hpp>

namespace genomes::weapons {

inline foundation::Result<void, foundation::Error> registerModule(api::ModuleHost& host) {
    return api::registerDomainModule(
        host, foundation::stable_id("module.weapons"), {foundation::stable_id("core")},
        {foundation::stable_id("weapons.catalog")},
        [](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiOperationDescriptor equip{};
            equip.id = foundation::stable_id("weapons.equip");
            equip.arguments = {api::ValueType::UnsignedInteger,
                               api::ValueType::UnsignedInteger};
            return registry.declareCommand(context.module(), std::move(equip));
        });
}

} // namespace genomes::weapons
