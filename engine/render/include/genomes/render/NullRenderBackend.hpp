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

    [[nodiscard]] RendererHealthState healthState() const noexcept override {
        return health_.state;
    }

    [[nodiscard]] RendererHealthDiagnostics healthDiagnostics() const noexcept override {
        return health_;
    }

    [[nodiscard]] RenderResult begin_frame() noexcept override {
        if (!capabilities_.initialized) {
            health_.state = RendererHealthState::Stopped;
            health_.error = {foundation::ErrorCode::InvalidState,
                             "null renderer is shut down"};
            return RenderResult::failure(health_.error);
        }
        if (frame_open_) {
            return abortFrame({foundation::ErrorCode::InvalidState,
                               "null renderer frame is already open"});
        }
        frame_open_ = true;
        health_.state = RendererHealthState::Healthy;
        return RenderResult::success();
    }

    [[nodiscard]] RenderResult end_frame() noexcept override {
        if (!frame_open_) {
            return abortFrame({foundation::ErrorCode::InvalidState,
                               "null renderer frame is not open"});
        }
        frame_open_ = false;
        ++health_.last_successful_frame;
        health_.last_present_frame = health_.last_successful_frame;
        health_.state = RendererHealthState::Healthy;
        health_.error = {};
        return RenderResult::success();
    }

    [[nodiscard]] RenderResult wait_idle() noexcept override {
        if (health_.state == RendererHealthState::Stopped) {
            health_.error = {foundation::ErrorCode::InvalidState,
                             "null renderer is shut down"};
            return RenderResult::failure(health_.error);
        }
        return RenderResult::success();
    }

    void shutdown() noexcept override {
        frame_open_ = false;
        capabilities_.initialized = false;
        health_.state = RendererHealthState::Stopped;
    }

private:
    [[nodiscard]] RenderResult abortFrame(foundation::Error error) noexcept {
        health_.state = RendererHealthState::FrameAborted;
        health_.error = error;
        return RenderResult::failure(error);
    }

    RenderConfig config_{};
    RenderCapabilities capabilities_{};
    bool frame_open_{false};
    RendererHealthDiagnostics health_{};
};

} // namespace genomes::render
