#pragma once

#include <genomes/api/Api.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/simulation/SystemGraph.hpp>

#include <span>

namespace genomes::runtime {

// Runtime-only implementation data for a system whose scheduling semantics are
// already declared by a module's execution::SystemSpec. Deliberately absent:
// lane, predecessors and read/write access. Those can only come from the
// frozen ModuleRegistry.
struct ModuleSystemBinding final {
    simulation::SystemId id{0};
    simulation::SystemPhase phase{simulation::SystemPhase::Commit};
    simulation::CadencePolicy cadence{};
    simulation::SystemCallback callback;

    [[nodiscard]] bool valid() const noexcept {
        return id != 0U && cadence.valid() && static_cast<bool>(callback);
    }
};

class ModuleSystemBridge final {
public:
    [[nodiscard]] static foundation::Result<simulation::SystemExecutionPlan,
                                             foundation::Error>
    compile(simulation::SystemGraph& graph,
            const api::ModuleRegistry& registry,
            std::span<const ModuleSystemBinding> bindings);
};

} // namespace genomes::runtime
