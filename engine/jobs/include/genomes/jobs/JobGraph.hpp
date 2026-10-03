#pragma once

#include <genomes/jobs/JobGroup.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace genomes::jobs {

class JobSystem;

using JobGraphNode = std::size_t;

class JobGraph final {
public:
    JobGraph() = default;
    JobGraph(JobGraph&&) noexcept = default;
    JobGraph& operator=(JobGraph&&) noexcept = default;
    JobGraph(const JobGraph&) = delete;
    JobGraph& operator=(const JobGraph&) = delete;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] JobCompletion start(JobSystem& system) const;
    [[nodiscard]] JobGroup run(JobSystem& system) const;

private:
    friend class JobGraphBuilder;

    struct Node final {
        JobGroup::JobFunction function;
        JobOptions options;
        std::vector<JobGraphNode> continuations;
        std::size_t dependency_count{0};
    };

    explicit JobGraph(std::vector<Node> nodes) : nodes_(std::move(nodes)) {}

    std::vector<Node> nodes_;
};

class JobGraphBuilder final {
public:
    [[nodiscard]] JobGraphNode add(JobGroup::JobFunction function, JobOptions options = {});
    void precedes(JobGraphNode dependency, JobGraphNode continuation);
    [[nodiscard]] JobGraph build() &&;

private:
    std::vector<JobGraph::Node> nodes_;
};

} // namespace genomes::jobs
