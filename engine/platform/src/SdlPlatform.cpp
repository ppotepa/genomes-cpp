#include <genomes/platform/Platform.hpp>

#include <SDL3/SDL.h>

#include <string>
#include <utility>

namespace genomes::platform {

namespace {

[[nodiscard]] foundation::Error sdl_error(foundation::ErrorCode code,
                                           const char* fallback) noexcept {
    const char* message = SDL_GetError();
    return {code, (message != nullptr && *message != '\0') ? message : fallback};
}

[[nodiscard]] SDL_Window* as_window(void* value) noexcept {
    return static_cast<SDL_Window*>(value);
}

} // namespace

struct SdlPlatform::Impl {};

foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>
SdlPlatform::create(WindowConfig config) {
    if (!config.valid()) {
        return foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid SDL window dimensions"});
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>::failure(
            sdl_error(foundation::ErrorCode::Internal, "SDL video initialization failed"));
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    SDL_Window* window = SDL_CreateWindow(
        std::string{config.title}.c_str(), config.width, config.height, flags);
    if (window == nullptr) {
        const foundation::Error error =
            sdl_error(foundation::ErrorCode::Internal, "SDL window creation failed");
        SDL_Quit();
        return foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>::failure(error);
    }

    auto platform = std::unique_ptr<SdlPlatform>{
        new SdlPlatform{config, static_cast<void*>(window)}};
    platform->initialized_ = true;
    return foundation::Result<std::unique_ptr<SdlPlatform>, foundation::Error>::success(
        std::move(platform));
}

SdlPlatform::SdlPlatform(WindowConfig config, void* window) noexcept
    : config_{config}, window_{window}, width_{config.width}, height_{config.height} {}

SdlPlatform::~SdlPlatform() {
    shutdown();
}

PlatformFrame SdlPlatform::poll_events() noexcept {
    PlatformFrame frame{};
    frame.width = width_;
    frame.height = height_;

    if (!initialized_ || window_ == nullptr) {
        frame.quit_requested = true;
        return frame;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            frame.quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            frame.resized = true;
            frame.width = event.window.data1;
            frame.height = event.window.data2;
            width_ = frame.width;
            height_ = frame.height;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat) {
                break;
            }
            switch (event.key.scancode) {
            case SDL_SCANCODE_UP:
                frame.input.up_pressed = true;
                break;
            case SDL_SCANCODE_DOWN:
                frame.input.down_pressed = true;
                break;
            case SDL_SCANCODE_LEFT:
                frame.input.left_pressed = true;
                break;
            case SDL_SCANCODE_RIGHT:
                frame.input.right_pressed = true;
                break;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
                frame.input.confirm_pressed = true;
                break;
            case SDL_SCANCODE_ESCAPE:
                frame.input.cancel_pressed = true;
                break;
            default:
                break;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                frame.input.mouse_left_pressed = true;
                frame.input.mouse_left_down = true;
                mouse_left_down_ = true;
                frame.input.mouse_x = event.button.x;
                frame.input.mouse_y = event.button.y;
                last_mouse_x_ = event.button.x;
                last_mouse_y_ = event.button.y;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                mouse_left_down_ = false;
                frame.input.mouse_x = event.button.x;
                frame.input.mouse_y = event.button.y;
                last_mouse_x_ = event.button.x;
                last_mouse_y_ = event.button.y;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            frame.input.mouse_x = event.motion.x;
            frame.input.mouse_y = event.motion.y;
            frame.input.mouse_delta_x += event.motion.xrel;
            frame.input.mouse_delta_y += event.motion.yrel;
            last_mouse_x_ = event.motion.x;
            last_mouse_y_ = event.motion.y;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            frame.input.mouse_wheel_y += event.wheel.y;
            break;
        default:
            break;
        }
    }
    frame.input.mouse_left_down = mouse_left_down_;
    frame.input.mouse_x = last_mouse_x_;
    frame.input.mouse_y = last_mouse_y_;
    return frame;
}

foundation::NativeWindowHandle SdlPlatform::native_window() const noexcept {
    foundation::NativeWindowHandle handle{};
    if (!initialized_ || window_ == nullptr) {
        return handle;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(as_window(window_));
#if defined(_WIN32)
    handle.system = foundation::NativeWindowSystem::Win32;
    handle.window = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    handle.display = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    handle.window_id = static_cast<std::uint64_t>(SDL_GetNumberProperty(
        properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    if (handle.display != nullptr && handle.window_id != 0) {
        handle.system = foundation::NativeWindowSystem::X11;
    } else {
        handle.system = foundation::NativeWindowSystem::Wayland;
        handle.display = SDL_GetPointerProperty(
            properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        handle.window = SDL_GetPointerProperty(
            properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#endif
    return handle;
}

void SdlPlatform::shutdown() noexcept {
    if (window_ != nullptr) {
        SDL_DestroyWindow(as_window(window_));
        window_ = nullptr;
    }
    if (initialized_) {
        SDL_Quit();
        initialized_ = false;
    }
}

} // namespace genomes::platform
