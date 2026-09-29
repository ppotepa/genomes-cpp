#include "BuildIdentity.hpp"
#include <ThreeppGlBootstrap.hpp>
#include <genomes/platform/Platform.hpp>
#include <threepp/math/Color.hpp>
#include <threepp/renderers/GLRenderer.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace {
struct Options { std::uint32_t frames{180}; std::uint32_t cycles{1}; bool vsync{true}; };
bool number(std::string_view text, std::uint32_t& value) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
bool parse(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--no-vsync") { options.vsync = false; continue; }
        if ((argument == "--frames" || argument == "--cycles") && i + 1 < argc) {
            std::uint32_t value = 0;
            if (!number(argv[++i], value)) return false;
            if (argument == "--frames") options.frames = value; // zero = until close
            else options.cycles = value;
        } else return false;
    }
    return options.cycles >= 1 && options.cycles <= 100;
}
// 0=bounded cycle completed, 1=user requested exit, 2=error.
int run_cycle(const Options& options, std::uint32_t cycle) {
    using namespace genomes;
    platform::WindowConfig config{};
    config.title = "Genomes threepp / G02 bootstrap (no character yet)";
    config.graphics_api = platform::WindowGraphicsApi::OpenGL;
    auto created = platform::SdlPlatform::create(config);
    if (!created) { std::cerr << created.error().message << '\n'; return 2; }
    auto window = std::move(created.value());
    auto loaded = render::initializeThreeppGl(*window);
    if (!loaded) { std::cerr << loaded.error().message << '\n'; return 2; }
    const auto& info = loaded.value();
    std::cout << "backend=THREEPP_GL code=" << bootstrap::kRevision
              << " tracked=" << bootstrap::kTrackedState
              << " threepp=" << bootstrap::kThreeppRevision << " cycle=" << cycle << '\n'
              << "GL " << info.version << " | " << info.vendor << " | " << info.renderer << '\n'
              << "GLSL " << info.shading_language << " attributes=" << info.max_vertex_attributes
              << " vertex-textures=" << info.vertex_texture_units
              << " depth=" << info.default_depth_bits << " stencil=" << info.default_stencil_bits << '\n';
    if (const auto interval = window->set_gl_swap_interval(options.vsync ? 1 : 0); !interval) {
        std::cerr << "warning: " << interval.error().message << '\n';
    }
    {
        // Renderer lifetime is nested inside window/context lifetime, including
        // exceptions and early returns. No Canvas, GLFW loop, ECS or generation.
        threepp::GLRenderer renderer(std::pair<int, int>{std::max(1, window->width()), std::max(1, window->height())});
        renderer.setPixelRatio(1.0F);
        renderer.autoClear = false;
        renderer.setClearColor(threepp::Color(0.08F, 0.12F, 0.18F), 1.0F);
        std::uint32_t rendered = 0;
        bool initial = true;
        while (options.frames == 0 || rendered < options.frames) {
            const auto frame = window->poll_events();
            if (frame.error.code != foundation::ErrorCode::None) {
                std::cerr << frame.error.message << '\n'; return 2;
            }
            if (frame.quit_requested || frame.input.cancel_pressed) return 1;
            if (!frame.metrics.drawable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            if (initial || frame.resized) {
                renderer.setSize({frame.width, frame.height});
                std::cout << "window=" << frame.metrics.logical_width << 'x' << frame.metrics.logical_height
                          << " pixels=" << frame.width << 'x' << frame.height
                          << " scale=" << frame.metrics.scale_x() << ',' << frame.metrics.scale_y() << '\n';
                initial = false;
            }
            if (frame.input.mouse_left_pressed) {
                std::cout << "pointer-pixels=" << frame.input.mouse_x << ',' << frame.input.mouse_y << '\n';
            }
            renderer.clear(true, true, true);
            if (const auto swap = window->swap_gl_window(); !swap) {
                std::cerr << swap.error().message << '\n'; return 2;
            }
            ++rendered;
            if (!options.vsync) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::cout << "cycle-complete frames=" << rendered << '\n';
    }
    // ~GLRenderer ran while the SDL GL context was still current.
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: genomes_threepp_bootstrap [--frames N] [--cycles 1..100] [--no-vsync]\n";
        return 2;
    }
    try {
        for (std::uint32_t cycle = 1; cycle <= options.cycles; ++cycle) {
            const int result = run_cycle(options, cycle);
            if (result == 2) return 1;
            if (result == 1) return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << "threepp bootstrap: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
