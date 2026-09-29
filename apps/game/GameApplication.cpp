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

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace genomes::game {

namespace {

struct RunOptions final {
    foundation::SceneId initial_scene{foundation::scene_id("scene.main-menu")};
    std::optional<std::filesystem::path> capture_path;
    std::uint64_t capture_frame{180U};
    std::uint64_t max_frames{0U};
    bool deterministic{false};
    std::uint8_t unitlab_camera_steps{0U};
};

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) return false;
    const char* begin=text.data();
    const char* end=begin+text.size();
    const auto result=std::from_chars(begin,end,value);
    return result.ec==std::errc{} && result.ptr==end;
}

std::optional<RunOptions> parse_options(int argc, char** argv) {
    RunOptions options{};
    for (int index=1; index<argc; ++index) {
        const std::string_view argument =
            argv != nullptr && argv[index] != nullptr ? std::string_view{argv[index]} : "";
        if (argument=="--battlefield") options.initial_scene=foundation::scene_id("scene.battlefield");
        else if (argument=="--unit-lab") options.initial_scene=foundation::scene_id("scene.unit-lab");
        else if (argument=="--building-lab") options.initial_scene=foundation::scene_id("scene.building-lab");
        else if (argument=="--deterministic") options.deterministic=true;
        else if (argument=="--unitlab-camera" && index+1<argc && argv[index+1]!=nullptr) {
            const std::string_view camera=argv[++index];
            if (camera=="3q" || camera=="three-quarter") options.unitlab_camera_steps=0U;
            else if (camera=="front") options.unitlab_camera_steps=1U;
            else if (camera=="side") options.unitlab_camera_steps=2U;
            else if (camera=="back") options.unitlab_camera_steps=3U;
            else if (camera=="face") options.unitlab_camera_steps=4U;
            else if (camera=="hands") options.unitlab_camera_steps=5U;
            else return std::nullopt;
        }
        else if (argument=="--capture" && index+1<argc && argv[index+1]!=nullptr) {
            options.capture_path=std::filesystem::path{argv[++index]};
            options.deterministic=true;
        } else if (argument=="--capture-frame" && index+1<argc && argv[index+1]!=nullptr) {
            if (!parse_u64(argv[++index], options.capture_frame) || options.capture_frame==0U)
                return std::nullopt;
        } else if (argument=="--frames" && index+1<argc && argv[index+1]!=nullptr) {
            if (!parse_u64(argv[++index], options.max_frames)) return std::nullopt;
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return std::nullopt;
        }
    }
    if (options.capture_path) {
        if (options.max_frames==0U) options.max_frames=options.capture_frame;
        if (options.max_frames<options.capture_frame) return std::nullopt;
    }
    return options;
}

} // namespace

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
    const auto parsed=parse_options(argc,argv);
    if (!parsed) {
        std::cerr << "Usage: genomes_game [--battlefield|--unit-lab|--building-lab] "
                     "[--unitlab-camera 3q|front|side|back|face|hands] "
                     "[--frames N] [--deterministic] [--capture FILE --capture-frame N]\n";
        return 2;
    }
    const RunOptions options=*parsed;
    director_.set_deterministic_capture(options.deterministic);
    if (!director_.start(options.initial_scene)) {
        std::cerr << "Could not start initial scene\n";
        return 1;
    }
    if (options.initial_scene == foundation::scene_id("scene.unit-lab")) {
        for (std::uint8_t step=0U; step<options.unitlab_camera_steps; ++step) {
            const auto result=director_.dispatch_ui_action(
                foundation::stable_id("unit.camera"), {});
            if (result != ui::UiActionResult::Handled) {
                std::cerr << "Could not select UnitLab camera preset\n";
                return 1;
            }
        }
    }
    if (options.capture_path && options.capture_path->has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(options.capture_path->parent_path(), error);
        if (error) {
            std::cerr << "Could not create capture directory: " << error.message() << '\n';
            return 1;
        }
    }

    auto previous = std::chrono::steady_clock::now();
    std::uint64_t presented_frames=0U;
    while (!director_.quit_requested()) {
        const platform::PlatformFrame platform_frame = platform_->poll_events();
        if (platform_frame.error.code != foundation::ErrorCode::None) {
            std::cerr << "Platform frame failed: " << platform_frame.error.message << '\n';
            return 1;
        }
        if (platform_frame.quit_requested) break;
        if (!platform_frame.metrics.drawable()) {
            // SDL keeps pumping events while minimized, but an OpenGL swap on a
            // zero-sized/non-drawable surface is invalid. Pause presentation
            // time instead of poisoning renderer health during minimize.
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
            previous = std::chrono::steady_clock::now();
            continue;
        }
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
        const auto elapsed = options.deterministic
            ? std::chrono::nanoseconds{16'666'667}
            : std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
        previous = now;
#if defined(GENOMES_HAS_RMLUI)
        if (rml_ui_ && rml_ui_->valid()) {
            director_.handle_input(rml_ui_->filter_input(platform_frame.input));
        } else {
            director_.handle_input(platform_frame.input);
        }
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
        const std::uint64_t next_presented_frame = presented_frames + 1U;
        const bool capture_this_frame =
            options.capture_path && next_presented_frame == options.capture_frame;
        if (capture_this_frame) {
            const auto requested=renderer_->capture(*options.capture_path);
            if (!requested) {
                std::cerr << "Capture request failed: " << requested.error().message << '\n';
                return 1;
            }
        }
        director_.present();
        ++presented_frames;
        if (!renderer_->healthy()) {
            std::cerr << "Renderer frame failed: " << renderer_->last_error().message << '\n';
            return 1;
        }
        if (capture_this_frame) {
            std::cout << "Capture complete frame=" << presented_frames
                      << " path=" << options.capture_path->string() << '\n';
        }
        if (options.max_frames != 0U && presented_frames >= options.max_frames) break;
        if (!options.deterministic) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    if (backend_owner_) {
        const auto idle = backend_owner_->wait_idle();
        if (!idle) std::cerr << "Renderer wait-idle failed: " << idle.error().message << '\n';
    }
    return 0;
}

} // namespace genomes::game
