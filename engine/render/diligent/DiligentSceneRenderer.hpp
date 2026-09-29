#pragma once

#include "DiligentBackend.hpp"

#include <genomes/render/PresentationSnapshot.hpp>

#include <cstddef>

namespace genomes::render {

// Bridges the scene presentation contract to the Diligent frame lifecycle.
// Scenes provide semantic instances and UI nodes while the backend owns the
// first debug world-map pass, shaders, buffers and draw submission.
class DiligentSceneRenderer final : public IRenderer {
public:
    explicit DiligentSceneRenderer(DiligentBackend& backend) noexcept
        : backend_{backend} {}

    void begin_frame() override;
    void submit(const PresentationSnapshot&, const ui::UiRenderFrame&) override;
    void end_frame() override;

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override {
        return backend_.capabilities();
    }

    [[nodiscard]] bool healthy() const noexcept override { return healthy_; }
    [[nodiscard]] foundation::Error last_error() const noexcept override { return last_error_; }
    [[nodiscard]] std::size_t submitted_instances() const noexcept {
        return submitted_instances_;
    }
    [[nodiscard]] std::size_t submitted_ui_nodes() const noexcept {
        return submitted_ui_nodes_;
    }

private:
    DiligentBackend& backend_;
    bool healthy_{true};
    foundation::Error last_error_{};
    std::size_t submitted_instances_{0};
    std::size_t submitted_ui_nodes_{0};
};

} // namespace genomes::render
