#include "ThreeppInputAdapter.hpp"

#include <threepp/math/Vector2.hpp>

#include <algorithm>

namespace genomes::render {

void ThreeppInputAdapter::setViewport(int left, int top, int width, int height) noexcept {
    left_ = std::max(0, left);
    top_ = std::max(0, top);
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

threepp::WindowSize ThreeppInputAdapter::size() const {
    return {width_, height_};
}

bool ThreeppInputAdapter::contains(float x, float y) const noexcept {
    return x >= static_cast<float>(left_) && y >= static_cast<float>(top_) &&
           x < static_cast<float>(left_ + width_) &&
           y < static_cast<float>(top_ + height_);
}

threepp::Vector2 ThreeppInputAdapter::local(float x, float y) const noexcept {
    return {x - static_cast<float>(left_), y - static_cast<float>(top_)};
}

void ThreeppInputAdapter::feed(const input::InputFrame& frame) {
    const auto slot = [](int button) noexcept -> std::size_t {
        return button > 0 ? static_cast<std::size_t>(button - 1) : 0U;
    };
    const auto any_down = [this]() noexcept {
        return std::any_of(buttons_down_.begin(), buttons_down_.end(),
                           [](bool value) { return value; });
    };

    for (const auto& event : frame.events) {
        switch (event.type) {
        case input::EventType::MouseButtonDown: {
            const auto index = slot(event.mouse_button);
            if (index >= buttons_down_.size() || !contains(event.x, event.y)) break;
            buttons_down_[index] = true;
            onMousePressedEvent(static_cast<int>(index), local(event.x, event.y),
                                MouseAction::PRESS);
            break;
        }
        case input::EventType::MouseButtonUp: {
            const auto index = slot(event.mouse_button);
            if (index >= buttons_down_.size() || !buttons_down_[index]) break;
            onMousePressedEvent(static_cast<int>(index), local(event.x, event.y),
                                MouseAction::RELEASE);
            buttons_down_[index] = false;
            break;
        }
        case input::EventType::MouseMove:
            if (contains(event.x, event.y) || any_down()) {
                onMouseMoveEvent(local(event.x, event.y));
            }
            break;
        case input::EventType::MouseWheel:
            if (contains(frame.mouse_x, frame.mouse_y)) {
                onMouseWheelEvent({event.wheel_x, event.wheel_y});
            }
            break;
        case input::EventType::KeyDown:
        case input::EventType::KeyUp:
        case input::EventType::TextInput:
        case input::EventType::WindowResize:
        case input::EventType::TextInputStart:
        case input::EventType::TextInputStop:
            break;
        }
    }
}

} // namespace genomes::render
