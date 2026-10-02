#include "GameApplication.hpp"
#include <DiligentBackend.hpp>
#include <DiligentSceneRenderer.hpp>
#include <genomes/platform/Platform.hpp>
#include <genomes/platform/SdlFileDialogService.hpp>
#include <genomes/render/RenderBackend.hpp>
#include <genomes/game_scenes/BuiltinScenes.hpp>
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
#if GENOMES_HAS_INFANTRY
#include <genomes/game_scenes/UnitLabCommandParsing.hpp>
#endif

namespace genomes::game {
namespace {
struct RunOptions final {
    foundation::SceneId initial_scene{foundation::scene_id("scene.main-menu")};
    std::optional<std::filesystem::path> capture_path;
    std::uint64_t capture_frame{180U},max_frames{0U};
    bool deterministic{false};
    std::uint8_t unitlab_camera_steps{0U},unitlab_locomotion_steps{0U},unitlab_expression_steps{0U};
#if GENOMES_HAS_INFANTRY
    std::optional<game_scenes::SetVariation> unitlab_variation;
    std::optional<game_scenes::SetEquipmentSlot> unitlab_equipment;
    std::optional<game_scenes::SetGeneOverride> unitlab_gene;
    std::optional<game_scenes::SetAppearancePreset> unitlab_appearance;
#endif
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
#if GENOMES_HAS_INFANTRY
        } else if (arg=="--unitlab-camera") {
            const auto value=next();if (!value) return {};
            const std::array<std::string_view, 2> tokens{
                "set-camera-mode", *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetCameraMode>(parsed.value()))
                return {};
            result.unitlab_camera_steps = static_cast<std::uint8_t>(
                std::get<game_scenes::SetCameraMode>(parsed.value()).value);
        } else if (arg=="--unitlab-locomotion") {
            const auto value=next();if (!value) return {};
            const std::array<std::string_view, 2> tokens{
                "set-locomotion-preset", *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed ||
                !std::holds_alternative<game_scenes::SetLocomotionPreset>(parsed.value()))
                return {};
            result.unitlab_locomotion_steps = static_cast<std::uint8_t>(
                std::get<game_scenes::SetLocomotionPreset>(parsed.value()).value);
        } else if (arg=="--unitlab-expression") {
            const auto value=next();if (!value) return {};
            const std::array<std::string_view, 2> tokens{
                "set-expression", *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetExpression>(parsed.value()))
                return {};
            result.unitlab_expression_steps = static_cast<std::uint8_t>(
                std::get<game_scenes::SetExpression>(parsed.value()).value);
        } else if (arg=="--unitlab-variation") {
            const auto value=next();if (!value) return {};
            const std::array<std::string_view, 2> tokens{
                "set-variation", *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetVariation>(parsed.value()))
                return {};
            result.unitlab_variation = std::get<game_scenes::SetVariation>(parsed.value());
        } else if (arg=="--unitlab-equipment") {
            const auto slot=next(); const auto item=next();
            if (!slot || !item) return {};
            const std::array<std::string_view, 3> tokens{
                "set-equipment-slot", *slot, *item};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetEquipmentSlot>(parsed.value()))
                return {};
            result.unitlab_equipment = std::get<game_scenes::SetEquipmentSlot>(parsed.value());
        } else if (arg=="--unitlab-gene") {
            const auto gene=next(); const auto value=next();
            if (!gene || !value) return {};
            const std::array<std::string_view, 3> tokens{
                "set-gene-override", *gene, *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetGeneOverride>(parsed.value()))
                return {};
            result.unitlab_gene = std::get<game_scenes::SetGeneOverride>(parsed.value());
        } else if (arg=="--unitlab-appearance") {
            const auto value=next();if (!value) return {};
            const std::array<std::string_view, 2> tokens{
                "set-appearance-preset", *value};
            const auto parsed = game_scenes::parseUnitLabCommandLine(tokens);
            if (!parsed || !std::holds_alternative<game_scenes::SetAppearancePreset>(parsed.value()))
                return {};
            result.unitlab_appearance = std::get<game_scenes::SetAppearancePreset>(parsed.value());
#endif
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
                                 std::unique_ptr<render::RenderBackend> backend_owner,
                                 std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile,
                                 std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile
#if GENOMES_HAS_INFANTRY
                                 , combat::TacticalAIProfile tactical_ai_profile
                                 , std::shared_ptr<const infantry::FrozenAppearanceCatalog> appearance_catalog
#endif
                                 )
    :platform_(std::move(platform)),backend_owner_(std::move(backend_owner)),
     renderer_(std::move(renderer)),jobs_(0U,2U),director_(*renderer_,ui_,presentation_,&jobs_) {
    application::BuiltinSceneConfig scene_config{};
    scene_config.real_battlefield = true;
    scene_config.world_generation_profile = std::move(world_profile);
    scene_config.building_profile = std::move(building_profile);
#if GENOMES_HAS_INFANTRY
    scene_config.tactical_ai_profile = tactical_ai_profile;
    scene_config.appearance_catalog = std::move(appearance_catalog);
#endif
    scene_catalog_ = std::make_unique<application::BuiltinSceneCatalog>(std::move(scene_config));
    scene_catalog_->install(director_);
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
            const auto* active_route = ui_.routes().top();
            if (event.route_revision != 0U &&
                (active_route == nullptr || event.route_revision != active_route->revision))
                return ui::UiActionResult::Rejected;
            const auto applied = ui_.apply_event(event);
            if (!event.field.empty() && !applied.consumed)
                return ui::UiActionResult::Rejected;
            if (applied.consumed && !applied.publish)
                return ui::UiActionResult::Handled;
            ui::UiActionArguments arguments;
            const auto& published_value = applied.consumed ? applied.value : event.value;
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
            }, published_value));
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
    auto world_profile = world::loadWorldGenerationProfile(
        "mods/core/profiles/world-generation.json");
    if (!world_profile) return Result::failure(world_profile.error());
    auto shared_world_profile = std::make_shared<const world::FrozenWorldGenerationProfile>(
        std::move(world_profile.value()));
    auto building_profile = buildings::loadBuildingProfile("mods/core/profiles/building.json");
    if (!building_profile) return Result::failure(building_profile.error());
    auto shared_building_profile = std::make_shared<const buildings::FrozenBuildingProfile>(
        std::move(building_profile.value()));
