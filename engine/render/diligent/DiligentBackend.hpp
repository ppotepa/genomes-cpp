#pragma once

#include <genomes/render/RenderBackend.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiDocument.hpp>
#include <cstdint>
#include <memory>

namespace genomes::render {

class DiligentBackend final : public RenderBackend {
public:
    static foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>
    create(RenderConfig config);
    static foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>
    create(RenderConfig config, foundation::NativeWindowHandle native_window);
    ~DiligentBackend() override;
    DiligentBackend(const DiligentBackend&) = delete;
    DiligentBackend& operator=(const DiligentBackend&) = delete;
    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderUploadTelemetry uploadTelemetry() const noexcept;
    [[nodiscard]] RenderResult begin_frame() noexcept override;
    [[nodiscard]] RenderResult draw_meshes(const PresentationSnapshot&) noexcept;
    [[nodiscard]] RenderResult draw_instances(const PresentationSnapshot&) noexcept;
    [[nodiscard]] RenderResult draw_ui(const ui::UiDocument&) noexcept;
    [[nodiscard]] RenderResult end_frame() noexcept override;
    [[nodiscard]] RenderResult resize(std::uint32_t width, std::uint32_t height) noexcept;
    [[nodiscard]] RenderResult wait_idle() noexcept override;
    void shutdown() noexcept override;
private:
    struct Impl;
    explicit DiligentBackend(std::unique_ptr<Impl> impl) noexcept;
    [[nodiscard]] RenderResult recreate_depth_buffer() noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace genomes::render
