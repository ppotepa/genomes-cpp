#include "GameApplication.hpp"

#if defined(GENOMES_GAME_DILIGENT)
#include <DiligentBackend.hpp>
#include <DiligentSceneRenderer.hpp>
#elif defined(GENOMES_GAME_THREEPP)
#include <ThreeppSceneRenderer.hpp>
#endif

#include <genomes/platform/Platform.hpp>
#include <genomes/render/RenderBackend.hpp>
#include <genomes/runtime/BuiltinScenes.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace genomes::game {

GameApplication::~GameApplication() = default;

GameApplication::GameApplication(std::unique_ptr<platform::SdlPlatform> platform,
                                 std::unique_ptr<render::IRenderer> renderer,
                                 std::unique_ptr<render::RenderBackend> backend_owner)
    : platform_(std::move(platform)), backend_owner_(std::move(backend_owner)),
      renderer_(std::move(renderer)), jobs_(0U, 2U),
      director_(*renderer_, ui_, presentation_, &jobs_) {
    runtime::registerBuiltinScenes(director_, true);
    if (auto content = ui::UiContentRegistry::discover("mods")) {
        content_ = std::move(content.value());
        ui::UiPluginError plugin_error;
        if (!plugins_.load(content_, &plugin_error)) {
            std::cerr << "UI plugin loading failed: " << plugin_error.message << '\n';
        }
    } else {
        std::cerr << "UI content discovery failed: " << content.error().message << '\n';
    }
#if defined(GENOMES_HAS_RMLUI)
    rml_ui_ = std::make_unique<ui::rml::Runtime>("mods/core",
                                                 static_cast<std::uint32_t>(platform_->width()),
                                                 static_cast<std::uint32_t>(platform_->height()));
    if (rml_ui_->valid()) rml_ui_->set_action_router(this);
#endif
}

#if defined(GENOMES_HAS_RMLUI)
std::string GameApplication::document_for_scene(foundation::SceneId scene) const {
    const auto* manifest = content_.find_scene(
        scene == foundation::scene_id("scene.battlefield") ? "scene.battlefield" :
        scene == foundation::scene_id("scene.settings") ? "scene.settings" :
        scene == foundation::scene_id("scene.world-config") ? "scene.world-config" :
        scene == foundation::scene_id("scene.building-lab") ? "scene.building-lab" :
        scene == foundation::scene_id("scene.unit-lab") ? "scene.unit-lab" :
        scene == foundation::scene_id("scene.world-lab") ? "scene.world-lab" :
        scene == foundation::scene_id("scene.pause") ? "scene.pause" : "scene.main-menu");
    if (manifest == nullptr) return "scenes/main-menu/screen.rml";
    for (const auto& mod : content_.mods()) {
        if (mod.id != manifest->mod_id) continue;
        const auto relative_root = manifest->root.lexically_relative(mod.root);
        return (relative_root / manifest->document).generic_string();
    }
    return "scenes/main-menu/screen.rml";
}

ui::UiActionResult GameApplication::dispatch(
    ui::UiActionId action, const ui::UiActionArguments& arguments) {
    return director_.dispatch_ui_action(action, arguments);
}
#endif

foundation::Result<std::unique_ptr<GameApplication>, foundation::Error> GameApplication::create() {
    platform::WindowConfig window{};
    window.title = "Genomes - Procedural World";
    window.width = 1280;
    window.height = 720;
    window.resizable = true;
#if defined(GENOMES_GAME_THREEPP)
    window.vulkan = false;
    window.graphics_api = platform::WindowGraphicsApi::OpenGL;
#elif defined(GENOMES_GAME_DILIGENT)
# if defined(_WIN32)
    window.vulkan = false;
    window.graphics_api = platform::WindowGraphicsApi::NativeD3D;
# else
    window.vulkan = true;
    window.graphics_api = platform::WindowGraphicsApi::Vulkan;
# endif
#else
# error "genomes_game requires one configured renderer"
#endif

    auto platform_result = platform::SdlPlatform::create(window);
    if (!platform_result) {
        return foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>::failure(
            platform_result.error());
    }
    auto platform = std::move(platform_result.value());

#if defined(GENOMES_GAME_THREEPP)
    auto renderer_result = render::ThreeppSceneRenderer::create(*platform);
    if (!renderer_result) {
        return foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>::failure(
            renderer_result.error());
    }
    std::unique_ptr<render::IRenderer> renderer = std::move(renderer_result.value());
    return foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>::success(
        std::unique_ptr<GameApplication>(
            new GameApplication(std::move(platform), std::move(renderer))));
#else
    render::RenderConfig render_config{};
# if defined(_WIN32)
    render_config.backend = render::RenderBackendKind::D3D12;
# else
    render_config.backend = render::RenderBackendKind::Vulkan;
# endif
    render_config.headless = false;
    render_config.width = static_cast<std::uint32_t>(platform->width());
    render_config.height = static_cast<std::uint32_t>(platform->height());
    auto backend_result = render::DiligentBackend::create(render_config, platform->native_window());
    if (!backend_result) {
        return foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>::failure(
            backend_result.error());
    }
    auto diligent = std::move(backend_result.value());
    std::unique_ptr<render::IRenderer> renderer =
        std::make_unique<render::DiligentSceneRenderer>(*diligent);
    std::unique_ptr<render::RenderBackend> backend = std::move(diligent);
    return foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>::success(
        std::unique_ptr<GameApplication>(
            new GameApplication(std::move(platform), std::move(renderer), std::move(backend))));
#endif
}

