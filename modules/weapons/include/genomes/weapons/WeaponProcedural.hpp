#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

namespace genomes::weapons {

struct WeaponGenerationRequest final {
    WeaponDefinition definition{};
    WeaponVariant variant{};
};

[[nodiscard]] foundation::Result<void, foundation::Error> registerWeaponGenerator(
    proc::GeneratorRegistry::Builder& builder);

} // namespace genomes::weapons
