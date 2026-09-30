#pragma once
#include "DiligentBackend.hpp"
#include <genomes/render/RenderFrameTransaction.hpp>
#include <cstddef>

namespace genomes::render {
class DiligentSceneRenderer final : public IRenderer {
public:
    explicit DiligentSceneRenderer(DiligentBackend& backend) noexcept : backend_(backend) {}
    void begin_frame() override;
    void submit(const PresentationSnapshot&,const ui::UiRenderFrame&) override;
    void end_frame() override;
    [[nodiscard]] RenderCapabilities capabilities() const noexcept override { return backend_.capabilities(); }
    [[nodiscard]] RenderUploadTelemetry uploadTelemetry() const noexcept override { return backend_.uploadTelemetry(); }
    [[nodiscard]] bool healthy() const noexcept override { return frame_.healthy(); }
    [[nodiscard]] foundation::Error last_error() const noexcept override { return frame_.error(); }
    [[nodiscard]] RenderResult capture(const std::filesystem::path& path) override { return backend_.capture(path); }
    [[nodiscard]] std::size_t submitted_instances() const noexcept { return instances_; }
    [[nodiscard]] std::size_t submitted_ui_nodes() const noexcept { return ui_nodes_; }
private:
    DiligentBackend& backend_;
    RenderFrameTransaction frame_;
    std::size_t instances_{0},ui_nodes_{0};
};
} // namespace genomes::render
