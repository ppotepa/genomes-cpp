#include "GameApplication.hpp"
#include <DiligentBackend.hpp>
#include <DiligentSceneRenderer.hpp>
#include <genomes/platform/Platform.hpp>
#include <genomes/platform/SdlFileDialogService.hpp>
#include <genomes/render/RenderBackend.hpp>
#include <genomes/runtime/BuiltinScenes.hpp>
#include <algorithm>
#include <array>
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
#include <vector>
#include <variant>
#include <type_traits>

namespace genomes::game {
namespace {
struct RunOptions final {
    foundation::SceneId initial_scene{foundation::scene_id("scene.main-menu")};
    std::optional<std::filesystem::path> capture_path;
    std::uint64_t capture_frame{180U},max_frames{0U};
    bool deterministic{false};
    std::uint8_t unitlab_camera_steps{0U},unitlab_locomotion_steps{0U},unitlab_expression_steps{0U};
};
bool parse_u64(std::string_view text,std::uint64_t& value) {
    if (text.empty()) return false;
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==std::errc{} && result.ptr==text.data()+text.size();
}
std::optional<RunOptions> parse_options(int argc,char** argv) {
    RunOptions result;
    for (int i=1;i<argc;++i) {
        const std::string_view arg=argv&&argv[i]?std::string_view{argv[i]}:"";
        const auto next=[&]()->std::optional<std::string_view> {
            if (!argv || i+1>=argc || !argv[i+1]) return {};
            return std::string_view{argv[++i]};
        };
        if (arg=="--battlefield") result.initial_scene=foundation::scene_id("scene.battlefield");
        else if (arg=="--unit-lab") result.initial_scene=foundation::scene_id("scene.unit-lab");
        else if (arg=="--building-lab") result.initial_scene=foundation::scene_id("scene.building-lab");
        else if (arg=="--deterministic") result.deterministic=true;
        else if (arg=="--capture") {
            const auto value=next();if (!value || value->empty()) return {};
            result.capture_path=std::filesystem::path{*value};result.deterministic=true;
        } else if (arg=="--frames" || arg=="--capture-frame") {
            const auto value=next();if (!value) return {};
            auto& count=arg=="--frames"?result.max_frames:result.capture_frame;
            if (!parse_u64(*value,count) || (arg=="--capture-frame"&&count==0)) return {};
        } else if (arg=="--unitlab-camera") {
            const auto value=next();if (!value) return {};
            if (*value=="3q" || *value=="three-quarter") result.unitlab_camera_steps=0;
            else if (*value=="front") result.unitlab_camera_steps=1;
            else if (*value=="side") result.unitlab_camera_steps=2;
            else if (*value=="back") result.unitlab_camera_steps=3;
            else if (*value=="face") result.unitlab_camera_steps=4;
            else if (*value=="hands") result.unitlab_camera_steps=5;
            else return {};
        } else if (arg=="--unitlab-locomotion") {
            const auto value=next();if (!value) return {};
            if (*value=="idle") result.unitlab_locomotion_steps=0;
            else if (*value=="walk") result.unitlab_locomotion_steps=1;
            else if (*value=="run") result.unitlab_locomotion_steps=2;
            else if (*value=="crouch") result.unitlab_locomotion_steps=3;
            else return {};
        } else if (arg=="--unitlab-expression") {
            const auto value=next();if (!value) return {};
            if (*value=="neutral") result.unitlab_expression_steps=0;
            else if (*value=="alert") result.unitlab_expression_steps=1;
            else if (*value=="fear") result.unitlab_expression_steps=2;
            else if (*value=="anger") result.unitlab_expression_steps=3;
            else if (*value=="pain") result.unitlab_expression_steps=4;
            else if (*value=="fatigue") result.unitlab_expression_steps=5;
            else if (*value=="eyes-closed") result.unitlab_expression_steps=6;
            else return {};
        } else {
            std::cerr<<"Unknown argument: "<<arg<<'\n';return {};
        }
    }
    if (result.capture_path) {
        if (!result.max_frames) result.max_frames=result.capture_frame;
        if (result.max_frames<result.capture_frame) return {};
    }
    return result;
}
}

GameApplication::~GameApplication()=default;
GameApplication::GameApplication(std::unique_ptr<platform::SdlPlatform> platform,
                                 std::unique_ptr<render::IRenderer> renderer,
                                 std::unique_ptr<render::RenderBackend> backend_owner)
    :platform_(std::move(platform)),backend_owner_(std::move(backend_owner)),
     renderer_(std::move(renderer)),jobs_(0U,2U),director_(*renderer_,ui_,presentation_,&jobs_) {
    runtime::registerBuiltinScenes(director_,true);
    if (auto content=ui::UiContentRegistry::discover("mods")) {
        content_=std::move(content.value());ui::UiPluginError plugin_error;
if (!plugins_.load(content_, false, &plugin_error)) std::cerr<<"UI plugin loading failed: "<<plugin_error.message<<'\n';
    } else std::cerr<<"UI content discovery failed: "<<content.error().message<<'\n';
    ui_.set_route_resolver([this](ui::UiRoute& route) {
        const auto* manifest = content_.find_scene(route.scene);
        if (manifest == nullptr) return;
        for (const auto& mod : content_.mods()) {
            if (mod.id == manifest->mod_id) {
                route.document = (manifest->root.lexically_relative(mod.root) /
                                  manifest->document).generic_string();
                break;
            }
        }
        route.controller = manifest->controller;
        route.action_namespace = manifest->action_namespace;
    });
#if defined(GENOMES_HAS_RMLUI)
    rml_ui_=std::make_unique<ui::rml::Runtime>("mods/core",static_cast<std::uint32_t>(platform_->width()),static_cast<std::uint32_t>(platform_->height()));
    if (rml_ui_->valid()) {
        file_dialog_service_=std::make_unique<platform::SdlFileDialogService>(
            static_cast<SDL_Window*>(platform_->sdl_window()));
        rml_ui_->set_file_dialog_service(file_dialog_service_.get());
        rml_ui_->set_action_router(this);
        rml_ui_->set_event_router([this](const ui::UiEvent& event) {
            if (event.route_revision != 0U && event.route_revision != ui_.routes().revision())
                return ui::UiActionResult::Rejected;
            const std::string& field_key = event.field;
            if (const auto* field = ui_.model().find_field(field_key); field != nullptr) {
                if (field->commit_policy == ui::UiCommitPolicy::OnChange &&
                    event.phase == ui::UiEventPhase::Input)
                    return ui::UiActionResult::Handled;
                if (field->commit_policy == ui::UiCommitPolicy::Explicit &&
                    event.phase == ui::UiEventPhase::Input)
                    return ui::UiActionResult::Handled;
            }
            ui::UiActionArguments arguments;
            arguments.emplace_back("value", std::visit([](const auto& value) -> std::string {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::string>) return value;
                else if constexpr (std::is_same_v<Value, bool>) return value ? "true" : "false";
                else {
                    std::array<char, 64> buffer{};
                    const auto result = [&] {
                        if constexpr (std::is_floating_point_v<Value>)
                            return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                                 std::chars_format::general);
                        else return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
                    }();
                    return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : std::string{};
                }
            }, event.value));
            for (const auto& [key, value] : event.arguments) {
                arguments.emplace_back(key, std::visit([](const auto& item) -> std::string {
                    using Value = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<Value, std::string>) return item;
                    else if constexpr (std::is_same_v<Value, bool>) return item ? "true" : "false";
                    else {
                        std::array<char, 64> buffer{};
                        const auto result = [&] {
                            if constexpr (std::is_floating_point_v<Value>)
                                return std::to_chars(buffer.data(), buffer.data() + buffer.size(), item,
                                                     std::chars_format::general);
                            else return std::to_chars(buffer.data(), buffer.data() + buffer.size(), item);
                        }();
                        return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : std::string{};
                    }
                }, value));
            }
            return director_.dispatch_ui_action(foundation::stable_id(event.control), arguments);
        });
    }
