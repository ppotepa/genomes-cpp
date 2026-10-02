#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace genomes::input {

enum class EventType : std::uint8_t {
    KeyDown,
    KeyUp,
    TextInput,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseWheel,
    WindowResize,
    TextInputStart,
    TextInputStop,
    FocusLost,
    PointerCancel
};

struct Event final {
    EventType type{EventType::KeyDown};
    std::int32_t scancode{0};
    std::int32_t keycode{0};
    std::int32_t mouse_button{0};
    float x{0.0F};
    float y{0.0F};
    float wheel_x{0.0F};
    float wheel_y{0.0F};
    std::string text;
    // Neutral modifier bitfield; bit 0 is Shift, bit 1 is Ctrl, bit 2 Alt.
    std::uint8_t modifiers{0};
};

struct InputFrame {
    bool up_pressed{false};
    bool down_pressed{false};
    bool left_pressed{false};
    bool right_pressed{false};
    bool confirm_pressed{false};
    bool cancel_pressed{false};
    bool reset_pressed{false};
    bool focus_lost{false};
    bool pointer_cancel{false};
    bool pointer_over_ui{false};
    bool mouse_left_pressed{false};
    bool mouse_left_down{false};
    bool mouse_middle_down{false};
    bool mouse_right_down{false};
    float mouse_x{0.0F};
    float mouse_y{0.0F};
    float mouse_delta_x{0.0F};
    float mouse_delta_y{0.0F};
    float mouse_wheel_y{0.0F};
    // Pointer coordinates and dimensions use framebuffer pixels, including DPI.
    float viewport_width{1280.0F};
    float viewport_height{720.0F};
    std::vector<Event> events;
};

} // namespace genomes::input
