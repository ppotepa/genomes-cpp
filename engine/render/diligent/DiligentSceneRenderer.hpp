#pragma once

#include "DiligentBackend.hpp"
#include <genomes/render/PresentationSnapshot.hpp>
#include <cstddef>

namespace genomes::render {

class DiligentSceneRenderer final : public IRenderer {
public:
    explicit DiligentSceneRenderer(DiligentBackend& backend) noexcept : backend_{backend} {}
    void begin_frame() override;
    void submit(const PresentationSnapshot&, const ui::UiRenderFrame&) override;
    void end_frame() override;
    [[nodiscard]] RenderCapabilities capabilities() const noexcept override {
        return backend_.capabilities();
    }
    [[nodiscard]] RenderUploadTelemetry uploadTelemetry() const noexcept override {
        return backend_.uploadTelemetry();
    }
    [[nodiscard]] bool healthy() const noexcept { return healthy_; }
    [[nodiscard]] foundation::Error last_error() const noexcept { return last_error_; }
    [[nodiscard]] std::size_t submitted_instances() const noexcept { return submitted_instances_; }
    [[nodiscard]] std::size_t submitted_ui_nodes() const noexcept { return submitted_ui_nodes_; }
private:
    DiligentBackend& backend_;
    bool healthy_{true};
    bool frame_started_{false};
    foundation::Error last_error_{};
    std::size_t submitted_instances_{0};
    std::size_t submitted_ui_nodes_{0};
};

} // namespace genomes::render
