#include <genomes/runtime/ViewportController.hpp>
#include <cassert>

int main() {
    using namespace genomes;
    camera::CameraRequest home{};
    home.viewport = {.25F, 0.0F, .5F, 1.0F};
    runtime::ViewportController viewport;
    viewport.configure(home, 1);
    const auto close = [](math::Vec3 a, math::Vec3 b) {
        return math::length(a-b) < 1.0e-5F;
    };
    input::InputFrame drag{};
    drag.mouse_left_down = true;
    drag.mouse_x = 640; drag.mouse_y = 360;
    drag.mouse_delta_x = 80;
    // A press starting outside cannot steal capture when it enters the viewport.
    drag.events.push_back({input::EventType::MouseButtonDown, 0, 0, 1, 10, 360, 0, 0, {}});
    viewport.handleInput(drag);
    assert(close(viewport.update(.016F).position, home.position));
    drag.events.front().x = 640;
    viewport.handleInput(drag);
    const auto orbited = viewport.update(.016F);
    assert(!close(orbited.position, home.position));
    // An owned drag continues outside, then focus loss cancels it without reset.
    drag.events.clear(); drag.mouse_x = 1200;
    viewport.handleInput(drag);
    const auto outside = viewport.update(.016F);
    assert(!close(outside.position, orbited.position));
    drag.focus_lost = true;
    viewport.handleInput(drag);
    assert(close(viewport.update(.016F).position, outside.position));
    drag.focus_lost = false;
    viewport.handleInput(drag);
    assert(close(viewport.update(.016F).position, outside.position));
    // Republished scene defaults and viewport resize preserve the interactive pose.
    home.viewport.width = .6F;
    viewport.configure(home, 1);
    const auto resized = viewport.update(.016F);
    assert(close(resized.position, outside.position));
    assert(resized.viewport.width == .6F);
    // Right drag pans; a UI-consumed press (no down event) must not pan.
    input::InputFrame pan{};
    pan.mouse_x = 640; pan.mouse_y = 360;
    pan.mouse_right_down = true; pan.mouse_delta_x = 30;
    viewport.handleInput(pan);
    assert(close(viewport.update(.016F).target, home.target));
    pan.events.push_back({input::EventType::MouseButtonDown, 0, 0, 3, 640, 360, 0, 0, {}});
    viewport.handleInput(pan);
    const auto panned = viewport.update(.016F);
    assert(!close(panned.target, home.target));
    viewport.configure(home, 1);
    assert(close(viewport.update(.016F).target, panned.target));
    viewport.cancelGesture();
    pan.events.clear();
    viewport.handleInput(pan);
    assert(close(viewport.update(.016F).target, panned.target));
    // Wheel outside does nothing. Explicit reset and preset changes do reset.
    input::InputFrame wheel{};
    wheel.mouse_x = 10; wheel.mouse_wheel_y = 2;
    viewport.handleInput(wheel);
    assert(close(viewport.update(.016F).position, panned.position));
    viewport.handleInput({.reset_pressed=true, .events={}});
    assert(close(viewport.update(.016F).position, home.position));
    home.position = {4,2,0};
    viewport.configure(home, 2);
    assert(close(viewport.update(.016F).position, home.position));
    // Keyboard movement uses real elapsed time, not the number of input calls.
    home.mode = camera::CameraMode::Fly;
    viewport.configure(home, 3);
    viewport.handleInput({.right_pressed=true, .events={}});
    const auto moved = viewport.update(.25F);
    assert(close(moved.position, home.position + math::Vec3{1,0,0}));
    assert(close(viewport.update(.25F).position, moved.position));
}
