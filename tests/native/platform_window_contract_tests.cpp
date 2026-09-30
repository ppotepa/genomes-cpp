#include <genomes/platform/Platform.hpp>

#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::platform;

    const WindowMetrics metrics{800, 600, 1200, 1200, false};
    const auto pixels = metrics.to_pixels(100.0F, 60.0F);
    assert(pixels.x == 150.0F && pixels.y == 120.0F);
    assert(metrics.drawable());

    auto minimized = metrics;
    minimized.minimized = true;
    assert(!minimized.drawable());

    const WindowMetrics empty{};
    assert(!empty.drawable());
    assert(std::isfinite(empty.scale_x()) && std::isfinite(empty.scale_y()));

    WindowConfig config{};
    config.vulkan = false;
    config.graphics_api = WindowGraphicsApi::NativeD3D;
    assert(config.valid() && config.graphicsApi() == WindowGraphicsApi::NativeD3D);
    config.vulkan = true;
    assert(config.valid() && config.graphicsApi() == WindowGraphicsApi::NativeD3D);
    config.graphics_api = static_cast<WindowGraphicsApi>(255);
    assert(!config.valid());
    config.graphics_api = WindowGraphicsApi::NativeD3D;
    config.width = 0;
    assert(!config.valid());

    genomes::input::InputFrame frame{};
    frame.events.push_back({genomes::input::EventType::FocusLost});
    frame.events.push_back({genomes::input::EventType::PointerCancel});
    assert(frame.events.size() == 2U);
    assert(frame.events[0].type == genomes::input::EventType::FocusLost);
    assert(frame.events[1].type == genomes::input::EventType::PointerCancel);
    return 0;
}