#if GENOMES_HAS_INFANTRY
    auto tactical_ai_profile = combat::loadTacticalAIProfile("mods/core/profiles/tactical-ai.json");
    if (!tactical_ai_profile) return Result::failure(tactical_ai_profile.error());
    auto appearance_catalog = infantry::loadAppearanceCatalog(
        "mods/core/profiles/appearance.json");
    if (!appearance_catalog) return Result::failure(appearance_catalog.error());
    return Result::success(std::unique_ptr<GameApplication>{new GameApplication{
        std::move(platform), std::move(renderer), std::move(backend),
        std::move(shared_world_profile),
        std::move(shared_building_profile),
        tactical_ai_profile.value().profile,
        std::make_shared<const infantry::FrozenAppearanceCatalog>(
            std::move(appearance_catalog.value()))}});
#else
    return Result::success(std::unique_ptr<GameApplication>{new GameApplication{
        std::move(platform), std::move(renderer), std::move(backend),
        std::move(shared_world_profile),
        std::move(shared_building_profile)}});
#endif
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
            "[--unitlab-variation 0..1.75] [--unitlab-equipment SLOT ITEM] [--unitlab-gene GENE VALUE] "
            "[--unitlab-appearance inspection-olive] "
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
#if GENOMES_HAS_INFANTRY
        if (options.unitlab_variation) {
            const auto text = std::to_string(options.unitlab_variation->value);
            if (director_.dispatch_ui_action(
                    foundation::stable_id("unit.variation"),
                    ui::UiActionArguments{{"value", text}}) !=
                ui::UiActionResult::Handled) {
                std::cerr<<"Could not select UnitLab variation\n";return 1;
            }
        }
        if (options.unitlab_equipment) {
            const auto& command = *options.unitlab_equipment;
            const auto& slots = infantry::EquipmentCatalog::slots();
            const auto slot = std::find_if(slots.begin(), slots.end(), [&](const auto& value) {
                return value.slot == command.slot;
            });
            if (slot == slots.end()) {
                std::cerr<<"Could not serialize UnitLab equipment slot\n";return 1;
            }
            std::string item = "auto";
            if (command.value.specified) {
                if (command.value.empty) item = "none";
                else {
                    const auto* definition = infantry::EquipmentCatalog::findItem(
                        command.value.definition_id);
                    if (!definition) {
                        std::cerr<<"Could not serialize UnitLab equipment item\n";return 1;
                    }
                    item = std::string{definition->identifier};
                }
            }
            if (director_.dispatch_ui_action(
                    foundation::stable_id("unit.equipment-item"),
                    ui::UiActionArguments{{"key", std::string{slot->identifier}}, {"value", item}}) !=
                ui::UiActionResult::Handled) {
                std::cerr<<"Could not select UnitLab equipment\n";return 1;
            }
        }
        if (options.unitlab_gene) {
            const auto& command = *options.unitlab_gene;
            const auto gene = infantry::genomeGeneName(command.gene);
            if (gene.empty()) {
                std::cerr<<"Could not serialize UnitLab gene\n";return 1;
            }
            if (director_.dispatch_ui_action(
                    foundation::stable_id("unit.genome"),
                    ui::UiActionArguments{{"key", std::string{gene}},
                                           {"value", std::to_string(command.value)}}) !=
                ui::UiActionResult::Handled) {
                std::cerr<<"Could not select UnitLab gene\n";return 1;
            }
        }
        if (options.unitlab_appearance) {
            const auto preset = game_scenes::unitLabAppearancePresetName(
                options.unitlab_appearance->value);
            if (preset.empty()) {
                std::cerr<<"Could not serialize UnitLab appearance preset\n";
                return 1;
            }
            if (director_.dispatch_ui_action(
                    foundation::stable_id("unit.appearance-preset"),
                    ui::UiActionArguments{{"value", std::string{preset}}}) !=
                ui::UiActionResult::Handled) {
                std::cerr<<"Could not select UnitLab appearance preset\n";return 1;
            }
        }
