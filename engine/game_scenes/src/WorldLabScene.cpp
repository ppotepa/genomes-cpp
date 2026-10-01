#include <genomes/runtime/WorldLabScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>
#include <utility>

namespace genomes::runtime {
namespace {
[[nodiscard]] std::string_view value_of(const ui::UiActionArguments& arguments) {
    for (const auto& argument : arguments)
        if (argument.first == "value") return argument.second;
    return {};
}

[[nodiscard]] bool parse_double(std::string_view text, double& value) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value,
                                        std::chars_format::general);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
           std::isfinite(value);
}
}

foundation::SceneId WorldLabScene::id() const noexcept {
    return foundation::scene_id("scene.world-lab");
}

void WorldLabScene::on_enter(SceneContext&) {
    category_ = "Terrain";
    species_ = "Oak";
    season_ = "Summer";
    detail_ = "World";
    seed_ = 0;
    age_ = 50.0;
    genome_ = 0.5;
    show_vegetation_ = true;
    show_buildings_ = true;
    imported_path_.clear();
}

ui::UiActionResult WorldLabScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments& arguments) {
    const auto value = value_of(arguments);
    if (action == foundation::stable_id("worldlab.category")) {
        if (value != "Terrain" && value != "Flora" && value != "Fauna") return ui::UiActionResult::Rejected;
        category_ = std::string{value};
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.species")) {
        if (value != "Oak" && value != "Pine" && value != "Birch") return ui::UiActionResult::Rejected;
        species_ = std::string{value};
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.season")) {
        if (value != "Summer" && value != "Winter" && value != "Autumn" && value != "Spring") return ui::UiActionResult::Rejected;
        season_ = std::string{value};
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.detail")) {
        if (value != "Far" && value != "World" && value != "High") return ui::UiActionResult::Rejected;
        detail_ = std::string{value};
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.seed")) {
        std::uint64_t parsed{};
        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return ui::UiActionResult::Rejected;
        seed_ = parsed;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.age") ||
        action == foundation::stable_id("worldlab.genome")) {
        double parsed = 0.0;
        if (!parse_double(value, parsed)) return ui::UiActionResult::Rejected;
        if (action == foundation::stable_id("worldlab.age")) age_ = std::clamp(parsed, 0.0, 100.0);
        else genome_ = std::clamp(parsed, 0.0, 1.0);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.show-vegetation")) {
        show_vegetation_ = value == "true" || value == "1";
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.show-buildings")) {
        show_buildings_ = value == "true" || value == "1";
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.import")) {
        imported_path_ = std::string{value};
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("worldlab.export")) {
        // Export is a prototype command; the native file dialog supplies a
        // destination later, while no world simulation state is changed here.
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("scene.return-main-menu")) {
        application::enqueueApplicationCommand(context,
                                                application::ApplicationCommandKind::ReturnToMainMenu);
        return ui::UiActionResult::Handled;
    }
    return ui::UiActionResult::Unknown;
}

void WorldLabScene::frame_update(SceneContext& context, double) {
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"World laboratory"});
    (void)model.set("description", std::string{"Session-only environment controls."});
    (void)model.set("prototype_status", std::string{"Prototype UI — not applied to world simulation"});
    (void)model.set("category", category_);
    (void)model.set("species", species_);
    (void)model.set("season", season_);
    (void)model.set("detail", detail_);
    (void)model.set("seed", static_cast<std::int64_t>(seed_));
    (void)model.set("age", age_);
    (void)model.set("genome", genome_);
    (void)model.set("show_vegetation", show_vegetation_);
    (void)model.set("show_buildings", show_buildings_);
    (void)model.set("imported_path", imported_path_);

    ui::UiFieldState category{};
    category.value = category_;
    category.options = {{"Terrain", "Terrain", true}, {"Flora", "Flora", true}, {"Fauna", "Fauna", true}};
    category.commit_policy = ui::UiCommitPolicy::OnChange;
    (void)model.set_field("category", std::move(category));
    ui::UiFieldState species{};
    species.value = species_;
    species.options = {{"Oak", "Oak", true}, {"Pine", "Pine", true}, {"Birch", "Birch", true}};
    species.commit_policy = ui::UiCommitPolicy::OnChange;
    (void)model.set_field("species", std::move(species));
    ui::UiFieldState season{};
    season.value = season_;
    season.options = {{"Spring", "Spring", true}, {"Summer", "Summer", true}, {"Autumn", "Autumn", true}, {"Winter", "Winter", true}};
    season.commit_policy = ui::UiCommitPolicy::OnChange;
    (void)model.set_field("season", std::move(season));
    ui::UiFieldState age{};
    age.value = age_; age.minimum = 0.0; age.maximum = 100.0; age.step = 1.0;
    age.commit_policy = ui::UiCommitPolicy::Live;
    (void)model.set_field("age", std::move(age));
    ui::UiFieldState genome{};
    genome.value = genome_; genome.minimum = 0.0; genome.maximum = 1.0; genome.step = 0.05;
    genome.commit_policy = ui::UiCommitPolicy::Live;
    (void)model.set_field("genome", std::move(genome));
}

} // namespace genomes::runtime
