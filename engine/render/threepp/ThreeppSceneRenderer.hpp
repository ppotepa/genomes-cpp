#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/input/InputFrame.hpp>

#include <filesystem>
#include <memory>

namespace genomes::platform { class SdlPlatform; }

namespace genomes::render {

// Production threepp/OpenGL implementation of the backend-neutral IRenderer
// contract. SDL owns the window and GL context; this adapter owns threepp scene
// objects and renderer resources only.
class ThreeppSceneRenderer final : public IRenderer {
public:
    [[nodiscard]] static foundation::Result<std::unique_ptr<ThreeppSceneRenderer>, foundation::Error>
    create(platform::SdlPlatform& platform);

    ~ThreeppSceneRenderer() override;
    ThreeppSceneRenderer(const ThreeppSceneRenderer&) = delete;
    ThreeppSceneRenderer& operator=(const ThreeppSceneRenderer&) = delete;

    void begin_frame() override;
    void submit(const PresentationSnapshot&, const ui::UiRenderFrame&) override;
    void end_frame() override;
    // Legacy adapter-local input hook; IRenderer no longer owns input.
    void handle_input(const input::InputFrame&);

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderUploadTelemetry uploadTelemetry() const noexcept override;
    [[nodiscard]] bool healthy() const noexcept override;
    [[nodiscard]] foundation::Error last_error() const noexcept override;

    [[nodiscard]] foundation::Result<void, foundation::Error> resize(
        std::uint32_t width, std::uint32_t height) noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> capture(
        const std::filesystem::path& path) noexcept override;
    void shutdown() noexcept;

private:
    struct Impl;
    explicit ThreeppSceneRenderer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace genomes::render
