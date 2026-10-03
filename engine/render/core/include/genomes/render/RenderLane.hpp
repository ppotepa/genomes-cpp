#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <thread>

namespace genomes::render {

class RenderLane final {
public:
    RenderLane() noexcept : owner_(std::this_thread::get_id()) {}

    [[nodiscard]] bool ownsCurrentThread() const noexcept {
        return std::this_thread::get_id() == owner_;
    }
    [[nodiscard]] foundation::Result<void, foundation::Error> requireOwner() const noexcept {
        if (!ownsCurrentThread()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "render operation called outside the render lane"});
        }
        return foundation::Result<void, foundation::Error>::success();
    }

private:
    std::thread::id owner_;
};

} // namespace genomes::render
