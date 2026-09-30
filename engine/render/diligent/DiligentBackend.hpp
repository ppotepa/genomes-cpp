#pragma once
#include <genomes/render/RenderBackend.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiRuntime.hpp>
#include <filesystem>
#include <memory>
#include <utility>

namespace genomes::render {
class DiligentBackend final : public RenderBackend {
public:
    static foundation::Result<std::unique_ptr<DiligentBackend>,foundation::Error> create(RenderConfig);
    static foundation::Result<std::unique_ptr<DiligentBackend>,foundation::Error> create(RenderConfig,foundation::NativeWindowHandle);
    ~DiligentBackend() override;
    DiligentBackend(const DiligentBackend&)=delete;
    DiligentBackend& operator=(const DiligentBackend&)=delete;
    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderUploadTelemetry uploadTelemetry() const noexcept;
    [[nodiscard]] std::pair<std::uint32_t,std::uint32_t> framebufferSize() const noexcept;
    [[nodiscard]] RenderResult begin_frame() noexcept override;
    [[nodiscard]] RenderResult draw_meshes(const PresentationSnapshot&) noexcept;
    [[nodiscard]] RenderResult draw_instances(const PresentationSnapshot&) noexcept;
    [[nodiscard]] RenderResult draw_ui(const ui::UiRenderFrame&) noexcept;
    [[nodiscard]] RenderResult end_frame() noexcept override;
    [[nodiscard]] RenderResult abort_frame() noexcept;
    [[nodiscard]] RenderResult resize(std::uint32_t,std::uint32_t) noexcept;
    [[nodiscard]] RenderResult wait_idle() noexcept override;
    [[nodiscard]] RenderResult capture(const std::filesystem::path&) noexcept;
    void set_camera(const RenderCamera&) noexcept;
    void shutdown() noexcept override;
private:
    struct Impl;
    explicit DiligentBackend(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace genomes::render