#endif
    }
    auto previous=std::chrono::steady_clock::now();std::uint64_t frames=0;bool captured=false;
    double fps_window_seconds = 0.0;
    std::uint32_t fps_window_frames = 0U;
    double displayed_fps = 60.0;
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
        const auto advance = clock_.advanceBy(
            elapsed, [&](const simulation::TickContext& context) noexcept {
                director_.fixed_update(context);
            });
        director_.set_presentation_timing(advance.first_tick,advance.next_tick,advance.interpolation_alpha);
        const double dt=std::chrono::duration<double>(elapsed).count();director_.frame_update(dt);
        if (dt > 0.0 && dt < 1.0) {
            fps_window_seconds += dt;
            ++fps_window_frames;
            if (options.deterministic) {
                displayed_fps = 1.0 / dt;
            } else if (fps_window_seconds >= 0.25 && fps_window_frames > 0U) {
                displayed_fps = static_cast<double>(fps_window_frames) / fps_window_seconds;
                fps_window_seconds = 0.0;
                fps_window_frames = 0U;
            }
        }
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
        const auto fps_value = displayed_fps > 0.0 ? displayed_fps : 0.0;
        (void)ui_.model().set("fps", std::to_string(static_cast<int>(fps_value + 0.5)) + " FPS");
        rml_ui_->set_model(ui_.model());
        rml_ui_->set_route_models(ui_);
        if (const auto* scale=ui_.model().find("ui_scale"); scale!=nullptr &&
            std::holds_alternative<double>(*scale)) {
            rml_ui_->set_density_ratio(static_cast<float>(std::get<double>(*scale)));
        }
        ui_.replace_frame(rml_ui_->update(dt));
        ui_.set_viewport_metrics(rml_ui_->element_viewport_metrics("unit-viewport"));
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