foundation::Result<void, foundation::Error> GameApplication::resize_renderer(
    std::uint32_t width, std::uint32_t height) {
#if defined(GENOMES_GAME_THREEPP)
    auto* threepp = dynamic_cast<render::ThreeppSceneRenderer*>(renderer_.get());
    if (threepp == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "threepp renderer composition mismatch"});
    }
    return threepp->resize(width, height);
#else
    auto* diligent = dynamic_cast<render::DiligentBackend*>(backend_owner_.get());
    if (diligent == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend composition mismatch"});
    }
    return diligent->resize(width, height);
#endif
}

int GameApplication::run(int argc, char** argv) {
    const std::string_view start_argument =
        argc > 1 && argv != nullptr && argv[1] != nullptr ? std::string_view{argv[1]} : "";
    const auto initial_scene = foundation::scene_id(
        start_argument == "--battlefield"
            ? "scene.battlefield"
            : (start_argument == "--unit-lab"
                   ? "scene.unit-lab"
                   : (start_argument == "--building-lab" ? "scene.building-lab"
                                                            : "scene.main-menu")));
    if (!director_.start(initial_scene)) {
        std::cerr << "Could not start initial scene\n";
        return 1;
    }

    auto previous = std::chrono::steady_clock::now();
    while (!director_.quit_requested()) {
        const platform::PlatformFrame platform_frame = platform_->poll_events();
        if (platform_frame.error.code != foundation::ErrorCode::None) {
            std::cerr << "Platform frame failed: " << platform_frame.error.message << '\n';
            return 1;
        }
        if (platform_frame.quit_requested) break;
        if (platform_frame.resized && platform_frame.width > 0 && platform_frame.height > 0) {
            const auto resized = resize_renderer(
                static_cast<std::uint32_t>(platform_frame.width),
                static_cast<std::uint32_t>(platform_frame.height));
            if (!resized) {
                std::cerr << "Renderer resize failed: " << resized.error().message << '\n';
                return 1;
            }
#if defined(GENOMES_HAS_RMLUI)
            if (rml_ui_) {
                rml_ui_->resize(static_cast<std::uint32_t>(platform_frame.width),
                                static_cast<std::uint32_t>(platform_frame.height));
            }
#endif
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
        previous = now;
#if defined(GENOMES_HAS_RMLUI)
        const bool rml_consumed = rml_ui_ && rml_ui_->process_input(platform_frame.input);
        if (rml_consumed) director_.handle_input({.events = {}});
        else director_.handle_input(platform_frame.input);
#else
        director_.handle_input(platform_frame.input);
#endif
        const auto simulate = [&](double fixed_dt, foundation::SimulationTick) noexcept {
            director_.fixed_update(fixed_dt);
        };
        const auto advance = clock_.advanceBy(elapsed, simulate);
        director_.set_presentation_timing(advance.first_tick, advance.next_tick,
                                          advance.interpolation_alpha);
        director_.frame_update(std::chrono::duration<double>(elapsed).count());
#if defined(GENOMES_HAS_RMLUI)
        if (rml_ui_ && rml_ui_->valid()) {
            const auto route_revision = ui_.routes().revision();
            if (route_revision != rml_route_revision_) {
                rml_ui_->unload_documents();
                bool loaded = true;
                for (const auto& route : ui_.routes().routes()) {
                    loaded = rml_ui_->push_document(document_for_scene(route.scene)) && loaded;
                }
                if (loaded) rml_route_revision_ = route_revision;
            }
            ui_.replace_frame(rml_ui_->update(std::chrono::duration<double>(elapsed).count()));
        }
#endif
        director_.present();
        if (!renderer_->healthy()) {
            std::cerr << "Renderer frame failed: " << renderer_->last_error().message << '\n';
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    if (backend_owner_) {
        const auto idle = backend_owner_->wait_idle();
        if (!idle) std::cerr << "Renderer wait-idle failed: " << idle.error().message << '\n';
    }
    return 0;
}

} // namespace genomes::game
