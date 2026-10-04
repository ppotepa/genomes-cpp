#include <genomes/jobs/SchedulerClient.hpp>

#include <genomes/jobs/JobGraph.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <utility>

namespace genomes::jobs {

SchedulerClient JobContext::scheduler() const noexcept {
    return SchedulerClient(system_);
}

JobHandle SchedulerClient::submit(JobFunction function, JobOptions options) const {
    return system_ != nullptr ? system_->submit(std::move(function), options) : JobHandle{};
}

void SchedulerClient::wait(const JobHandle& handle) const noexcept {
    if (system_ != nullptr) system_->wait(handle);
}

void SchedulerClient::wait(const JobCompletion& completion) const noexcept {
    if (system_ != nullptr) system_->wait(completion);
}

JobCompletion SchedulerClient::start(const JobGraph& graph) const {
    return system_ != nullptr ? graph.start(*system_) : JobCompletion{};
}

std::uint32_t SchedulerClient::workerCount() const noexcept {
    return system_ != nullptr ? system_->workerCount() : 0U;
}

SchedulerMode SchedulerClient::mode() const noexcept {
    return system_ != nullptr ? system_->mode() : SchedulerMode::Serial;
}

bool SchedulerClient::isWorkerThread() const noexcept {
    return system_ != nullptr && system_->isWorkerThread();
}

bool SchedulerClient::isCancellationRequested() const noexcept {
    return system_ == nullptr || system_->isCancellationRequested();
}

SchedulerTelemetry SchedulerClient::telemetry() const noexcept {
    return system_ != nullptr ? system_->telemetry() : SchedulerTelemetry{};
}

} // namespace genomes::jobs
