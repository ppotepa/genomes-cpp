#include <genomes/simulation/SystemGraph.hpp>

#include <genomes/jobs/JobGraph.hpp>

#include <limits>
#include <mutex>
#include <optional>
#include <utility>

namespace genomes::simulation {

namespace {

[[nodiscard]] foundation::Error executionError(const char* message) {
    return {foundation::ErrorCode::Internal, message};
}

struct ExecutionFailure final {
    void record(SystemId id, foundation::Error value) {
        std::lock_guard lock(mutex);
        if (!error || id < failed_system) {
            failed_system = id;
            error = std::move(value);
        }
    }

    [[nodiscard]] bool failed() const {
        std::lock_guard lock(mutex);
        return error.has_value();
    }

    [[nodiscard]] std::optional<foundation::Error> take() {
        std::lock_guard lock(mutex);
        return std::move(error);
    }

    mutable std::mutex mutex;
    SystemId failed_system{std::numeric_limits<SystemId>::max()};
    std::optional<foundation::Error> error;
};

} // namespace

SystemExecutionPlan SystemGraph::executionPlan() noexcept {
    return SystemExecutionPlan(*this);
}

bool SystemExecutionPlan::valid() const noexcept {
    return graph_ != nullptr && graph_->compiled_;
}

jobs::JobCompletion SystemExecutionPlan::start(foundation::SimulationTick tick,
                                               double fixed_dt,
                                               jobs::JobSystem* jobs,
                                               CommandBufferSet* command_buffers,
                                               std::function<void()> deterministic_commit) const {
    if (!valid()) {
        return {};
    }
    if (jobs == nullptr) {
        return {};
    }
    jobs::JobSystem* scheduler = jobs;
    if (command_buffers != nullptr) {
        command_buffers->reset(graph_->graph_.size());
    }
    auto failure = std::make_shared<ExecutionFailure>();
    jobs::JobGraphBuilder builder;
    std::vector<jobs::JobGraphNode> nodes;
    nodes.reserve(graph_->graph_.size());
    for (std::size_t index = 0; index < graph_->graph_.size(); ++index) {
        auto& compiled = graph_->graph_[index];
        const auto decision = evaluateCadence(compiled.descriptor.cadence,
                                              compiled.cadence_state,
                                              compiled.descriptor.id,
                                              tick);
        const SystemDescriptor& descriptor = compiled.descriptor;
        CommandBuffer* commands = command_buffers == nullptr
                                      ? nullptr
                                      : &command_buffers->at(index);
        SystemContext context{tick, fixed_dt, decision.elapsed_ticks, compiled.descriptor.id,
                              descriptor.phase, scheduler, commands};
        jobs::JobOptions options;
        options.lane = compiled.descriptor.lane;
        options.work_class = jobs::WorkClass::Simulation;
        options.priority = jobs::JobPriority::Critical;
        nodes.push_back(builder.add(
            [callback = descriptor.callback, context, due = decision.due, failure](
                jobs::JobContext&) mutable {
                if (!due || failure->failed()) {
                    return;
                }
                try {
                    callback(context);
                } catch (...) {
                    failure->record(context.system,
                                    executionError("simulation system callback failed"));
                    throw;
                }
            },
            options));
    }
    for (std::size_t index = 0; index < graph_->graph_.size(); ++index) {
        for (const std::size_t successor : graph_->graph_[index].successors) {
            builder.precedes(nodes[index], nodes[successor]);
        }
    }
    if (deterministic_commit) {
        jobs::JobOptions options;
        options.lane = jobs::ExecutionLane::Main;
        options.work_class = jobs::WorkClass::Simulation;
        options.priority = jobs::JobPriority::Critical;
        const auto merge = builder.add(
            [commit = std::move(deterministic_commit)](jobs::JobContext&) mutable {
                commit();
            }, options);
        for (std::size_t index = 0; index < graph_->graph_.size(); ++index) {
            if (graph_->graph_[index].successors.empty()) {
                builder.precedes(nodes[index], merge);
            }
        }
    }
    auto job_graph = std::move(builder).build();
    return job_graph.start(*scheduler);
}

foundation::Result<SystemGraphRunResult, foundation::Error> SystemExecutionPlan::run(
    foundation::SimulationTick tick,
    double fixed_dt,
    jobs::JobSystem* jobs,
    CommandBufferSet* command_buffers) const {
    if (!valid()) {
        return foundation::Result<SystemGraphRunResult, foundation::Error>::failure(
            executionError("simulation execution plan is not valid"));
    }

    if (jobs == nullptr) {
        return foundation::Result<SystemGraphRunResult, foundation::Error>::failure(
            executionError("simulation execution requires the composition-root scheduler"));
    }
    jobs::JobSystem* scheduler = jobs;

    if (command_buffers != nullptr) {
        command_buffers->reset(graph_->graph_.size());
    }

    auto failure = std::make_shared<ExecutionFailure>();
    jobs::JobGraphBuilder builder;
    std::vector<jobs::JobGraphNode> nodes;
    nodes.reserve(graph_->graph_.size());
    std::size_t due_systems = 0U;
    for (std::size_t index = 0; index < graph_->graph_.size(); ++index) {
        auto& compiled = graph_->graph_[index];
        const auto decision = evaluateCadence(compiled.descriptor.cadence,
                                              compiled.cadence_state,
                                              compiled.descriptor.id,
                                              tick);
        const SystemDescriptor& descriptor = compiled.descriptor;
        if (decision.due) {
            ++due_systems;
        }
        CommandBuffer* commands = command_buffers == nullptr
                                      ? nullptr
                                      : &command_buffers->at(index);
        SystemContext context{tick,
                              fixed_dt,
                              decision.elapsed_ticks,
                              compiled.descriptor.id,
                              descriptor.phase,
                              scheduler,
                              commands};
        jobs::JobOptions options;
        options.lane = compiled.descriptor.lane;
        options.work_class = jobs::WorkClass::Simulation;
        options.priority = jobs::JobPriority::Critical;
        nodes.push_back(builder.add(
            [callback = descriptor.callback, context, due = decision.due, failure](
                jobs::JobContext&) mutable {
                if (!due || failure->failed()) {
                    return;
                }
                try {
                    callback(context);
                } catch (...) {
                    failure->record(context.system,
                                    executionError("simulation system callback failed"));
                    throw;
                }
            },
            options));
    }

    for (std::size_t index = 0; index < graph_->graph_.size(); ++index) {
        for (const std::size_t successor : graph_->graph_[index].successors) {
            builder.precedes(nodes[index], nodes[successor]);
        }
    }

    auto job_graph = std::move(builder).build();
    auto group = job_graph.run(*scheduler);
    group.wait();
    if (const auto error = failure->take(); error) {
        if (command_buffers != nullptr) {
            command_buffers->reset(0);
        }
        return foundation::Result<SystemGraphRunResult, foundation::Error>::failure(*error);
    }
    if (group.failed()) {
        if (command_buffers != nullptr) {
            command_buffers->reset(0);
        }
        return foundation::Result<SystemGraphRunResult, foundation::Error>::failure(
            executionError("simulation system job failed"));
    }

    return foundation::Result<SystemGraphRunResult, foundation::Error>::success(
        // Every due callback has completed successfully at this point. Keep
        // the legacy result fields meaningful: cadence no-ops contribute to
        // neither count, while each due system contributes to both the
        // planned and executed totals.
        SystemGraphRunResult{due_systems, due_systems});
}

} // namespace genomes::simulation
