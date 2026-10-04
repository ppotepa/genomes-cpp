#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/execution/SystemSpec.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/Cadence.hpp>
#include <genomes/simulation/SimulationCommand.hpp>
#include <genomes/simulation/SystemId.hpp>

#include <cstddef>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace genomes::simulation {

class SystemExecutionPlan;

using AccessKey = execution::AccessKey;
using SystemAccess = execution::SystemAccess;

// The phase order is part of the simulation contract. New systems should be
// placed in the narrowest phase that contains their authoritative writes.
enum class SystemPhase : std::uint8_t {
    InputCommands = 0,
    Sense,
    Decide,
    Navigate,
    MoveIntent,
    PhysicsCommands,
    PhysicsStep,
    CombatBallistics,
    DamageDestruction,
    Commit,
    PresentationExtract,
};

struct SystemContext final {
    foundation::SimulationTick tick{};
    double fixed_dt{0.0};
    std::uint64_t cadence_elapsed_ticks{0};
    SystemId system{0};
    SystemPhase phase{SystemPhase::Commit};
    jobs::JobSystem* jobs{nullptr};
    CommandBuffer* commands{nullptr};
};

using SystemCallback = std::function<void(SystemContext&)>;

struct SystemDescriptor final : execution::SystemSpec {
    SystemPhase phase{SystemPhase::Commit};
    CadencePolicy cadence{};

    // Compatibility-only inputs. SystemGraph::add() normalizes these into the
    // inherited canonical predecessors/lane fields before the graph stores the
    // descriptor. New production systems should not write these fields.
    bool main_thread_only{false};
    std::vector<SystemId> before;
    std::vector<SystemId> after;
    SystemCallback callback;

    [[nodiscard]] bool valid() const noexcept;
};

struct SystemGraphRunResult final {
    std::size_t systems_run{0};
    std::size_t parallel_batches{0};
};

// A frozen, deterministic dependency graph for one simulation owner. The
// graph owns no gameplay state; callbacks capture their domain module or use
// services passed through the surrounding runtime.
class SystemGraph final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> add(
        SystemDescriptor descriptor);
    [[nodiscard]] foundation::Result<void, foundation::Error> compile();

    void clear() noexcept;

    [[nodiscard]] bool compiled() const noexcept { return compiled_; }
    [[nodiscard]] std::size_t size() const noexcept { return systems_.size(); }
    [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }

    [[nodiscard]] foundation::Result<SystemGraphRunResult, foundation::Error> run(
        foundation::SimulationTick tick,
        double fixed_dt,
        jobs::JobSystem* jobs = nullptr,
        CommandBufferSet* command_buffers = nullptr);

    [[nodiscard]] SystemExecutionPlan executionPlan() noexcept;

    bool setCadenceTier(SystemId,
                        CadenceTier,
                        foundation::SimulationTick) noexcept;

    // Stable graph dump useful for diagnostics and later Tracy integration.
    [[nodiscard]] std::string dump() const;

private:
    friend class SystemExecutionPlan;
    struct CompiledNode final {
        SystemDescriptor descriptor;
        std::vector<std::size_t> successors;
        std::size_t indegree{0};
        CadenceState cadence_state{};
    };

    [[nodiscard]] bool hasHazard(const execution::SystemSpec& left,
                                 const execution::SystemSpec& right) const noexcept;
    bool addEdge(std::size_t from, std::size_t to) noexcept;
    [[nodiscard]] const SystemDescriptor* findSystem(SystemId id) const noexcept;
    void fail(std::string message);

    std::vector<SystemDescriptor> systems_;
    std::vector<CompiledNode> graph_;
    std::string diagnostic_;
    bool compiled_{false};
};

// Execution form of a compiled semantic SystemGraph. Dependencies are mapped
// directly to JobGraph edges, so a successor is released by the final direct
// prerequisite rather than by a whole-frontier barrier.
class SystemExecutionPlan final {
public:
    SystemExecutionPlan() = default;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] jobs::JobCompletion start(
        foundation::SimulationTick tick,
        double fixed_dt,
        jobs::JobSystem* jobs = nullptr,
        CommandBufferSet* command_buffers = nullptr,
        std::function<void()> deterministic_commit = {}) const;
    [[nodiscard]] foundation::Result<SystemGraphRunResult, foundation::Error> run(
        foundation::SimulationTick tick,
        double fixed_dt,
        jobs::JobSystem* jobs = nullptr,
        CommandBufferSet* command_buffers = nullptr) const;

private:
    friend class SystemGraph;
    explicit SystemExecutionPlan(SystemGraph& graph) noexcept : graph_(&graph) {}

    SystemGraph* graph_{nullptr};
};

} // namespace genomes::simulation
