#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/NativeWindow.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/input/InputFrame.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace genomes::platform {

struct WindowConfig final {
    std::string_view title{"Genomes"};
    std::int32_t width{1280};
    std::int32_t height{720};
    bool resizable{true};
    bool vulkan{true};

    [[nodiscard]] bool valid() const noexcept {
        return width >= 320 && width <= 16'384 && height >= 200 && height <= 16'384;
    }
};

struct PlatformFrame final {
    input::InputFrame input{};
    bool quit_requested{false};
    bool resized{false};
    std::int32_t width{0};
    std::int32_t height{0};
};

class SdlPlatform final {
public:
    static foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>
    create(WindowConfig config);

    ~SdlPlatform();

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    [[nodiscard]] PlatformFrame poll_events() noexcept;
    [[nodiscard]] foundation::NativeWindowHandle native_window() const noexcept;
    [[nodiscard]] std::int32_t width() const noexcept { return width_; }
    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    void shutdown() noexcept;

private:
    struct Impl;

    SdlPlatform(WindowConfig config, void* window) noexcept;

    WindowConfig config_{};
    void* window_{nullptr};
    std::int32_t width_{0};
    std::int32_t height_{0};
    bool initialized_{false};
};

} // namespace genomes::platform
