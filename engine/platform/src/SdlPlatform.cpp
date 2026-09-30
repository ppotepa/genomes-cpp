#include <genomes/platform/Platform.hpp>

#include <SDL3/SDL.h>

#include <atomic>
#include <cassert>
#include <string>
#include <utility>

namespace genomes::platform {
namespace {
// SDL's event queue is process-wide. This is an OS resource lease, not a
// gameplay service. The first migration profile has one
// live Genomes window; sequential reopen is supported after renderer teardown.
std::atomic_flag window_lease = ATOMIC_FLAG_INIT;

[[nodiscard]] foundation::Error sdl_error(foundation::ErrorCode code,
                                         const char* operation) noexcept {
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s: %s", operation, SDL_GetError());
    // Error::message is string_view. Never return SDL's mutable error buffer or
    // a temporary std::string that will die during create() failure cleanup.
    return {code, operation};
}
[[nodiscard]] SDL_Window* as_window(void* value) noexcept {
    return static_cast<SDL_Window*>(value);
}
} // namespace

foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>
SdlPlatform::create(WindowConfig config) {
    using CreateResult = foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>;
    if (!config.valid()) {
        return CreateResult::failure({foundation::ErrorCode::InvalidArgument,
                                      "invalid SDL window configuration"});
    }
#if !defined(_WIN32)
    if (config.graphicsApi() == WindowGraphicsApi::NativeD3D) {
        return CreateResult::failure({foundation::ErrorCode::Unsupported,
                                      "NativeD3D windows require Windows"});
    }
#endif
    auto platform = std::unique_ptr<SdlPlatform>(new SdlPlatform(config, nullptr));
    if (window_lease.test_and_set(std::memory_order_acquire)) {
        return CreateResult::failure({foundation::ErrorCode::InvalidState,
                                      "only one live Genomes SDL window is supported"});
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        const auto error = sdl_error(foundation::ErrorCode::Internal, "SDL video initialization failed");
        window_lease.clear(std::memory_order_release);
        return CreateResult::failure(error);
    }
    platform->initialized_ = true;
    // The candidate now owns the lease and video subsystem on every exit path.
    const auto fail = [](const char* operation) {
        return CreateResult::failure(sdl_error(foundation::ErrorCode::Internal, operation));
    };
    if (!SDL_IsMainThread()) {
        return CreateResult::failure({foundation::ErrorCode::InvalidState,
                                      "SDL window must be created on the main thread"});
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (config.graphicsApi() == WindowGraphicsApi::Vulkan) flags |= SDL_WINDOW_VULKAN;
    const std::string title(config.title);
    SDL_Window* window = SDL_CreateWindow(title.c_str(), config.width, config.height, flags);
    if (window == nullptr) return fail("SDL window creation failed");
    platform->window_ = window;
    if (!platform->refresh_metrics()) {
        return CreateResult::failure(sdl_error(foundation::ErrorCode::Internal,
                                               "SDL initial drawable size query failed"));
    }
    return CreateResult::success(std::move(platform));
}

SdlPlatform::SdlPlatform(WindowConfig config, void* window) noexcept
    : config_(config), window_(window),
      owner_thread_(std::this_thread::get_id()) {}
SdlPlatform::~SdlPlatform() { shutdown(); }

bool SdlPlatform::on_owner_thread() const noexcept {
    return std::this_thread::get_id() == owner_thread_;
}
bool SdlPlatform::refresh_metrics() noexcept {
    int width = 0, height = 0, pixels_x = 0, pixels_y = 0;
    auto* window = as_window(window_);
    if (window == nullptr || !SDL_GetWindowSize(window, &width, &height) ||
        !SDL_GetWindowSizeInPixels(window, &pixels_x, &pixels_y)) return false;
    metrics_ = {width, height, pixels_x, pixels_y,
                (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0};
    return true;
}

PlatformFrame SdlPlatform::poll_events() {
    PlatformFrame frame{};
    if (!initialized_ || window_ == nullptr || !on_owner_thread()) {
        frame.quit_requested = true;
        frame.error = {foundation::ErrorCode::InvalidState, "SDL poll called without its owning window/thread"};
        return frame;
    }
    const WindowMetrics previous = metrics_;
    if (!refresh_metrics()) {
        frame.quit_requested = true;
        frame.error = sdl_error(foundation::ErrorCode::Internal, "SDL drawable size query failed");
        return frame;
    }
    const auto mouse_position = [this](float x, float y) {
        last_mouse_x_ = x;
        last_mouse_y_ = y;
        return metrics_.to_pixels(x, y);
    };
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            frame.quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            if (!refresh_metrics()) {
                frame.quit_requested = true;
                frame.error = sdl_error(foundation::ErrorCode::Internal, "SDL resize query failed");
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            frame.input.focus_lost = true;
            frame.input.pointer_cancel = true;
            frame.input.events.push_back({input::EventType::FocusLost, 0, 0, 0,
                0.0F, 0.0F, 0.0F, 0.0F, {}});
            frame.input.events.push_back({input::EventType::PointerCancel, 0, 0, 0,
                0.0F, 0.0F, 0.0F, 0.0F, {}});
            if (mouse_left_down_) {
                const auto p = metrics_.to_pixels(last_mouse_x_, last_mouse_y_);
                frame.input.events.push_back({input::EventType::MouseButtonUp, 0, 0,
                    SDL_BUTTON_LEFT, p.x, p.y, 0.0F, 0.0F, {}});
            }
            mouse_left_down_ = false;
            (void)SDL_CaptureMouse(false);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            frame.input.events.push_back({pressed ? input::EventType::KeyDown : input::EventType::KeyUp,
                static_cast<std::int32_t>(event.key.scancode), static_cast<std::int32_t>(event.key.key),
                0, 0.0F, 0.0F, 0.0F, 0.0F, {}});
            if (!pressed || event.key.repeat) break;
            switch (event.key.scancode) {
            case SDL_SCANCODE_UP: frame.input.up_pressed = true; break;
            case SDL_SCANCODE_DOWN: frame.input.down_pressed = true; break;
            case SDL_SCANCODE_LEFT: frame.input.left_pressed = true; break;
            case SDL_SCANCODE_RIGHT: frame.input.right_pressed = true; break;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER: frame.input.confirm_pressed = true; break;
            case SDL_SCANCODE_ESCAPE: frame.input.cancel_pressed = true; break;
            default: break;
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            frame.input.events.push_back({input::EventType::TextInput, 0, 0, 0,
                0.0F, 0.0F, 0.0F, 0.0F, event.text.text != nullptr ? event.text.text : ""});
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const auto p = mouse_position(event.button.x, event.button.y);
            const bool pressed = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            frame.input.events.push_back({pressed ? input::EventType::MouseButtonDown : input::EventType::MouseButtonUp,
                0, 0, event.button.button, p.x, p.y, 0.0F, 0.0F, {}});
            if (event.button.button == SDL_BUTTON_LEFT) {
                mouse_left_down_ = pressed;
                if (pressed) frame.input.mouse_left_pressed = true;
                (void)SDL_CaptureMouse(pressed);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            const auto p = mouse_position(event.motion.x, event.motion.y);
            const auto delta = metrics_.to_pixels(event.motion.xrel, event.motion.yrel);
            frame.input.events.push_back({input::EventType::MouseMove, 0, 0, 0,
                p.x, p.y, delta.x, delta.y, {}});
            frame.input.mouse_delta_x += delta.x;
            frame.input.mouse_delta_y += delta.y;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            const float sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
            const float x = event.wheel.x * sign, y = event.wheel.y * sign;
            // Wheel data is a scroll amount, never a framebuffer position.
            frame.input.events.push_back({input::EventType::MouseWheel, 0, 0, 0,
                x, y, x, y, {}});
            frame.input.mouse_wheel_y += y;
            break;
        }
        default: break;
        }
    }
    frame.metrics = metrics_;
    frame.width = metrics_.pixel_width;
    frame.height = metrics_.pixel_height;
    // Do not feed zero-sized swapchains to callers. The application checks
    // drawable(), and restoration emits a real resize.
    frame.resized = metrics_.drawable() && !(metrics_ == previous);
    if (frame.resized) {
        frame.input.events.push_back({input::EventType::WindowResize, 0, 0, 0,
            static_cast<float>(frame.width), static_cast<float>(frame.height), 0.0F, 0.0F, {}});
    }
    frame.input.mouse_left_down = mouse_left_down_;
    const auto p = metrics_.to_pixels(last_mouse_x_, last_mouse_y_);
    frame.input.mouse_x = p.x;
    frame.input.mouse_y = p.y;
    frame.input.viewport_width = static_cast<float>(frame.width);
    frame.input.viewport_height = static_cast<float>(frame.height);
    return frame;
}

foundation::NativeWindowHandle SdlPlatform::native_window() const noexcept {
    foundation::NativeWindowHandle handle{};
    if (!initialized_ || window_ == nullptr || !on_owner_thread()) return handle;
    const SDL_PropertiesID properties = SDL_GetWindowProperties(as_window(window_));
#if defined(_WIN32)
    handle.system = foundation::NativeWindowSystem::Win32;
    handle.window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    // The production Diligent path uses a native D3D12 window adapter.
    (void)properties;
#else
    handle.display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    handle.window_id = static_cast<std::uint64_t>(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    if (handle.display != nullptr && handle.window_id != 0) {
        handle.system = foundation::NativeWindowSystem::X11;
    } else {
        handle.system = foundation::NativeWindowSystem::Wayland;
        handle.display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        handle.window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#endif
    return handle;
}

void SdlPlatform::shutdown() noexcept {
    assert(on_owner_thread() && "destroy SDL resources on their owning thread");
    if (!on_owner_thread()) return;
    if (window_ != nullptr) {
        SDL_DestroyWindow(as_window(window_));
        window_ = nullptr;
    }
    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        initialized_ = false;
        window_lease.clear(std::memory_order_release);
    }
    metrics_ = {};
}

} // namespace genomes::platform
