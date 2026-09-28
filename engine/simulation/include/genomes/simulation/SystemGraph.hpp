#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/Cadence.hpp>
#include <genomes/simulation/SimulationCommand.hpp>
#include <genomes/simulation/SystemId.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace genomes::simulation {

using AccessKey = foundation::StableId;

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

struct SystemAccess final {
    std::vector<AccessKey> reads;
    std::vector<AccessKey> writes;
    std::vector<AccessKey> resource_reads;
    std::vector<AccessKey> resource_writes;

    [[nodiscard]] bool valid() const noexcept;
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

struct SystemDescriptor final {
    SystemId id{0};
    SystemPhase phase{SystemPhase::Commit};
    SystemAccess access{};
    CadencePolicy cadence{};
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

    bool setCadenceTier(SystemId,
                        CadenceTier,
                        foundation::SimulationTick) noexcept;

    // Stable graph dump useful for diagnostics and later Tracy integration.
    [[nodiscard]] std::string dump() const;

private:
    struct CompiledNode final {
        SystemDescriptor descriptor;
        std::vector<std::size_t> successors;
        std::size_t indegree{0};
        CadenceState cadence_state{};
    };

    [[nodiscard]] bool hasHazard(const SystemDescriptor& left,
                                 const SystemDescriptor& right) const noexcept;
    bool addEdge(std::size_t from, std::size_t to) noexcept;
    [[nodiscard]] const SystemDescriptor* findSystem(SystemId id) const noexcept;
    void fail(std::string message);

    std::vector<SystemDescriptor> systems_;
    std::vector<CompiledNode> graph_;
    std::string diagnostic_;
    bool compiled_{false};
};

} // namespace genomes::simulation
