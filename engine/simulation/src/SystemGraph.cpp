#include <genomes/simulation/SystemGraph.hpp>

#include <algorithm>
#include <exception>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace genomes::simulation {

namespace {

[[nodiscard]] bool contains(const std::vector<AccessKey>& values, AccessKey key) noexcept {
    return std::find(values.begin(), values.end(), key) != values.end();
}

[[nodiscard]] bool intersects(const std::vector<AccessKey>& left,
                              const std::vector<AccessKey>& right) noexcept {
    for (const AccessKey key : left) {
        if (contains(right, key)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasId(const std::vector<SystemId>& values, SystemId id) noexcept {
    return std::find(values.begin(), values.end(), id) != values.end();
}

[[nodiscard]] const char* phaseName(SystemPhase phase) noexcept {
    switch (phase) {
    case SystemPhase::InputCommands:
        return "InputCommands";
    case SystemPhase::Sense:
        return "Sense";
    case SystemPhase::Decide:
        return "Decide";
    case SystemPhase::Navigate:
        return "Navigate";
    case SystemPhase::MoveIntent:
        return "MoveIntent";
    case SystemPhase::PhysicsCommands:
        return "PhysicsCommands";
    case SystemPhase::PhysicsStep:
        return "PhysicsStep";
    case SystemPhase::CombatBallistics:
        return "CombatBallistics";
    case SystemPhase::DamageDestruction:
        return "DamageDestruction";
    case SystemPhase::Commit:
        return "Commit";
    case SystemPhase::PresentationExtract:
        return "PresentationExtract";
    }
    return "Unknown";
}

[[nodiscard]] foundation::Error graphError(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

} // namespace

bool SystemDescriptor::valid() const noexcept {
    if (main_thread_only && lane != jobs::ExecutionLane::Worker &&
        lane != jobs::ExecutionLane::Main) {
        return false;
    }
    const auto valid_legacy_ids = [this](const std::vector<SystemId>& values) noexcept {
        return std::all_of(values.begin(), values.end(), [this](SystemId value) {
            return value != 0U && value != id;
        });
    };
    return execution::SystemSpec::valid() && cadence.valid() &&
           static_cast<bool>(callback) && valid_legacy_ids(before) &&
           valid_legacy_ids(after);
}

foundation::Result<void, foundation::Error> SystemGraph::add(SystemDescriptor descriptor) {
    if (!descriptor.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            graphError("invalid simulation system descriptor"));
    }

    // Normalize legacy scheduling fields exactly once at the graph boundary.
    // From this point onward the inherited execution::SystemSpec is the sole
    // source of dependency and lane semantics.
    if (descriptor.main_thread_only) {
        descriptor.lane = jobs::ExecutionLane::Main;
    }
    for (const SystemId predecessor : descriptor.after) {
        if (!hasId(descriptor.predecessors, predecessor)) {
            descriptor.predecessors.push_back(predecessor);
        }
    }
    std::sort(descriptor.predecessors.begin(), descriptor.predecessors.end());
    descriptor.predecessors.erase(
        std::unique(descriptor.predecessors.begin(), descriptor.predecessors.end()),
        descriptor.predecessors.end());
    descriptor.main_thread_only = false;
    descriptor.after.clear();

    if (!static_cast<const execution::SystemSpec&>(descriptor).valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            graphError("invalid canonical simulation system specification"));
    }
    if (findSystem(descriptor.id) != nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            graphError("duplicate simulation system id"));
    }
    systems_.push_back(std::move(descriptor));
    compiled_ = false;
    diagnostic_.clear();
    return foundation::Result<void, foundation::Error>::success();
}

void SystemGraph::clear() noexcept {
    systems_.clear();
    graph_.clear();
    diagnostic_.clear();
    compiled_ = false;
}

bool SystemGraph::hasHazard(const execution::SystemSpec& left,
                            const execution::SystemSpec& right) const noexcept {
    const bool component_hazard =
        intersects(left.access.writes, right.access.reads) ||
        intersects(left.access.writes, right.access.writes) ||
        intersects(right.access.writes, left.access.reads);
    const bool resource_hazard =
        intersects(left.access.resource_writes, right.access.resource_reads) ||
        intersects(left.access.resource_writes, right.access.resource_writes) ||
        intersects(right.access.resource_writes, left.access.resource_reads);
    return component_hazard || resource_hazard;
}

bool SystemGraph::addEdge(std::size_t from, std::size_t to) noexcept {
    if (from == to || from >= graph_.size() || to >= graph_.size()) {
        return false;
    }
    auto& successors = graph_[from].successors;
    if (std::find(successors.begin(), successors.end(), to) != successors.end()) {
        return true;
    }
    successors.push_back(to);
    ++graph_[to].indegree;
    return true;
}

const SystemDescriptor* SystemGraph::findSystem(SystemId id) const noexcept {
    const auto iterator = std::find_if(systems_.begin(), systems_.end(),
                                       [id](const SystemDescriptor& descriptor) {
                                           return descriptor.id == id;
                                       });
    return iterator == systems_.end() ? nullptr : &*iterator;
}

void SystemGraph::fail(std::string message) {
    diagnostic_ = std::move(message);
    graph_.clear();
    compiled_ = false;
}

foundation::Result<void, foundation::Error> SystemGraph::compile() {
    graph_.clear();
    diagnostic_.clear();
    compiled_ = false;
    graph_.reserve(systems_.size());
    for (const SystemDescriptor& descriptor : systems_) {
        graph_.push_back({descriptor, {}, 0, {}});
    }

    std::unordered_map<SystemId, std::size_t> indices;
    indices.reserve(systems_.size());
    for (std::size_t index = 0; index < systems_.size(); ++index) {
        indices.emplace(systems_[index].id, index);
    }

    for (std::size_t descriptor_index = 0; descriptor_index < graph_.size();
         ++descriptor_index) {
        const SystemDescriptor& descriptor = graph_[descriptor_index].descriptor;
        const auto from_iterator = indices.find(descriptor.id);
        if (from_iterator == indices.end()) {
            fail("internal system graph index failure");
            return foundation::Result<void, foundation::Error>::failure(
                graphError("simulation system graph compile failed"));
        }
        const std::size_t from = from_iterator->second;
        for (const SystemId target_id : descriptor.before) {
            const auto target = indices.find(target_id);
            if (target == indices.end()) {
                fail("system '" + std::to_string(descriptor.id) +
                     "' references unknown before-system");
                return foundation::Result<void, foundation::Error>::failure(
                    graphError("simulation system graph references an unknown system"));
            }
            addEdge(from, target->second);
        }
        for (const SystemId source_id : descriptor.predecessors) {
            const auto source = indices.find(source_id);
            if (source == indices.end()) {
                fail("system '" + std::to_string(descriptor.id) +
                     "' references unknown after-system");
                return foundation::Result<void, foundation::Error>::failure(
                    graphError("simulation system graph references an unknown system"));
            }
            addEdge(source->second, from);
        }
    }

    for (std::size_t left = 0; left < systems_.size(); ++left) {
        for (std::size_t right = left + 1; right < systems_.size(); ++right) {
            const auto& left_descriptor = systems_[left];
            const auto& right_descriptor = systems_[right];
            // Phases are semantic labels, not implicit frontier barriers.
            // Only declared dependencies and read/write hazards impose an
            // edge; independent systems from different phases may overlap.
            if (hasHazard(left_descriptor, right_descriptor)) {
                const auto left_phase = static_cast<std::uint8_t>(left_descriptor.phase);
                const auto right_phase = static_cast<std::uint8_t>(right_descriptor.phase);
                if (left_phase < right_phase ||
                    (left_phase == right_phase && left_descriptor.id < right_descriptor.id)) {
                    addEdge(left, right);
                } else {
                    addEdge(right, left);
                }
            }
        }
    }

    std::vector<std::size_t> indegrees;
    indegrees.reserve(graph_.size());
    for (const CompiledNode& node : graph_) {
        indegrees.push_back(node.indegree);
    }
    std::vector<std::size_t> ready;
    ready.reserve(graph_.size());
    for (std::size_t index = 0; index < indegrees.size(); ++index) {
        if (indegrees[index] == 0) {
            ready.push_back(index);
        }
    }

    std::size_t visited = 0;
    while (!ready.empty()) {
        std::sort(ready.begin(), ready.end(), [this](std::size_t left, std::size_t right) {
            return graph_[left].descriptor.id < graph_[right].descriptor.id;
        });
        const std::size_t node = ready.front();
        ready.erase(ready.begin());
        ++visited;
        for (const std::size_t successor : graph_[node].successors) {
            if (--indegrees[successor] == 0) {
                ready.push_back(successor);
            }
        }
    }

    if (visited != graph_.size()) {
        fail("simulation system dependency cycle detected");
        return foundation::Result<void, foundation::Error>::failure(
            graphError("simulation system dependency cycle detected"));
    }
    for (CompiledNode& node : graph_) {
        std::sort(node.successors.begin(), node.successors.end(), [this](std::size_t left,
                                                                         std::size_t right) {
            return graph_[left].descriptor.id < graph_[right].descriptor.id;
        });
    }
    compiled_ = true;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<SystemGraphRunResult, foundation::Error> SystemGraph::run(
    foundation::SimulationTick tick,
    double fixed_dt,
    jobs::JobSystem* jobs,
    CommandBufferSet* command_buffers) {
    // Keep the legacy entry point as a thin synchronous barrier over the same
    // dependency-aware JobGraph path used by production callers.  The former
    // implementation released systems only after a whole ready frontier had
    // completed, which defeated direct prerequisite scheduling.
    return executionPlan().run(tick, fixed_dt, jobs, command_buffers);
}

bool SystemGraph::setCadenceTier(SystemId id,
                                  CadenceTier tier,
                                  foundation::SimulationTick tick) noexcept {
    if (!compiled_) {
        return false;
    }
    const auto iterator = std::find_if(graph_.begin(), graph_.end(), [id](const CompiledNode& node) {
        return node.descriptor.id == id;
    });
    if (iterator == graph_.end()) {
        return false;
    }
    ::genomes::simulation::setCadenceTier(iterator->cadence_state, tier, tick);
    return true;
}

std::string SystemGraph::dump() const {
    std::ostringstream output;
    for (std::size_t index = 0; index < graph_.size(); ++index) {
        const auto& node = graph_[index];
        output << node.descriptor.id << " [" << phaseName(node.descriptor.phase) << "] ->";
        std::vector<SystemId> successors;
        successors.reserve(node.successors.size());
        for (const std::size_t successor : node.successors) {
            successors.push_back(graph_[successor].descriptor.id);
        }
        std::sort(successors.begin(), successors.end());
        for (const SystemId successor : successors) {
            output << ' ' << successor;
        }
        if (index + 1 < graph_.size()) {
            output << '\n';
        }
    }
    return output.str();
}

} // namespace genomes::simulation
