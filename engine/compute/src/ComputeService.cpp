#include <genomes/compute/ComputeService.hpp>

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

    std::vector<jobs::JobHandle> handles;
    handles.reserve((dispatch.element_count + dispatch.grain_size - 1) /
                    dispatch.grain_size);
    for (std::size_t begin = 0; begin < dispatch.element_count; begin += dispatch.grain_size) {
        const std::size_t end = std::min(dispatch.element_count, begin + dispatch.grain_size);
        handles.push_back(jobs_->submit([&function, begin, end](jobs::JobContext&) {
            function(begin, end);
        }));
    }
    bool failed = false;
    for (const jobs::JobHandle& handle : handles) {
        jobs_->wait(handle);
        failed = failed || handle.failed() || handle.wasCanceled();
    }
    if (failed) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "CPU compute worker failed"});
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::compute
