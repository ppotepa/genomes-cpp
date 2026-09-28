#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstdint>

namespace genomes::infantry {

struct InfantryIdentity final {
    simulation::EntityId entity{};
    foundation::StableId genome_artifact{0};
    foundation::StableId phenotype_artifact{0};
    std::uint32_t schema_version{1};

    [[nodiscard]] bool valid() const noexcept {
        return entity.isValid() && genome_artifact != 0 && phenotype_artifact != 0 &&
               schema_version != 0;
    }
};

struct InfantryModuleRegistration final {
    foundation::StableId module_id{foundation::stable_id("module.infantry")};
    foundation::StableId identity_component{
        foundation::stable_id("component.infantry.identity")};
    foundation::StableId phenotype_service{foundation::stable_id("service.infantry.phenotype")};
    bool explicit_registration{true};

    [[nodiscard]] bool valid() const noexcept {
        return module_id != 0 && identity_component != 0 && phenotype_service != 0 &&
               explicit_registration;
    }
};

class InfantryModule final {
public:
    [[nodiscard]] static InfantryModuleRegistration registration() noexcept { return {}; }

    [[nodiscard]] static foundation::Result<InfantryIdentity, foundation::Error> bindIdentity(
        InfantryIdentity identity) noexcept {
        if (!registration().valid() || !identity.valid()) {
            return foundation::Result<InfantryIdentity, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid infantry identity"});
        }
        return foundation::Result<InfantryIdentity, foundation::Error>::success(identity);
    }
};

} // namespace genomes::infantry