#endif
}
#if defined(GENOMES_HAS_RMLUI)
std::string GameApplication::resolve_document(foundation::SceneId scene) const {
    const auto* manifest=content_.find_scene(scene);
    if (!manifest) return "scenes/main-menu/screen.rml";
    for (const auto& mod:content_.mods()) if (mod.id==manifest->mod_id)
        return (manifest->root.lexically_relative(mod.root)/manifest->document).generic_string();
    return "scenes/main-menu/screen.rml";
}
ui::UiActionResult GameApplication::dispatch(ui::UiActionId action,const ui::UiActionArguments& args) {
    return director_.dispatch_ui_action(action,args);
}
#endif
foundation::Result<std::unique_ptr<GameApplication>,foundation::Error> GameApplication::create() {
    using Result=foundation::Result<std::unique_ptr<GameApplication>,foundation::Error>;
    platform::WindowConfig window{};window.title="Genomes - Procedural World";
    window.width=1280;window.height=720;window.resizable=true;
    // The production application is Windows/D3D12. NativeD3D is explicit so
    // the platform layer cannot silently create a non-D3D presentation window.
    window.graphics_api=platform::WindowGraphicsApi::NativeD3D;
    auto created=platform::SdlPlatform::create(window);
    if (!created) return Result::failure(created.error());
    auto platform=std::move(created.value());
    render::RenderConfig config{};
    config.backend=render::RenderBackendKind::D3D12;
    config.headless=false;config.width=static_cast<std::uint32_t>(platform->width());config.height=static_cast<std::uint32_t>(platform->height());
#ifndef NDEBUG
    config.validation=true;
#endif
    auto created_backend=render::DiligentBackend::create(config,platform->native_window());
    if (!created_backend) return Result::failure(created_backend.error());
    auto backend=std::move(created_backend.value());
    std::unique_ptr<render::IRenderer> renderer=std::make_unique<render::DiligentSceneRenderer>(*backend);
    return Result::success(std::unique_ptr<GameApplication>{new GameApplication{std::move(platform),std::move(renderer),std::move(backend)}});
}
foundation::Result<void,foundation::Error> GameApplication::resize_renderer(std::uint32_t w,std::uint32_t h) {
    auto* backend=dynamic_cast<render::DiligentBackend*>(backend_owner_.get());
    if (!backend) return foundation::Result<void,foundation::Error>::failure({foundation::ErrorCode::InvalidState,"renderer composition mismatch"});
    return backend->resize(w,h);
}
int GameApplication::run(int argc,char** argv) {
    const auto parsed=parse_options(argc,argv);
    if (!parsed) {
        std::cerr<<"Usage: genomes_game [--unit-lab|--battlefield|--building-lab] "
            "[--unitlab-camera 3q|front|side|back|face|hands] [--unitlab-locomotion idle|walk|run|crouch] "
            "[--unitlab-expression neutral|alert|fear|anger|pain|fatigue|eyes-closed] "
            "[--frames N] [--deterministic] [--capture FILE.png --capture-frame N]\n";
        return 2;
    }
    const RunOptions options=*parsed;
#if defined(GENOMES_HAS_RMLUI)
    if (!rml_ui_ || !rml_ui_->valid()) {std::cerr<<"RmlUi initialization failed; refusing an invisible interface\n";return 1;}
#endif
    director_.set_deterministic_capture(options.deterministic);
    if (!director_.start(options.initial_scene)) {std::cerr<<"Could not start initial scene\n";return 1;}
    if (options.initial_scene==foundation::scene_id("scene.unit-lab")) {
        const auto repeat=[this](std::string_view action,std::uint8_t count) {
            for (std::uint8_t i=0;i<count;++i)
                if (director_.dispatch_ui_action(foundation::stable_id(action),{})!=ui::UiActionResult::Handled) return false;
            return true;
        };
        if (!repeat("unit.camera",options.unitlab_camera_steps)||!repeat("unit.locomotion",options.unitlab_locomotion_steps)||!repeat("unit.expression",options.unitlab_expression_steps)) {
            std::cerr<<"Could not select deterministic UnitLab state\n";return 1;
        }
    }
    auto previous=std::chrono::steady_clock::now();std::uint64_t frames=0;bool captured=false;
    while (!director_.quit_requested()) {
        const auto platform_frame=platform_->poll_events();
        if (platform_frame.error.code!=foundation::ErrorCode::None) {std::cerr<<"Platform failed: "<<platform_frame.error.message<<'\n';return 1;}
        if (platform_frame.quit_requested) break;
        if (!platform_frame.metrics.drawable()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});previous=std::chrono::steady_clock::now();continue;
        }
        if (platform_frame.resized && platform_frame.width>0 && platform_frame.height>0) {
            const auto w=static_cast<std::uint32_t>(platform_frame.width),h=static_cast<std::uint32_t>(platform_frame.height);
            if (auto result=resize_renderer(w,h);!result) {std::cerr<<"Resize failed: "<<result.error().message<<'\n';return 1;}
#if defined(GENOMES_HAS_RMLUI)
            rml_ui_->resize(w,h);
#endif
        }
        const auto now=std::chrono::steady_clock::now();
        const auto elapsed=options.deterministic?std::chrono::nanoseconds{16'666'667}:std::chrono::duration_cast<std::chrono::nanoseconds>(now-previous);
        previous=now;
        // Captures keep their declared state independent of accidental input.
        if (!options.capture_path) {
            // UI filtering completes before SceneDirector routes input to the camera controller.
#if defined(GENOMES_HAS_RMLUI)
            director_.handle_input(rml_ui_->filter_input(platform_frame.input));
#else
            director_.handle_input(platform_frame.input);
#endif
        }
        const auto advance=clock_.advanceBy(elapsed,[&](double dt,foundation::SimulationTick) noexcept {director_.fixed_update(dt);});
        director_.set_presentation_timing(advance.first_tick,advance.next_tick,advance.interpolation_alpha);
        const double dt=std::chrono::duration<double>(elapsed).count();director_.frame_update(dt);
