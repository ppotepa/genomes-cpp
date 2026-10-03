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
    camera::CameraRequest rts{};
    rts.mode=camera::CameraMode::RTS; rts.position={0,60,80}; rts.target={0,0,0};
    rts.rts.target_min={-50,-50}; rts.rts.target_max={50,50};
    viewport.configure(rts,4);
    input::InputFrame key_down{};
    key_down.mouse_x=640; key_down.mouse_y=360;
    input::Event key_event{};
    key_event.type = input::EventType::KeyDown;
    key_event.scancode = 26;
    key_down.events.push_back(key_event);
    viewport.handleInput(key_down);
    const auto rts_moved=viewport.update(.25F);
    assert(rts_moved.target.z<0.0F);
    viewport.handleInput({.mouse_x=640,.mouse_y=360,.events={}});
    assert(viewport.update(.25F).target.z<rts_moved.target.z);
    input::InputFrame focus_loss{}; focus_loss.focus_lost=true;
    viewport.handleInput(focus_loss);
    const auto stopped=viewport.update(.25F);
    assert(close(viewport.update(.25F).target,stopped.target));
    rts.target={12,0,8}; rts.position={12,60,88};
    viewport.configure(rts,4);
    viewport.handleInput({.reset_pressed=true,.mouse_x=640,.mouse_y=360,.events={}});
    assert(close(viewport.update(0.0F).target,rts.target));
    input::InputFrame edge{}; edge.mouse_x=1; edge.mouse_y=360;
    viewport.handleInput(edge);
    const auto edge_moved=viewport.update(.1F);
    assert(edge_moved.target.x<rts.target.x);
    edge.pointer_over_ui=true;
    viewport.handleInput(edge);
    assert(close(viewport.update(.1F).target,edge_moved.target));

    // RTS input tuning is carried by the camera request and can vary by scene.
    camera::CameraRequest stock_rts = rts;
    stock_rts.position = {0,60,80};
    stock_rts.target = {0,0,0};
    camera::CameraRequest tuned_rts = stock_rts;
    tuned_rts.rts.zoom_sensitivity = 0.24F;
    tuned_rts.rts.move_speed_factor = 1.30F;
    runtime::ViewportController stock_controls;
    runtime::ViewportController tuned_controls;
    stock_controls.configure(stock_rts, 5);
    tuned_controls.configure(tuned_rts, 5);
    input::InputFrame tuned_wheel{};
    tuned_wheel.viewport_width = 1280;
    tuned_wheel.viewport_height = 720;
    tuned_wheel.mouse_x = 640;
    tuned_wheel.mouse_y = 360;
    tuned_wheel.mouse_wheel_y = 1.0F;
    stock_controls.handleInput(tuned_wheel);
    tuned_controls.handleInput(tuned_wheel);
    const auto stock_zoomed = stock_controls.update(0.0F);
    const auto tuned_zoomed = tuned_controls.update(0.0F);
    assert(close(tuned_zoomed.target, stock_zoomed.target));
    assert(math::length(tuned_zoomed.position - tuned_zoomed.target) <
           math::length(stock_zoomed.position - stock_zoomed.target));

    input::InputFrame tuned_move{};
    tuned_move.viewport_width = 1280;
    tuned_move.viewport_height = 720;
    tuned_move.mouse_x = 640;
    tuned_move.mouse_y = 360;
    tuned_move.right_pressed = true;
    stock_controls.handleInput(tuned_move);
    tuned_controls.handleInput(tuned_move);
    const auto stock_moved = stock_controls.update(.25F);
    const auto tuned_moved = tuned_controls.update(.25F);
    assert(math::length(tuned_moved.target) > math::length(stock_moved.target));
}
