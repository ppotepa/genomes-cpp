#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/api/Api.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstdint>
#include <utility>

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

// Composition-root registration. The module exposes only stable API
// descriptors; runtime implementations remain behind SimulationFacade.
inline foundation::Result<void, foundation::Error> registerModule(
    api::ModuleHost& host) {
    const InfantryModuleRegistration identity = InfantryModule::registration();
    return host.registerModule(
        {.id = identity.module_id,
         .version = {},
         .required_modules = {foundation::stable_id("core")},
         .required_capabilities = {foundation::stable_id("core.scheduler")},
         .provided_capabilities = {identity.phenotype_service}},
        [identity](api::ModuleRegistry& registry, api::ModuleContext& context) {
            if (!context.hasCapability(foundation::stable_id("core.scheduler"))) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "infantry scheduler capability missing"});
            }
            api::ApiOperationDescriptor issue{};
            issue.id = foundation::stable_id("units.issue");
            issue.arguments = {api::ValueType::Bytes};
            issue.lane = jobs::ExecutionLane::Worker;
            issue.deterministic = true;
            auto result = registry.declareCommand(identity.module_id, issue);
            if (!result) return result;
            api::ApiOperationDescriptor query{};
            query.id = foundation::stable_id("units.snapshot");
            query.arguments = {api::ValueType::Bytes};
            result = registry.declareQuery(identity.module_id, query);
            if (!result) return result;
            api::ApiSystemDescriptor simulation{};
            simulation.id = foundation::stable_id("units.simulation");
            simulation.reads = {foundation::stable_id("world.snapshot")};
            simulation.writes = {foundation::stable_id("units.snapshot")};
            simulation.lane = jobs::ExecutionLane::Worker;
            result = registry.declareSystem(identity.module_id, std::move(simulation));
            if (!result) return result;
            result = registry.declareResourceRead(
                identity.module_id, foundation::stable_id("world.snapshot"));
            if (!result) return result;
            return registry.declareResourceWrite(
                identity.module_id, foundation::stable_id("units.snapshot"));
        });
}

} // namespace genomes::infantry
