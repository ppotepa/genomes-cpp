#pragma once

#include <genomes/render/RenderBackend.hpp>

namespace genomes::render {

class NullRenderBackend final : public RenderBackend {
public:
    explicit NullRenderBackend(RenderConfig config = {}) noexcept
        : config_(config), capabilities_{RenderBackendKind::Null, true,
                                         config.headless, false, false} {}

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override {
        return capabilities_;
    }

    [[nodiscard]] RenderResult begin_frame() noexcept override {
        if (!capabilities_.initialized) {
            return RenderResult::failure({foundation::ErrorCode::InvalidState,
                                          "null renderer is shut down"});
        }
        if (frame_open_) {
            return RenderResult::failure({foundation::ErrorCode::InvalidState,
                                          "null renderer frame is already open"});
        }
        frame_open_ = true;
        return RenderResult::success();
    }

    [[nodiscard]] RenderResult end_frame() noexcept override {
        if (!frame_open_) {
            return RenderResult::failure({foundation::ErrorCode::InvalidState,
                                          "null renderer frame is not open"});
        }
        frame_open_ = false;
        return RenderResult::success();
    }

    [[nodiscard]] RenderResult wait_idle() noexcept override {
        return RenderResult::success();
    }

    void shutdown() noexcept override {
        frame_open_ = false;
        capabilities_.initialized = false;
    }

private:
    RenderConfig config_{};
    RenderCapabilities capabilities_{};
    bool frame_open_{false};
};

} // namespace genomes::render
