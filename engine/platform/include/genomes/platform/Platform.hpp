#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/NativeWindow.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/input/InputFrame.hpp>
#include <genomes/platform/WindowMetrics.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>

namespace genomes::platform {

enum class WindowGraphicsApi : std::uint8_t { None, NativeD3D };

struct WindowConfig final {
    std::string_view title{"Genomes"};
    std::int32_t width{1280};
    std::int32_t height{720};
    bool resizable{true};
    std::optional<WindowGraphicsApi> graphics_api{};

    [[nodiscard]] WindowGraphicsApi graphicsApi() const noexcept {
        return graphics_api.value_or(WindowGraphicsApi::None);
    }
    [[nodiscard]] bool valid() const noexcept {
        const auto api = graphicsApi();
        return width >= 320 && width <= 16'384 && height >= 200 && height <= 16'384 &&
               (api == WindowGraphicsApi::None || api == WindowGraphicsApi::NativeD3D);
    }
};

struct PlatformFrame final {
    input::InputFrame input{};
    bool quit_requested{false};
    bool resized{false};
    // Legacy width/height accessors now consistently mean framebuffer pixels.
    std::int32_t width{0};
    std::int32_t height{0};
    WindowMetrics metrics{};
    foundation::Error error{};
};

class SdlPlatform final {
public:
    using OperationResult = foundation::Result<void, foundation::Error>;
    static foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>
    create(WindowConfig config);
    ~SdlPlatform();
    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    [[nodiscard]] PlatformFrame poll_events();
    [[nodiscard]] foundation::NativeWindowHandle native_window() const noexcept;
    [[nodiscard]] std::int32_t width() const noexcept { return metrics_.pixel_width; }
    [[nodiscard]] std::int32_t height() const noexcept { return metrics_.pixel_height; }
    [[nodiscard]] const WindowMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] WindowGraphicsApi graphics_api() const noexcept { return config_.graphicsApi(); }
    [[nodiscard]] bool on_owner_thread() const noexcept;
    // Opaque SDL window for application-owned SDL services such as the native
    // file dialog. The pointer is valid only while this platform is alive.
    [[nodiscard]] void* sdl_window() const noexcept { return window_; }
    // All methods and destruction run on the SDL/main thread. Dispose renderer
    // resources before this call; it destroys the native window only.
    void shutdown() noexcept;

private:
    SdlPlatform(WindowConfig config, void* window) noexcept;
    [[nodiscard]] bool refresh_metrics() noexcept;

    WindowConfig config_{};
    void* window_{nullptr};
    WindowMetrics metrics_{};
    std::thread::id owner_thread_;
    float last_mouse_x_{0.0F}; // window coordinates, not previously scaled pixels
    float last_mouse_y_{0.0F};
    bool mouse_left_down_{false};
    bool mouse_middle_down_{false};
    bool mouse_right_down_{false};
    bool initialized_{false};
};

} // namespace genomes::platform
