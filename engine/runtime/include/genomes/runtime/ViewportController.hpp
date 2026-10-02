#pragma once

#include <genomes/camera/CameraController.hpp>
#include <genomes/input/InputFrame.hpp>
#include <algorithm>
#include <array>
#include <cstdint>

namespace genomes::runtime {

// Owns one viewport's interactive camera and gesture lifetime, independently
// of scene payloads and the renderer. Input must already be filtered by the UI.
class ViewportController final {
public:
    void clear() noexcept { *this = {}; }
    void cancelGesture() noexcept { button_ = 0; pending_ = {}; }

    void configure(const camera::CameraRequest& declared, std::uint64_t revision) noexcept {
        if (!initialized_ || revision != revision_ || declared.mode != request_.mode ||
            declared.mode == camera::CameraMode::Fixed) {
            request_ = declared;
            controller_.reset(request_);
            cancelGesture();
            initialized_ = true;
        } else {
            // Layout/lens changes must not overwrite the user's orbit or pan.
            request_.lens = declared.lens;
            request_.viewport = declared.viewport;
            request_.up = declared.up;
            request_.rts = declared.rts;
            if (declared.mode == camera::CameraMode::RTS) controller_.updateHome(declared);
        }
        revision_ = revision;
    }

    void handleInput(const input::InputFrame& input) noexcept {
        if (!initialized_ || request_.mode == camera::CameraMode::Fixed) return;
        if (input.focus_lost || input.pointer_cancel || input.cancel_pressed) {
            cancelGesture();
            keys_.fill(false);
            return;
        }
        const float width = std::max(1.0F, request_.viewport.width * input.viewport_width);
        const float height = std::max(1.0F, request_.viewport.height * input.viewport_height);
        const float left = request_.viewport.x * input.viewport_width;
        const float top = request_.viewport.y * input.viewport_height;
        const auto inside = [&](float x, float y) {
            return x >= left && x < left + width && y >= top && y < top + height;
        };
        if ((button_ == 1 && !input.mouse_left_down) ||
            (button_ == 2 && !input.mouse_middle_down) ||
            (button_ == 3 && !input.mouse_right_down)) button_ = 0;
        for (const auto& event : input.events) {
            if (event.type == input::EventType::MouseButtonDown && button_ == 0 &&
                event.mouse_button >= 1 && event.mouse_button <= 3 && inside(event.x, event.y))
                button_ = event.mouse_button;
            if (event.type == input::EventType::MouseButtonUp && event.mouse_button == button_)
                button_ = 0;
        }
        if (input.events.empty() && input.mouse_left_pressed && inside(input.mouse_x, input.mouse_y))
            button_ = 1;
        const bool held = button_ == 1 ? input.mouse_left_down :
                          button_ == 2 ? input.mouse_middle_down :
                          button_ == 3 && input.mouse_right_down;
        if (!held) button_ = 0;
        if (request_.mode == camera::CameraMode::RTS && button_ == 3) {
            pending_.orbit_x += input.mouse_delta_x / height * 4.0F;
            pending_.orbit_y += input.mouse_delta_y / height * 4.0F;
        } else if (request_.mode == camera::CameraMode::Orbit && button_ == 1) {
            pending_.orbit_x += input.mouse_delta_x / height * 4.0F;
            pending_.orbit_y += input.mouse_delta_y / height * 4.0F;
        } else if ((request_.mode == camera::CameraMode::RTS && button_ == 2) ||
                   (request_.mode == camera::CameraMode::Orbit && button_ != 0)) {
            // Both axes use viewport height: equal pixel drags have equal scale.
            pending_.pan_x += input.mouse_delta_x / height;
            pending_.pan_y += input.mouse_delta_y / height;
        }
        if (inside(input.mouse_x, input.mouse_y)) pending_.zoom -= input.mouse_wheel_y * .12F;
        pending_.reset |= input.reset_pressed;
        for (const auto& event : input.events) {
            if (event.type != input::EventType::KeyDown && event.type != input::EventType::KeyUp) continue;
            const bool down = event.type == input::EventType::KeyDown;
            switch (event.scancode) {
            case 4: keys_[0] = down; break;  // A
            case 7: keys_[1] = down; break;  // D
            case 22: keys_[2] = down; break; // S
            case 26: keys_[3] = down; break; // W
            case 80: keys_[4] = down; break; // Left
            case 79: keys_[5] = down; break; // Right
            case 81: keys_[6] = down; break; // Down
            case 82: keys_[7] = down; break; // Up
            default: break;
            }
        }
        pending_.move_x = static_cast<float>(keys_[1] || keys_[5]) -
                          static_cast<float>(keys_[0] || keys_[4]);
        pending_.move_z = static_cast<float>(keys_[3] || keys_[7]) -
                          static_cast<float>(keys_[2] || keys_[6]);
        if (request_.mode == camera::CameraMode::RTS && request_.rts.edge_scroll &&
            !input.pointer_over_ui && inside(input.mouse_x,input.mouse_y)) {
            const float edge=request_.rts.edge_scroll_pixels;
            pending_.move_x += static_cast<float>(input.mouse_x >= left+width-edge) -
                               static_cast<float>(input.mouse_x < left+edge);
            pending_.move_z += static_cast<float>(input.mouse_y < top+edge) -
                               static_cast<float>(input.mouse_y >= top+height-edge);
        }
    }

    [[nodiscard]] camera::CameraRequest update(float dt) noexcept {
        controller_.update(request_, pending_, dt);
        pending_ = {};
        return request_;
    }

private:
    camera::CameraController controller_{};
    camera::CameraRequest request_{};
    camera::CameraInput pending_{};
    std::uint64_t revision_{0};
    int button_{0};
    std::array<bool, 8> keys_{};
    bool initialized_{false};
};

} // namespace genomes::runtime
