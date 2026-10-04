#include <genomes/runtime/ModuleSystemBridge.hpp>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace genomes::runtime {

namespace {

[[nodiscard]] foundation::Result<simulation::SystemExecutionPlan, foundation::Error>
bridgeFailure(foundation::ErrorCode code, const char* message) {
    return foundation::Result<simulation::SystemExecutionPlan,
                              foundation::Error>::failure({code, message});
}

} // namespace

foundation::Result<simulation::SystemExecutionPlan, foundation::Error>
ModuleSystemBridge::compile(simulation::SystemGraph& graph,
                            const api::ModuleRegistry& registry,
                            std::span<const ModuleSystemBinding> bindings) {
    if (graph.size() != 0U || graph.compiled()) {
        return bridgeFailure(foundation::ErrorCode::InvalidState,
                             "module system bridge requires an empty graph");
    }
    if (bindings.empty()) {
        return bridgeFailure(foundation::ErrorCode::InvalidArgument,
                             "module system bridge requires at least one binding");
    }
    if (registry.systemOrder().size() != registry.systems().size()) {
        return bridgeFailure(foundation::ErrorCode::InvalidState,
                             "module registry must be finalized before execution binding");
    }

    std::unordered_map<simulation::SystemId, const ModuleSystemBinding*> implementations;
    implementations.reserve(bindings.size());
    for (const ModuleSystemBinding& binding : bindings) {
        if (!binding.valid() || !implementations.emplace(binding.id, &binding).second) {
            return bridgeFailure(foundation::ErrorCode::InvalidArgument,
                                 "invalid or duplicate module system binding");
        }
        if (registry.findSystem(binding.id) == nullptr) {
            return bridgeFailure(foundation::ErrorCode::NotFound,
                                 "module system binding has no registered SystemSpec");
        }
    }

    std::size_t imported = 0U;
    for (const api::ApiId system_id : registry.systemOrder()) {
        const auto implementation = implementations.find(system_id);
        if (implementation == implementations.end()) {
            continue;
        }
        const api::ApiSystemDescriptor* registered = registry.findSystem(system_id);
        if (registered == nullptr) {
            return bridgeFailure(foundation::ErrorCode::Internal,
                                 "finalized module registry lost a system descriptor");
        }
        for (const execution::SystemId predecessor : registered->predecessors) {
            if (!implementations.contains(predecessor)) {
                return bridgeFailure(
                    foundation::ErrorCode::InvalidState,
                    "bound module system depends on an unbound predecessor");
            }
        }

        simulation::SystemDescriptor descriptor{};
        static_cast<execution::SystemSpec&>(descriptor) =
            static_cast<const execution::SystemSpec&>(*registered);
        descriptor.phase = implementation->second->phase;
        descriptor.cadence = implementation->second->cadence;
        descriptor.callback = implementation->second->callback;
        const auto added = graph.add(std::move(descriptor));
        if (!added) {
            graph.clear();
            return foundation::Result<simulation::SystemExecutionPlan,
                                      foundation::Error>::failure(added.error());
        }
        ++imported;
    }

    if (imported != bindings.size()) {
        graph.clear();
        return bridgeFailure(foundation::ErrorCode::NotFound,
                             "module execution binding was not present in system order");
    }
    const auto compiled = graph.compile();
    if (!compiled) {
        graph.clear();
        return foundation::Result<simulation::SystemExecutionPlan,
                                  foundation::Error>::failure(compiled.error());
    }
    return foundation::Result<simulation::SystemExecutionPlan,
                              foundation::Error>::success(graph.executionPlan());
}

} // namespace genomes::runtime
