#pragma once

namespace genomes::input {

struct InputFrame {
    bool up_pressed{false};
    bool down_pressed{false};
    bool left_pressed{false};
    bool right_pressed{false};
    bool confirm_pressed{false};
    bool cancel_pressed{false};
    bool mouse_left_pressed{false};
    bool mouse_left_down{false};
    float mouse_x{0.0F};
    float mouse_y{0.0F};
    float mouse_delta_x{0.0F};
    float mouse_delta_y{0.0F};
    float mouse_wheel_y{0.0F};
};

} // namespace genomes::input