#if defined(GENOMES_HAS_RMLUI)
        const auto route_revision=ui_.routes().revision();
        if (route_revision!=rml_route_revision_) {
            std::vector<ui::UiRoute> mounted_routes;
            mounted_routes.reserve(ui_.routes().routes().size());
            for (const auto& route:ui_.routes().routes()) {
                auto mounted = route;
                mounted.document=resolve_document(route.scene);
                if (const auto* manifest=content_.find_scene(route.scene);manifest!=nullptr) {
                    mounted.controller=manifest->controller;
                    mounted.action_namespace=manifest->action_namespace;
                }
                mounted_routes.push_back(std::move(mounted));
            }
            if (!rml_ui_->mount_routes(mounted_routes, ui_)) {
                std::cerr<<"Could not mount RmlUi route stack\n";
                return 1;
            }
            rml_route_revision_=route_revision;
        }
        rml_ui_->set_model(ui_.model());
        rml_ui_->set_route_models(ui_);
        if (const auto* scale=ui_.model().find("ui_scale"); scale!=nullptr &&
            std::holds_alternative<double>(*scale)) {
            rml_ui_->set_density_ratio(static_cast<float>(std::get<double>(*scale)));
        }
        ui_.replace_frame(rml_ui_->update(dt));
#endif
        const bool capture_now=options.capture_path&&frames+1U==options.capture_frame;
        if (capture_now) if (auto result=renderer_->capture(*options.capture_path);!result) {
            std::cerr<<"Capture request failed: "<<result.error().message<<'\n';return 1;
        }
        director_.present();++frames;
        if (!renderer_->healthy()) {std::cerr<<"Renderer failed: "<<renderer_->last_error().message<<'\n';return 1;}
        if (capture_now) {captured=true;std::cout<<"Capture complete frame="<<frames<<" path="<<options.capture_path->string()<<'\n';}
        if (options.max_frames && frames>=options.max_frames) break;
        if (!options.deterministic) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    if (backend_owner_) if (auto result=backend_owner_->wait_idle();!result) {
        std::cerr<<"GPU wait failed: "<<result.error().message<<'\n';return 1;
    }
    if (options.capture_path&&!captured) {std::cerr<<"Application stopped before the requested capture\n";return 1;}
    return 0;
}
} // namespace genomes::game
