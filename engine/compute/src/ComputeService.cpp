#include <genomes/compute/ComputeService.hpp>
#include <genomes/jobs/ParallelFor.hpp>

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

namespace genomes::compute {

foundation::Result<void, foundation::Error> CpuComputeService::dispatch(
    const ComputeDispatch& dispatch, const ComputeRange& function) {
    if (!dispatch.valid() || !function) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid compute dispatch"});
    }

    if (jobs_ == nullptr || jobs_->workerCount() == 0 ||
        dispatch.element_count <= dispatch.grain_size) {
        try {
            function(0, dispatch.element_count);
        } catch (...) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "CPU compute range raised an exception"});
        }
        return foundation::Result<void, foundation::Error>::success();
    }

    const bool completed = jobs::parallelForAndWait(
        *jobs_, 0U, dispatch.element_count, dispatch.grain_size,
        [&function](const jobs::BatchRange& range) { function(range.begin, range.end); });
    if (!completed) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "CPU compute worker failed"});
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::compute
