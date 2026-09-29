#include <genomes/platform/Platform.hpp>

#include <cmath>
#include <iostream>

int main() {
    using namespace genomes::platform;
    int failures = 0;
    const auto check = [&failures](bool condition, const char* label) {
        if (!condition) { std::cerr << label << '\n'; ++failures; }
    };
    const WindowMetrics metrics{800, 600, 1200, 1200, false};
    const auto p = metrics.to_pixels(100.0F, 60.0F);
    check(p.x == 150.0F && p.y == 120.0F, "independent DPI axes");
    check(metrics.drawable(), "positive metrics drawable");
    auto hidden = metrics;
    hidden.minimized = true;
    check(!hidden.drawable(), "minimized not drawable");
    const WindowMetrics empty{};
    check(!empty.drawable() && std::isfinite(empty.scale_x()) && std::isfinite(empty.scale_y()),
          "zero dimensions must not divide by zero");
    WindowConfig config{};
    check(config.valid() && config.graphicsApi() == WindowGraphicsApi::Vulkan,
          "legacy default still Vulkan");
    config.vulkan = false;
    check(config.graphicsApi() == WindowGraphicsApi::None, "legacy native window");
    config.graphics_api = WindowGraphicsApi::OpenGL;
    config.vulkan = true;
    check(config.valid() && config.graphicsApi() == WindowGraphicsApi::OpenGL,
          "explicit API has one authoritative interpretation");
    config.graphics_api = static_cast<WindowGraphicsApi>(255);
    check(!config.valid(), "reject unknown API");
    config.graphics_api = WindowGraphicsApi::OpenGL;
    config.width = 0;
    check(!config.valid(), "reject invalid dimensions");
    return failures == 0 ? 0 : 1;
}
