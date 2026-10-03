#include <RmlUiRuntime.hpp>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <genomes/ui/UiRuntime.hpp>

#include <algorithm>
#include <cassert>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <tuple>
#include <vector>

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
    auto* gallery_document = runtime.context()->GetDocument(0);
    assert(gallery_document != nullptr);
    Rml::ElementList action_elements;
    gallery_document->QuerySelectorAll(action_elements,
                                       "[data-action=\"scene.start-battlefield\"]");
    assert(action_elements.size() == 1U);
    auto* nested_label = action_elements.front()->QuerySelector("span");
    assert(nested_label != nullptr);
    const auto action_position = nested_label->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto action_size = nested_label->GetBox().GetSize(Rml::BoxArea::Border);
    const float action_x = action_position.x + action_size.x * 0.5F;
    const float action_y = action_position.y + action_size.y * 0.5F;
    input.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                            action_x, action_y, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                            action_x, action_y, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonUp, 0, 0, 1,
                            action_x, action_y, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::KeyDown, 4, 0, 0,
                            0.0F, 0.0F, 0.0F, 0.0F, {}});
    const auto filtered = runtime.filter_input(input);
    assert(router.calls > 0);
    assert(router.last_action == genomes::foundation::stable_id("scene.start-battlefield"));
    bool kept_key = false;
    for (const auto& event : filtered.events) {
        kept_key = kept_key ||
            (event.type == genomes::input::EventType::KeyDown && event.scancode == 4);
    }
    assert(kept_key);

    auto* disclosure = gallery_document->QuerySelector("[data-disclosure]");
    assert(disclosure != nullptr);
    auto* gallery = gallery_document->QuerySelector(".gallery");
    assert(gallery != nullptr);
    const auto initial_disclosure_position = disclosure->GetAbsoluteOffset(Rml::BoxArea::Border);
    gallery->SetScrollTop(std::max(0.0F, initial_disclosure_position.y - 160.0F));
    (void)runtime.update(1.0 / 60.0);
    const auto disclosure_position = disclosure->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto disclosure_size = disclosure->GetBox().GetSize(Rml::BoxArea::Border);
    genomes::input::InputFrame disclosure_click{};
    const float disclosure_x = disclosure_position.x + disclosure_size.x * 0.5F;
    const float disclosure_y = disclosure_position.y + disclosure_size.y * 0.5F;
    disclosure_click.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                       disclosure_x, disclosure_y, 0.0F, 0.0F, {}});
    disclosure_click.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                                       disclosure_x, disclosure_y, 0.0F, 0.0F, {}});
    disclosure_click.events.push_back({genomes::input::EventType::MouseButtonUp, 0, 0, 1,
                                       disclosure_x, disclosure_y, 0.0F, 0.0F, {}});
    (void)runtime.filter_input(disclosure_click);
    assert(!disclosure->GetAttribute<bool>("aria-expanded", true));
    auto* disclosure_container = disclosure->GetParentNode()->GetParentNode();
    assert(disclosure_container != nullptr);
    auto* disclosure_body = disclosure_container->QuerySelector(".disclosure-body");
    assert(disclosure_body != nullptr);
    assert(disclosure_body->GetProperty("display")->ToString() == "none");

    genomes::input::InputFrame viewport_drag{};
    viewport_drag.mouse_right_down = true;
    const auto gallery_position = gallery->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto gallery_size = gallery->GetBox().GetSize(Rml::BoxArea::Border);
    const float drag_x = std::min(1270.0F, gallery_position.x + gallery_size.x + 12.0F);
    const float drag_y = gallery_position.y + gallery_size.y * 0.5F;
    viewport_drag.mouse_x = drag_x;
    viewport_drag.mouse_y = drag_y;
    viewport_drag.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 3,
                                    drag_x, drag_y, 0.0F, 0.0F, {}});
    viewport_drag.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                    drag_x + 20.0F, drag_y - 12.0F, 20.0F, -12.0F, {}});
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
        RouteFixture{"scenes/infantry-mass-battle/screen.rml", "builtin.battlefield"},
        RouteFixture{"scenes/unit-lab/screen.rml", "builtin.unit-lab"},
        RouteFixture{"scenes/building-lab/screen.rml", "builtin.building-lab"},
        RouteFixture{"scenes/world-lab/screen.rml", "builtin.world-lab"},
        RouteFixture{"scenes/settings/screen.rml", "builtin.settings"},
        RouteFixture{"scenes/pause/screen.rml", "builtin.pause"},
    };
    genomes::ui::UiRuntime model_source;
    (void)model_source.model().set("title", std::string{"Smoke"});
    (void)model_source.model().set("description", std::string{"Smoke"});
    (void)model_source.model().set("profile", std::string{"Balanced"});
    (void)model_source.model().set("animation_mode", std::string{"atlas"});
    (void)model_source.model().set("orientation", std::string{"model +Z | blue +X | red -X"});
    (void)model_source.model().set("version", std::string{"Smoke"});
    (void)model_source.model().set("note", std::string{"Smoke"});
    (void)model_source.model().set("fps", std::string{"60 FPS"});
    (void)model_source.model().set("scene_loading_phase", std::string{"completed"});
    (void)model_source.model().set("scene_loading_active", false);
    (void)model_source.model().set("scene_loading_failed", false);
    (void)model_source.model().set("scene_loading_progress", 1.0);
    (void)model_source.model().set("scene_loading_message", std::string{"Scene ready"});
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
                            "tab_model", "expression_intensity_enabled"})
        (void)model_source.model().set(key, true);
    for (const auto* key : {"tab_equipment", "tab_genome", "tab_skeleton",
                            "tab_animation", "tab_face"})
        (void)model_source.model().set(key, false);
    (void)model_source.model().set("wear", 0.0);
    (void)model_source.model().set("animation_speed", 1.0);
    (void)model_source.model().set("animation_phase", 0.0);
    (void)model_source.model().set("expression_intensity", 0.0);
    (void)model_source.model().set_list("parts", {});
    (void)model_source.model().set_list("rooms", {});
    (void)model_source.model().set_list("parts_options", {});
    (void)model_source.model().set("show_vegetation", true);
    (void)model_source.model().set("show_buildings", true);
    std::vector<genomes::ui::UiTableRow> long_genes;
    for (int index = 0; index < 72; ++index)
        long_genes.push_back({{"id", "gene_" + std::to_string(index)},
                              {"group", index < 2 ? "Root" : index < 48 ? "Body" : "Face"},
                              {"label", "Gene " + std::to_string(index)},
                              {"value", static_cast<double>(index) / 72.0},
                              {"enabled", true}, {"selected", false}, {"overridden", index == 3}});
    (void)model_source.model().set_list("genes", std::move(long_genes));
    std::vector<genomes::ui::UiTableRow> long_slots;
    std::vector<genomes::ui::UiTableRow> long_items;
    for (int index = 0; index < 24; ++index) {
        const auto id = "slot_" + std::to_string(index);
        long_slots.push_back({{"id", id}, {"group", "Apparel"},
                              {"label", "Slot " + std::to_string(index)}, {"value", "auto"},
                              {"enabled", true}, {"selected", false}});
        long_items.push_back({{"id", "auto"}, {"group", id}, {"label", "Auto"},
                              {"value", "auto"}, {"enabled", true}, {"selected", true}});
    }
    (void)model_source.model().set_list("equipment_slots", std::move(long_slots));
    (void)model_source.model().set_list("equipment_items", std::move(long_items));
    (void)model_source.model().set_list("loadouts", {});
    std::vector<genomes::ui::UiTableRow> long_bones;
    for (int index = 0; index < 72; ++index)
        long_bones.push_back({{"id", std::to_string(index)}, {"group", "Hierarchy"},
                              {"label", "Bone " + std::to_string(index)},
                              {"value", "parent: root"}, {"enabled", true},
                              {"selected", false}, {"overridden", false}});
    (void)model_source.model().set_list("bones", std::move(long_bones));
    for (const auto& fixture : routes) {
        for (const auto& [width, height, scale] : {
                std::tuple{1280, 720, 0.75F}, std::tuple{1280, 720, 1.0F},
                std::tuple{1280, 720, 1.5F}, std::tuple{1920, 1080, 0.75F},
                std::tuple{1920, 1080, 1.0F}, std::tuple{1920, 1080, 1.5F}}) {
            runtime.resize(width, height);
            runtime.set_density_ratio(scale);
            assert(runtime.mount_routes({genomes::ui::UiRoute{
                genomes::foundation::stable_id(fixture.document), fixture.document,
                fixture.controller, {}, false, 1}}, model_source));
            runtime.set_model(model_source.model());
            runtime.set_route_models(model_source);
            (void)runtime.update(1.0 / 60.0);
            assert(!runtime.has_diagnostics_errors());
            auto* route_document = runtime.context()->GetDocument(0);
            assert(route_document != nullptr);
            auto* route_main = route_document->QuerySelector("main");
            assert(route_main != nullptr && route_main->IsVisible(true));
            auto* fps_readout = route_document->QuerySelector(".app-fps");
            assert(fps_readout != nullptr &&
                   fps_readout->GetInnerRML().find("60 FPS") != std::string::npos);
            if (std::string_view{fixture.document}.find("infantry-mass-battle/") !=
                std::string_view::npos) {
                assert(route_document->GetElementById("infantry-mass-battle-hud") != nullptr);
                assert(route_document->QuerySelector("#main-menu") == nullptr);
                assert(route_document->GetInnerRML().find("{{ version }}") ==
                       std::string::npos);
                for (const auto* binding : {"{{ fps }}", "{{ status }}", "{{ features }}",
                                             "{{ seed }}", "{{ map_size }}",
                                             "{{ scene_loading_phase }}",
                                             "{{ scene_loading_message }}"}) {
                    assert(route_document->GetInnerRML().find(binding) == std::string::npos);
                }
            }
            const auto route_position = route_main->GetAbsoluteOffset(Rml::BoxArea::Border);
            const auto route_size = route_main->GetBox().GetSize(Rml::BoxArea::Border);
            assert(route_size.x > 0.0F && route_size.y > 0.0F);
            assert(route_position.x >= 0.0F && route_position.y >= 0.0F);
            assert(route_position.x + route_size.x <= static_cast<float>(width) + 0.5F);
            assert(route_position.y + route_size.y <= static_cast<float>(height) + 0.5F);
            if (std::string_view{fixture.document}.find("settings/") != std::string_view::npos) {
                auto* scale_control = route_document->QuerySelector(
                    "[data-control=\"settings.ui-scale\"]");
                assert(scale_control != nullptr);
                auto* scale_output = scale_control->GetParentNode()->QuerySelector("output");
                assert(scale_output != nullptr && !scale_output->GetInnerRML().empty());
            }
        }
    }

    struct Rect { float x; float y; float width; float height; };
    const auto overlaps = [](const Rect& a, const Rect& b) {
        return a.x < b.x + b.width && b.x < a.x + a.width &&
               a.y < b.y + b.height && b.y < a.y + a.height;
    };
    for (const auto& [width, height, scale] : {
            std::tuple{1280, 720, 0.75F}, std::tuple{1280, 720, 1.0F},
            std::tuple{1280, 720, 1.5F}, std::tuple{1920, 1080, 0.75F},
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
        const float chrome_scale = scale;
        assert(regions[0].height <= 46.0F * chrome_scale + 0.5F);
        assert(regions[1].width <= 52.0F * chrome_scale + 0.5F);
        assert(regions[2].height <= 68.0F * chrome_scale + 0.5F);
        assert(regions[3].width <= 380.0F * chrome_scale + 0.5F);
        assert(regions[4].height <= 28.0F * chrome_scale + 0.5F);
        auto* detail = document->QuerySelector("[data-control=\"unit.detail\"]");
        auto* camera = document->QuerySelector("[data-control=\"unit.camera\"]");
        assert(detail != nullptr && camera != nullptr);
        const auto detail_x = detail->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        const auto camera_x = camera->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        assert(std::abs(detail_x - camera_x) < 0.5F);
        // Nested disclosures must not introduce another horizontal inset.
        // Each visible field keeps the label and control inside its own row.
        Rml::ElementList aligned_fields;
        document->QuerySelectorAll(aligned_fields, ".unit-inspector .field");
        for (auto* field : aligned_fields) {
            if (!field->IsVisible(true)) continue;
            auto* label = field->QuerySelector(".field-label");
            auto* control = field->QuerySelector(".field-control");
            if (control == nullptr) control = field->QuerySelector(".range-row");
            assert(label != nullptr && control != nullptr);
            const auto row_position = field->GetAbsoluteOffset(Rml::BoxArea::Border);
            const auto row_size = field->GetBox().GetSize(Rml::BoxArea::Border);
            const auto control_position = control->GetAbsoluteOffset(Rml::BoxArea::Border);
            const auto control_size = control->GetBox().GetSize(Rml::BoxArea::Border);
            assert(std::abs(control_position.x - row_position.x - 104.0F * scale) <= 1.0F);
            assert(control_size.x > 0.0F);
            assert(control_position.x + control_size.x <= row_position.x + row_size.x + 1.0F);
        }
        auto* status = document->QuerySelector(".status-chip");
        assert(status != nullptr);
        const auto status_position = status->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto status_size = status->GetBox().GetSize(Rml::BoxArea::Border);
        assert(status_position.x + status_size.x <=
               regions[2].x + regions[2].width + 0.5F);
        assert(regions[2].x + regions[2].width - status_position.x - status_size.x <=
               14.0F * chrome_scale + 0.5F);
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

    // Text interpolation must render initial values and react to later model
    // updates. This covers the bindings that previously produced empty nodes.
    auto* value_document = runtime.context()->GetDocument(0);
    assert(value_document != nullptr);
    auto* compact_status = value_document->QuerySelector(".status-chip");
    assert(compact_status != nullptr);
    assert(compact_status->GetInnerRML().find("Smoke") != std::string::npos);
    (void)model_source.model().set("status_compact", std::string{"Ready"});
    runtime.set_model(model_source.model());
    (void)runtime.update(1.0 / 60.0);
    assert(compact_status->GetInnerRML().find("Ready") != std::string::npos);

    for (const char* tab : {"tab_model", "tab_equipment", "tab_genome", "tab_skeleton",
                            "tab_animation", "tab_face"})
        (void)model_source.model().set(tab, std::string{tab} == "tab_equipment");
    (void)model_source.model().set("wear", 0.35);
    runtime.set_model(model_source.model());
    (void)runtime.update(1.0 / 60.0);
    auto* wear_control = value_document->QuerySelector("[data-control=\"unit.wear\"]");
    assert(wear_control != nullptr);
    auto* wear_output = wear_control->GetParentNode()->QuerySelector("output");
    assert(wear_output != nullptr && !wear_output->GetInnerRML().empty());

    for (const char* tab : {"tab_model", "tab_equipment", "tab_genome", "tab_skeleton",
                            "tab_animation", "tab_face"})
        (void)model_source.model().set(tab, std::string{tab} == "tab_animation");
    (void)model_source.model().set("animation_speed", 1.25);
    (void)model_source.model().set("animation_phase", 0.4);
    (void)model_source.model().set("pause_label", std::string{"Resume"});
    int phase_commands = 0;
    runtime.set_event_router([&phase_commands](const genomes::ui::UiEvent& event) {
        if (event.control == "unit.phase") ++phase_commands;
        return genomes::ui::UiActionResult::Handled;
    });
    runtime.set_model(model_source.model());
    (void)runtime.update(1.0 / 60.0);
    assert(phase_commands == 0); // Playback must not seek/pause itself via data-value.
    for (double phase : {0.45123, 0.50789, 0.55432}) {
        (void)model_source.model().set("animation_phase", phase);
        runtime.set_model(model_source.model());
        (void)runtime.update(1.0 / 60.0);
        assert(phase_commands == 0);
    }
    auto* phase_control = value_document->QuerySelector("[data-control=\"unit.phase\"]");
    assert(phase_control != nullptr);
    Rml::Dictionary phase_change;
    phase_change["value"] = Rml::Variant{0.4F};
    phase_control->DispatchEvent("change", phase_change);
    assert(phase_commands == 1); // Explicit user/control events still reach the router.
    runtime.set_event_router({});
    for (const char* control_name : {"unit.animation-speed", "unit.phase"}) {
        auto* control = value_document->QuerySelector(
            (std::string{"[data-control=\""} + control_name + "\"]").c_str());
        assert(control != nullptr);
        auto* output = control->GetParentNode()->QuerySelector("output");
        assert(output != nullptr && !output->GetInnerRML().empty());
    }
    auto* pause = value_document->QuerySelector("[data-action=\"unit.pause\"]");
    assert(pause != nullptr && pause->GetInnerRML().find("Resume") != std::string::npos);

    // Exercise the long inspector sections on the three data-heavy tabs,
    // not just the default Model tab. Each tab must keep one bounded scroll
    // owner and remain measurable after a disclosure is opened.
    auto* tab_document = runtime.context()->GetDocument(0);
    assert(tab_document != nullptr);
    auto* tab_scroll = tab_document->QuerySelector(".unit-inspector-scroll");
    assert(tab_scroll != nullptr);
    for (const char* active_tab : {"tab_equipment", "tab_genome", "tab_skeleton"}) {
        for (const char* tab : {"tab_model", "tab_equipment", "tab_genome", "tab_skeleton",
                                "tab_animation", "tab_face"})
            (void)model_source.model().set(tab, std::string{tab} == active_tab);
        runtime.set_model(model_source.model());
        (void)runtime.update(1.0 / 60.0);
        auto* section = tab_document->QuerySelector(
            (std::string{"section[data-if=\""} + active_tab + "\"]").c_str());
        assert(section != nullptr && section->IsVisible(true));
        Rml::ElementList section_disclosures;
        section->QuerySelectorAll(section_disclosures, "details.disclosure");
        assert(section_disclosures.size() >= 2U);
        // The second disclosure is the intentionally long list on each tab:
        // apparel, genome overrides, or rig hierarchy respectively.
        auto* section_disclosure = section_disclosures[1];
        section_disclosure->SetAttribute("open", "open");
        if (auto* body = section_disclosure->QuerySelector(".disclosure-body"); body != nullptr)
            body->SetProperty("display", "block");
        (void)runtime.update(1.0 / 60.0);
        tab_scroll->SetScrollTop(0.0F);
        assert(tab_scroll->GetScrollHeight() > tab_scroll->GetClientHeight());
        assert(tab_scroll->GetScrollTop() <=
               tab_scroll->GetScrollHeight() - tab_scroll->GetClientHeight() + 0.5F);
        if (std::string_view{active_tab} == "tab_equipment") {
            Rml::ElementList equipment_selects;
            section->QuerySelectorAll(equipment_selects,
                                      "select[data-control=\"unit.equipment-item\"]");
            assert(equipment_selects.size() == 24U);
            for (auto* select : equipment_selects) {
                auto* select_control = dynamic_cast<Rml::ElementFormControlSelect*>(select);
                // The structural data-for template remains in the tree and
                // is hidden; twenty-four generated options follow it.
                assert(select_control != nullptr &&
                       select_control->GetNumOptions() == 25);
                std::size_t visible_options = 0U;
                for (int option_index = 0;
                     option_index < select_control->GetNumOptions(); ++option_index) {
                    auto* option = select_control->GetOption(option_index);
                    assert(option != nullptr);
                    visible_options += option->GetLocalStyleProperties().count(
                        Rml::PropertyId::Display) == 0U ? 1U : 0U;
                }
                // Every generated select receives the complete stable list,
                // but its data-if predicate must leave only its own slot.
                assert(visible_options == 1U);
            }
        }
    }

    // A wheel over the inspector is consumed by RmlUi and advances the one
    // shared scroll container. The same wheel over the viewport remains in
    // the filtered frame for SceneDirector camera zoom.
    auto* document = runtime.context()->GetDocument(0);
    assert(document != nullptr);
    Rml::ElementList scroll_elements;
    document->GetElementsByClassName(scroll_elements, "unit-inspector-scroll");
    assert(scroll_elements.size() == 1U);
    auto* inspector_scroll = scroll_elements.front();
    assert(inspector_scroll->GetScrollHeight() > inspector_scroll->GetClientHeight());
    inspector_scroll->SetScrollTop(0.0F);
    const auto inspector_position = inspector_scroll->GetAbsoluteOffset(Rml::BoxArea::Border);
    genomes::input::InputFrame hud_wheel{};
    hud_wheel.mouse_x = inspector_position.x + 16.0F;
    hud_wheel.mouse_y = inspector_position.y + 24.0F;
    hud_wheel.events.push_back({genomes::input::EventType::MouseWheel, 0, 0, 0,
                                hud_wheel.mouse_x, hud_wheel.mouse_y, 0.0F, 4.0F, {}});
    const auto hud_wheel_filtered = runtime.filter_input(hud_wheel);
    (void)runtime.update(1.0 / 60.0);
    assert(inspector_scroll->GetScrollTop() > 0.0F);
    assert(hud_wheel_filtered.mouse_wheel_y == 0.0F);
    assert(hud_wheel_filtered.pointer_over_ui);

    genomes::input::InputFrame viewport_wheel{};
    viewport_wheel.mouse_x = 700.0F;
    viewport_wheel.mouse_y = 400.0F;
    viewport_wheel.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                     viewport_wheel.mouse_x, viewport_wheel.mouse_y,
                                     0.0F, 0.0F, {}});
    viewport_wheel.events.push_back({genomes::input::EventType::MouseWheel, 0, 0, 0,
                                     viewport_wheel.mouse_x, viewport_wheel.mouse_y,
                                     0.0F, 1.0F, {}});
    const auto viewport_wheel_filtered = runtime.filter_input(viewport_wheel);
    assert(viewport_wheel_filtered.mouse_wheel_y == 1.0F);
    assert(!viewport_wheel_filtered.pointer_over_ui);
    bool viewport_wheel_reached_camera = false;
    for (const auto& event : viewport_wheel_filtered.events)
        viewport_wheel_reached_camera = viewport_wheel_reached_camera ||
            event.type == genomes::input::EventType::MouseWheel;
    assert(viewport_wheel_reached_camera);

    genomes::input::InputFrame hud_press{};
    hud_press.mouse_left_down = true;
    hud_press.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                                1800.0F, 220.0F, 0.0F, 0.0F, {}});
    const auto hud_filtered = runtime.filter_input(hud_press);
    assert(!hud_filtered.mouse_left_down);
    assert(hud_filtered.events.empty());
    assert(hud_filtered.pointer_over_ui);

    genomes::input::InputFrame scene_press{};
    scene_press.mouse_left_down = true;
    scene_press.mouse_left_pressed = true;
    scene_press.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                                  700.0F, 400.0F, 0.0F, 0.0F, {}});
    const auto scene_filtered = runtime.filter_input(scene_press);
    assert(scene_filtered.mouse_left_down && scene_filtered.mouse_left_pressed);
    assert(!scene_filtered.pointer_over_ui);

    genomes::input::InputFrame captured_drag{};
    captured_drag.mouse_left_down = true;
    captured_drag.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                                    1800.0F, 220.0F, 18.0F, -9.0F, {}});
    const auto drag_filtered = runtime.filter_input(captured_drag);
    assert(drag_filtered.mouse_left_down);
    assert(drag_filtered.mouse_delta_x == 18.0F && drag_filtered.mouse_delta_y == -9.0F);
    return 0;
}
