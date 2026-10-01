#include <RmlUiRuntime.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cassert>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <tuple>

class TestRouter final : public genomes::ui::IUiActionRouter {
public:
    genomes::ui::UiActionResult dispatch(
        genomes::ui::UiActionId action,
        const genomes::ui::UiActionArguments&) override {
        last_action = action;
        ++calls;
        return genomes::ui::UiActionResult::Handled;
    }

    genomes::ui::UiActionId last_action{0};
    int calls{0};
};

int main() {
    genomes::ui::rml::Runtime runtime{
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods" / "core", 1280, 720};
    assert(runtime.valid());
    TestRouter router;
    runtime.set_action_router(&router);
    assert(runtime.load_document("ui-test.rml"));
    const auto& frame = runtime.update(1.0 / 60.0);
    assert(frame.revision > 0);
    assert(!frame.commands.empty());
    assert(!frame.commands.front().vertices.empty());
    assert(!frame.commands.front().indices.empty());

    genomes::input::InputFrame input{};
    input.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonUp, 0, 0, 1,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    input.mouse_left_pressed = true;
    input.events.push_back({genomes::input::EventType::KeyDown, 4, 0, 0,
                            0.0F, 0.0F, 0.0F, 0.0F, {}});
    const auto filtered = runtime.filter_input(input);
    assert(router.calls > 0);
    assert(router.last_action == genomes::foundation::stable_id("scene.start-battlefield"));
    assert(!filtered.mouse_left_pressed);
    bool kept_key = false;
    bool kept_release = false;
    bool kept_press = false;
    for (const auto& event : filtered.events) {
        kept_key = kept_key ||
            (event.type == genomes::input::EventType::KeyDown && event.scancode == 4);
        kept_release = kept_release ||
            event.type == genomes::input::EventType::MouseButtonUp;
        kept_press = kept_press ||
            event.type == genomes::input::EventType::MouseButtonDown;
    }
    assert(kept_key);
    assert(kept_release);
    assert(!kept_press);

    genomes::input::InputFrame viewport_drag{};
    viewport_drag.mouse_right_down = true;
    viewport_drag.mouse_x = 1100.0F;
    viewport_drag.mouse_y = 360.0F;
    viewport_drag.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 3,
                                    1100.0F, 360.0F, 0.0F, 0.0F, {}});
    viewport_drag.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                    1120.0F, 348.0F, 20.0F, -12.0F, {}});
    const auto viewport_filtered = runtime.filter_input(viewport_drag);
    assert(viewport_filtered.mouse_right_down);
    assert(viewport_filtered.mouse_delta_x == 20.0F);
    assert(viewport_filtered.mouse_delta_y == -12.0F);

    // Every production route must parse with the basic renderer contract. A
    // route model is mounted here because scene documents deliberately bind
    // to their manifest-owned data model names.
    struct RouteFixture { const char* document; const char* controller; };
    constexpr std::array routes{
        RouteFixture{"scenes/main-menu/screen.rml", "builtin.main-menu"},
        RouteFixture{"scenes/world-config/screen.rml", "builtin.world-config"},
        RouteFixture{"scenes/battlefield/screen.rml", "builtin.battlefield"},
        RouteFixture{"scenes/unit-lab/screen.rml", "builtin.unit-lab"},
        RouteFixture{"scenes/building-lab/screen.rml", "builtin.building-lab"},
        RouteFixture{"scenes/world-lab/screen.rml", "builtin.world-lab"},
        RouteFixture{"scenes/settings/screen.rml", "builtin.settings"},
        RouteFixture{"scenes/pause/screen.rml", "builtin.pause"},
    };
    genomes::ui::UiRuntime model_source;
    (void)model_source.model().set("title", std::string{"Smoke"});
    (void)model_source.model().set("description", std::string{"Smoke"});
    (void)model_source.model().set("version", std::string{"Smoke"});
    (void)model_source.model().set("note", std::string{"Smoke"});
    (void)model_source.model().set("selected", std::string{"Battlefield"});
    genomes::ui::UiFieldState map_size{};
    map_size.value = std::int64_t{600};
    map_size.options = {{"400", "400m", true}, {"600", "600m", true}};
    (void)model_source.model().set_field("map_size", std::move(map_size));
    (void)model_source.model().set("seed", std::int64_t{42});
    (void)model_source.model().set("vegetation", 0.5);
    (void)model_source.model().set("buildings", 0.5);
    (void)model_source.model().set("fenced_parcels", 0.5);
    for (const auto* key : {"status", "diagnostics", "features", "error", "damage_amount",
                            "integrity", "selected_part", "detail", "variation", "age",
                            "genome", "imported_path", "prototype_status", "season", "species",
                            "vegetation_percent", "buildings_percent", "fenced_parcels_percent",
                            "ui_scale_percent"})
        (void)model_source.model().set(key, std::string{"Smoke"});
    for (const auto* key : {"metrics", "equipment_seed", "ragdoll_readout",
                            "status_compact", "pause_label"})
        (void)model_source.model().set(key, std::string{"Smoke"});
    for (const auto* key : {"surface", "wireframe", "skeleton", "bounds", "normals", "auto_rotate",
                            "tab_model", "tab_equipment", "tab_genome", "tab_skeleton",
                            "tab_animation", "tab_face", "expression_intensity_enabled"})
        (void)model_source.model().set(key, true);
    (void)model_source.model().set("wear", 0.0);
    (void)model_source.model().set("animation_speed", 1.0);
    (void)model_source.model().set("animation_phase", 0.0);
    (void)model_source.model().set("expression_intensity", 0.0);
    for (const auto* key : {"parts", "rooms", "genes", "equipment_slots",
                            "equipment_items", "loadouts", "bones"})
        (void)model_source.model().set_list(key, {});
    for (const auto& fixture : routes) {
        assert(runtime.mount_routes({genomes::ui::UiRoute{
            genomes::foundation::stable_id(fixture.document), fixture.document,
            fixture.controller, {}, false, 1}}, model_source));
        runtime.set_model(model_source.model());
        runtime.set_route_models(model_source);
        (void)runtime.update(1.0 / 60.0);
        assert(!runtime.has_diagnostics_errors());
        assert(runtime.diagnostics().empty());
    }

    struct Rect { float x; float y; float width; float height; };
    const auto overlaps = [](const Rect& a, const Rect& b) {
        return a.x < b.x + b.width && b.x < a.x + a.width &&
               a.y < b.y + b.height && b.y < a.y + a.height;
    };
    for (const auto& [width, height, scale] : {
            std::tuple{1280, 720, 1.0F}, std::tuple{1280, 720, 1.5F},
            std::tuple{1920, 1080, 1.0F}, std::tuple{1920, 1080, 1.5F}}) {
        runtime.resize(width, height);
        runtime.set_density_ratio(scale);
        assert(runtime.mount_routes({genomes::ui::UiRoute{
            genomes::foundation::stable_id("unit-layout"),
            "scenes/unit-lab/screen.rml", "builtin.unit-lab", {}, false, 1}},
            model_source));
        runtime.set_model(model_source.model());
        runtime.set_route_models(model_source);
        (void)runtime.update(1.0 / 60.0);
        auto* document = runtime.context()->GetDocument(0);
        assert(document != nullptr);
        const auto measure = [document, width, height](const char* id) {
            auto* element = document->GetElementById(id);
            assert(element != nullptr && element->IsVisible(true));
            const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
            const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
            assert(size.x > 0.0F && size.y > 0.0F);
            assert(position.x >= 0.0F && position.y >= 0.0F);
            assert(position.x + size.x <= static_cast<float>(width) + 0.5F);
            assert(position.y + size.y <= static_cast<float>(height) + 0.5F);
            return Rect{position.x, position.y, size.x, size.y};
        };
        const std::array regions{
            measure("unit-header"), measure("unit-rail"), measure("unit-toolbar"),
            measure("unit-inspector"), measure("unit-caption")};
        const auto viewport = measure("unit-viewport");
        const auto measured_viewport = runtime.element_viewport_metrics("unit-viewport");
        assert(measured_viewport);
        assert(std::abs(measured_viewport->left - viewport.x) < 1.0e-4F);
        assert(std::abs(measured_viewport->top - viewport.y) < 1.0e-4F);
        assert(std::abs(measured_viewport->width - viewport.width) < 1.0e-4F);
        assert(std::abs(measured_viewport->height - viewport.height) < 1.0e-4F);
        for (std::size_t a = 0; a < regions.size(); ++a)
            for (std::size_t b = a + 1; b < regions.size(); ++b)
                assert(!overlaps(regions[a], regions[b]));
        for (const auto& region : regions) assert(!overlaps(viewport, region));
        assert(!runtime.has_diagnostics_errors());
    }

    genomes::input::InputFrame hud_press{};
    hud_press.mouse_left_down = true;
    hud_press.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                                1800.0F, 220.0F, 0.0F, 0.0F, {}});
    const auto hud_filtered = runtime.filter_input(hud_press);
    assert(!hud_filtered.mouse_left_down);
    assert(hud_filtered.events.empty());

    genomes::input::InputFrame scene_press{};
    scene_press.mouse_left_down = true;
    scene_press.mouse_left_pressed = true;
    scene_press.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                                  700.0F, 400.0F, 0.0F, 0.0F, {}});
    const auto scene_filtered = runtime.filter_input(scene_press);
    assert(scene_filtered.mouse_left_down && scene_filtered.mouse_left_pressed);

    genomes::input::InputFrame captured_drag{};
    captured_drag.mouse_left_down = true;
    captured_drag.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                    1800.0F, 220.0F, 18.0F, -9.0F, {}});
    const auto drag_filtered = runtime.filter_input(captured_drag);
    assert(drag_filtered.mouse_left_down);
    assert(drag_filtered.mouse_delta_x == 18.0F && drag_filtered.mouse_delta_y == -9.0F);
    return 0;
}
