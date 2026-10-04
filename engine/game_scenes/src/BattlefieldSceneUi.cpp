#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include "BattlefieldSceneDetail.hpp"

#include <genomes/foundation/StableHash.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/EquipmentCatalog.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace genomes::game_scenes {

using namespace battlefield_detail;

#if GENOMES_HAS_INFANTRY
void BattlefieldScene::set_mass_battle_profile(
    MassBattlePresentationProfile profile) noexcept {
    mass_battle_profile_ = profile;
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) return;
    // Profile changes are presentation-only.  The runtime, entity positions
    // and per-unit animation phase are deliberately left untouched.
    if (profile == MassBattlePresentationProfile::Quality ||
        !mass_battle_pose_atlas_ready_) {
        for (auto& agent : animation_agents_) agent.lod.setTier(infantry::AnimationLOD::Near);
    }
}
#endif

std::size_t BattlefieldScene::massBattleUnitCount() const noexcept {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) return 0U;
#if GENOMES_HAS_INFANTRY
    if (mass_battle_session_ != nullptr)
        return mass_battle_session_->presentationSnapshot().states.size();
#endif
    // The configured population is also available while the scene is loading.
    return 2U * MassBattleUnitsPerTeam;
}

std::size_t BattlefieldScene::liveAnimationBudget() const noexcept {
    return std::min(live_animation_budget_, massBattleUnitCount());
}

ui::UiActionResult BattlefieldScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments&) {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) {
        return ui::UiActionResult::Unknown;
    }
    const auto submit_world_regenerate = [&context, this]() {
        if (simulation_facade_ == nullptr) return false;
        const auto view = simulation_facade_->snapshotView();
        api::CommandEnvelope command{};
        command.module = foundation::stable_id("module.world");
        command.verb = foundation::stable_id("world.regenerate");
        command.target_tick = foundation::SimulationTick{view.tick.value + 1U};
        command.source = foundation::stable_id("ui.mass-battle");
        command.stable_order = foundation::stableHashCombine(command.source, command.verb);
        command.payload = {api::EncodedValue::CurrentFormat, api::ValueType::Bytes, {0U}};
        return context.submitCommand(std::move(command)).accepted;
    };
    if (action == foundation::stable_id("mass-battle.restart")) {
        if (simulation_facade_ != nullptr) {
            const auto view = simulation_facade_->snapshotView();
            api::CommandEnvelope command{};
            command.module = foundation::stable_id("module.infantry");
            command.verb = foundation::stable_id("battlefield.restart");
            command.target_tick = foundation::SimulationTick{view.tick.value + 1U};
            command.source = foundation::stable_id("ui.mass-battle");
            command.stable_order = foundation::stableHashCombine(command.source, command.verb);
            command.payload = {api::EncodedValue::CurrentFormat, api::ValueType::Bytes, {0U}};
            if (context.submitCommand(std::move(command)).accepted) {
                return ui::UiActionResult::Handled;
            }
        }
        // Loading scenes have no SimulationFacade yet; preserve the
        // application-level fallback for that lifecycle window.
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle, config_);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.generate-new")) {
        if (simulation_facade_ != nullptr) {
            const auto view = simulation_facade_->snapshotView();
            api::CommandEnvelope command{};
            command.module = foundation::stable_id("module.world");
            command.verb = foundation::stable_id("world.regenerate");
            command.target_tick = foundation::SimulationTick{view.tick.value + 1U};
            command.source = foundation::stable_id("ui.mass-battle");
            command.stable_order = foundation::stableHashCombine(command.source, command.verb);
            command.payload = {api::EncodedValue::CurrentFormat, api::ValueType::Bytes, {0U}};
            if (context.submitCommand(std::move(command)).accepted) {
                return ui::UiActionResult::Handled;
            }
        }
        auto next_config = config_;
        next_config.seed = foundation::stableHashCombine(
            config_.seed, foundation::stableHashString("mass-battle.generate-new"));
        if (next_config.seed == 0U) next_config.seed = 1U;
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-terrain")) {
        auto next_config = config_;
        next_config.terrain.preset = nextTerrainPreset(next_config.terrain.preset);
        if (simulation_facade_ != nullptr) {
            config_ = next_config;
            if (submit_world_regenerate()) return ui::UiActionResult::Handled;
        }
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-terrain-detail")) {
        auto next_config = config_;
        next_config.terrain.sample_spacing_m =
            nextTerrainSampleSpacing(next_config.terrain.sample_spacing_m);
        if (simulation_facade_ != nullptr) {
            config_ = next_config;
            if (submit_world_regenerate()) return ui::UiActionResult::Handled;
        }
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-hydrology")) {
        auto next_config = config_;
        next_config.hydrology_mode = nextHydrologyMode(next_config.hydrology_mode);
        if (simulation_facade_ != nullptr) {
            config_ = next_config;
            if (submit_world_regenerate()) return ui::UiActionResult::Handled;
        }
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.frame-battle")) {
        mass_battle_frame_terrain_ = false;
        if (++mass_battle_camera_revision_ == 0U)
            mass_battle_camera_revision_ = MassBattleRtsCameraRevision;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.frame-terrain")) {
        mass_battle_frame_terrain_ = true;
        if (++mass_battle_camera_revision_ == 0U)
            mass_battle_camera_revision_ = MassBattleRtsCameraRevision;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-toggle")) {
        mass_battle_diagnostics_open_ = !mass_battle_diagnostics_open_;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-overview")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Overview;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-terrain")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Terrain;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-hydrology")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Hydrology;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-performance")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Performance;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-units")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Units;
        return ui::UiActionResult::Handled;
    }
#if GENOMES_HAS_INFANTRY
    constexpr std::size_t budget_step = 32U;
    if (action == foundation::stable_id("mass-battle.animation-decrease")) {
        const auto budget = liveAnimationBudget();
        live_animation_budget_ = budget - std::min(budget, budget_step);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.animation-increase")) {
        const auto budget = liveAnimationBudget();
        live_animation_budget_ = budget + std::min(budget_step, massBattleUnitCount() - budget);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.animation-maximum")) {
        live_animation_budget_ = massBattleUnitCount();
        return ui::UiActionResult::Handled;
    }
#endif
    if (action == foundation::stable_id("mass-battle.profile-quality")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Quality);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    if (action == foundation::stable_id("mass-battle.profile-balanced")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Balanced);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    if (action == foundation::stable_id("mass-battle.profile-stress")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Stress);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    return ui::UiActionResult::Unknown;
}

void BattlefieldScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.cancel_pressed || input.confirm_pressed) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

void BattlefieldScene::frame_update(SceneContext& context, double dt) {
#if GENOMES_HAS_INFANTRY
    if (tactical_model_ticket_.valid() && tactical_model_ticket_.complete()) {
        if (const auto generated = tactical_model_ticket_.artifact(); generated) {
            infantry_model_artifact_ = generated->artifact;
        } else {
            generation_error_ = std::string(tactical_model_ticket_.error().message);
            simulation_failed_ = true;
        }
        tactical_model_ticket_ = {};
    }
    if (animation_job_.valid() && animation_job_.isComplete()) {
        const auto error = pending_animation_error_;
        const auto pending = pending_animation_context_;
        const bool job_failed = animation_job_.failed();
        animation_job_ = {};
        pending_animation_error_.reset();
        pending_animation_context_.reset();
        if (error && *error) {
            generation_error_ = std::string((*error)->message);
        } else if (job_failed) {
            generation_error_ = "infantry pose evaluation job failed";
        } else {
            publish_infantry_animation_result();
            if (context.engine_services != nullptr &&
                context.engine_services->telemetry != nullptr && animation_system_.has_value()) {
                context.engine_services->telemetry->animation_duration =
                    animation_system_->lastStats().evaluation_duration;
            }
        }
        if (pending && !simulation_failed_) {
            evaluate_infantry_animation(*pending);
        }
    }
    if (mass_battle_presentation_scheduler_.atlasComplete()) {
        const auto baked = mass_battle_presentation_scheduler_.takeAtlas();
        if (baked) {
            mass_battle_pose_meshes_ = *baked;
            mass_battle_pose_atlas_ready_ = true;
            mass_battle_pose_atlas_failed_ = false;
        } else {
            mass_battle_pose_meshes_.fill(nullptr);
            mass_battle_pose_atlas_ready_ = false;
            mass_battle_pose_atlas_failed_ = true;
        }
    }
    advance_mass_battle_loading();
    if (context.engine_services != nullptr && simulation_facade_ != nullptr) {
        context.engine_services->simulation = simulation_facade_;
    }
    if (context.engine_services != nullptr && context.engine_services->simulation != nullptr) {
        context.requestPresentationSnapshot(
            context.engine_services->simulation->snapshotView());
    }
#endif
    if (scenario_) {
        const auto generated = scenario_->poll();
        if (!generated) {
            generation_error_ = std::string(generated.error().message);
        } else if (!plan_ && generated.value()) {
            if (const gameplay::WorldScenarioArtifact* artifact = scenario_->activeArtifact();
                artifact != nullptr) {
                finalize_plan(artifact->plan);
            }
        }
    } else if (!plan_ && region_streamer_) {
        region_streamer_->poll();
        auto ready_regions = region_streamer_->take_ready();
        if (!ready_regions.empty()) {
            finalize_plan(std::move(ready_regions.front().plan));
        }
    }

    context.ui.clear();
    auto& model = context.ui.model();
    (void)model.set("title", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"INFANTRY MASS BATTLE"} : std::string{"BATTLEFIELD"});
    (void)model.set("description", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"WASD/arrows move | RMB rotate | MMB pan | wheel zoom | R frame battle"}
        : std::string{"Procedural world plan"});
    (void)model.set("error", std::string{});
    (void)model.set("seed", static_cast<std::int64_t>(config_.seed));
    (void)model.set("map_size", static_cast<std::int64_t>(config_.map_size_m));
    (void)model.set("features", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"2000 infantry units in two deterministic formations"}
        : std::string{"World features pending"});
    (void)model.set("status", std::string{"Preparing world presentation..."});
    const std::string profile = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? massBattleProfileName(mass_battle_profile_) : "n/a";
    (void)model.set("profile", profile);
    (void)model.set("live_animation_budget", static_cast<std::int64_t>(liveAnimationBudget()));
    (void)model.set("live_animation_limit", static_cast<std::int64_t>(massBattleUnitCount()));
    (void)model.set("live_animation_note",
        mass_battle_profile_ == MassBattlePresentationProfile::Quality
            ? "Quality animates all units; budget applies to Balanced and Stress."
            : "Higher budgets animate more units smoothly and increase frame cost.");
    (void)model.set("animation_mode", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? massBattleAnimationMode(mass_battle_profile_) : "n/a");
    (void)model.set("orientation", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? "model +Z | blue +X | red -X" : "");
    const double fps = std::isfinite(dt) && dt > 0.0 ? 1.0 / dt : 0.0;
    (void)model.set("diagnostics_panel_open", mass_battle_diagnostics_open_);
    (void)model.set("diagnostics_panel_closed", !mass_battle_diagnostics_open_);
    (void)model.set("diag_tab_overview",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Overview);
    (void)model.set("diag_tab_terrain",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Terrain);
    (void)model.set("diag_tab_hydrology",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Hydrology);
    (void)model.set("diag_tab_performance",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Performance);
    (void)model.set("diag_tab_units",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Units);
    (void)model.set("terrain_preset", std::string{terrainPresetName(config_.terrain.preset)});
    (void)model.set("terrain_detail", std::to_string(config_.terrain.sample_spacing_m) + " m");
    (void)model.set("hydrology_mode", std::string{hydrologyModeName(config_.hydrology_mode)});
    (void)model.set("camera_frame",
                    std::string{mass_battle_frame_terrain_ ? "Terrain" : "Battle"});
    (void)model.set("diag_scene_epoch", std::to_string(scene_epoch_));
    (void)model.set("diag_fps", static_cast<std::int64_t>(fps + 0.5));
    (void)model.set("diag_draw_calls",
                    static_cast<std::int64_t>(context.render_telemetry.draw_calls));
    (void)model.set("diag_mesh_uploads",
                    static_cast<std::int64_t>(context.render_telemetry.mesh_uploads));
    (void)model.set("diag_palette_updates",
                    static_cast<std::int64_t>(context.render_telemetry.palette_updates));
    const api::EngineTelemetry empty_telemetry{};
    const api::EngineTelemetry& telemetry =
        context.engine_services != nullptr && context.engine_services->telemetry != nullptr
            ? *context.engine_services->telemetry
            : empty_telemetry;
    (void)model.set("diag_jobs_queued",
                    static_cast<std::int64_t>(telemetry.scheduler.queued));
    (void)model.set("diag_jobs_running",
                    static_cast<std::int64_t>(telemetry.scheduler.running));
    (void)model.set("diag_jobs_completed",
                    static_cast<std::int64_t>(telemetry.scheduler.completed));
    (void)model.set("diag_jobs_canceled",
                    static_cast<std::int64_t>(telemetry.scheduler.canceled));
    (void)model.set("diag_proc_requested", static_cast<std::int64_t>(0));
    (void)model.set("diag_proc_running", static_cast<std::int64_t>(0));
    (void)model.set("diag_proc_completed", static_cast<std::int64_t>(0));
    (void)model.set("diag_proc_canceled", static_cast<std::int64_t>(0));
    (void)model.set("diag_proc_cache_hits", static_cast<std::int64_t>(0));
    (void)model.set("diag_proc_cache_misses", static_cast<std::int64_t>(0));
    (void)model.set("diag_sim_ms", durationText(telemetry.simulation_duration));
    (void)model.set("diag_presentation_ms", durationText(telemetry.presentation_duration));
    (void)model.set("diag_animation_ms", std::string{"0.000"});
    (void)model.set("diag_extraction_ms", std::string{"0.000"});
    (void)model.set("diag_gpu_ms", durationText(telemetry.gpu_duration));
    (void)model.set("diag_stale_snapshots",
                    static_cast<std::int64_t>(telemetry.rejected_stale_snapshots));
    (void)model.set("diag_semantic_hash", std::string{"pending"});
    (void)model.set("diag_world_hash", std::string{"pending"});
    (void)model.set("diag_world_revision", std::string{"pending"});
    (void)model.set("diag_terrain_samples", std::string{"pending"});
    (void)model.set("diag_terrain_mesh", std::string{"pending"});
    (void)model.set("diag_elevation", std::string{"pending"});
    (void)model.set("diag_cell_size", std::string{"pending"});
    (void)model.set("diag_rivers", static_cast<std::int64_t>(0));
    (void)model.set("diag_river_width", std::string{"0.0"});
    (void)model.set("diag_river_depth", std::string{"0.0"});
    (void)model.set("diag_wetness", std::string{"0.0"});
    (void)model.set("diag_crossings", static_cast<std::int64_t>(0));
    (void)model.set("diag_water_cells", static_cast<std::int64_t>(0));
    (void)model.set("diag_flood_cells", static_cast<std::int64_t>(0));
    (void)model.set("diag_tick", static_cast<std::int64_t>(0));
    (void)model.set("diag_units", static_cast<std::int64_t>(massBattleUnitCount()));
    (void)model.set("diag_blue_units", static_cast<std::int64_t>(0));
    (void)model.set("diag_red_units", static_cast<std::int64_t>(0));
    (void)model.set("diag_average_speed", std::string{"0.0"});
    (void)model.set("diag_direction_changes", static_cast<std::int64_t>(0));
    (void)model.set("diag_published_units",
                    static_cast<std::int64_t>(mass_battle_visible_units_));
    (void)model.set("diag_atlas_slots",
                    static_cast<std::int64_t>(mass_battle_active_pose_slots_));
    (void)model.set("diag_animation_summary",
        "idle " + std::to_string(mass_battle_archetype_counts_[0U]) +
        " | walk " + std::to_string(mass_battle_archetype_counts_[1U]) +
        " | run " + std::to_string(mass_battle_archetype_counts_[2U]) +
        " | crouch " + std::to_string(mass_battle_archetype_counts_[3U]) +
        " | prone " + std::to_string(mass_battle_archetype_counts_[5U]));
    std::string diagnostics = "profile " + profile + " | FPS " +
        std::to_string(static_cast<int>(fps + 0.5)) +
        " | draw calls " + std::to_string(context.render_telemetry.draw_calls) +
        " | mesh uploads " + std::to_string(context.render_telemetry.mesh_uploads) +
        " | palette updates " + std::to_string(context.render_telemetry.palette_updates) +
        " | CPU sim running " + std::to_string(
            telemetry.scheduler.running_by_class[
                static_cast<std::size_t>(jobs::WorkClass::Simulation)]) +
        " | CPU presentation running " + std::to_string(
            telemetry.scheduler.running_by_class[
                static_cast<std::size_t>(jobs::WorkClass::Presentation)]) +
        " | GPU draw calls " + std::to_string(context.render_telemetry.draw_calls);
    if (context.engine_services != nullptr && context.engine_services->telemetry != nullptr) {
        const auto& telemetry = *context.engine_services->telemetry;
        (void)model.set("diag_sim_ms",
                        durationText(telemetry.simulation_duration));
        (void)model.set("diag_presentation_ms",
                        durationText(telemetry.presentation_duration));
        (void)model.set("diag_animation_ms",
                        durationText(telemetry.animation_duration));
        (void)model.set("diag_extraction_ms",
                        durationText(telemetry.extraction_duration));
        (void)model.set("diag_gpu_ms",
                        durationText(telemetry.gpu_duration));
        (void)model.set("diag_stale_snapshots",
                        static_cast<std::int64_t>(telemetry.rejected_stale_snapshots));
        (void)model.set("diag_semantic_hash", std::to_string(telemetry.semantic_hash));
        diagnostics += " | sim ms " + std::to_string(
                           telemetry.simulation_duration.count() / 1'000'000.0) +
                       " | presentation ms " + std::to_string(
                           telemetry.presentation_duration.count() / 1'000'000.0) +
                       " | GPU ms " + std::to_string(
                           telemetry.gpu_duration.count() / 1'000'000.0) +
                       " | stale snapshots " + std::to_string(
                           telemetry.rejected_stale_snapshots);
    }
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        diagnostics += " | published " + std::to_string(mass_battle_visible_units_) +
                       " | atlas slots " + std::to_string(mass_battle_active_pose_slots_) +
                       " | idle " + std::to_string(mass_battle_archetype_counts_[0U]) +
                       " walk " + std::to_string(mass_battle_archetype_counts_[1U]) +
                       " run " + std::to_string(mass_battle_archetype_counts_[2U]) +
                       " crouch " + std::to_string(mass_battle_archetype_counts_[3U]) +
                       " crouch-walk " + std::to_string(mass_battle_archetype_counts_[4U]) +
                       " prone " + std::to_string(mass_battle_archetype_counts_[5U]) +
                       " prone-move " + std::to_string(mass_battle_archetype_counts_[6U]) +
                       " weapon-ready " + std::to_string(mass_battle_archetype_counts_[7U]);
        if (mass_battle_profile_ == MassBattlePresentationProfile::Quality) {
            diagnostics += " | poses evaluated " +
                           std::to_string(mass_battle_evaluated_poses_) +
                           " | LOD near " + std::to_string(mass_battle_lod_counts_[0U]) +
                           " mid " + std::to_string(mass_battle_lod_counts_[1U]) +
                           " far " + std::to_string(mass_battle_lod_counts_[2U]);
        } else {
            diagnostics += std::string{" | atlas | phases "} +
                           (mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? "full" : "adaptive") +
                           " | shadows " +
                           (mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? "full" : "180m");
        }
    }
    (void)model.set("diagnostics",std::move(diagnostics));
    if (plan_) {
        if (scenario_ != nullptr) {
            const auto procedural = scenario_->proceduralTelemetry();
            const std::uint64_t canceled = procedural.canceled + procedural.superseded;
            (void)model.set("diag_proc_requested",
                            static_cast<std::int64_t>(procedural.requested));
            (void)model.set("diag_proc_running",
                            static_cast<std::int64_t>(procedural.running));
            (void)model.set("diag_proc_completed",
                            static_cast<std::int64_t>(procedural.completed));
            (void)model.set("diag_proc_canceled",
                            static_cast<std::int64_t>(canceled));
            (void)model.set("diag_proc_cache_hits",
                            static_cast<std::int64_t>(procedural.cache_hits));
            (void)model.set("diag_proc_cache_misses",
                            static_cast<std::int64_t>(procedural.cache_misses));
        }
        (void)model.set("seed", static_cast<std::int64_t>(plan_->seed));
        (void)model.set("map_size", static_cast<std::int64_t>(plan_->map_size_m));
        (void)model.set("features", feature_summary(*plan_));
#if GENOMES_HAS_INFANTRY
        const std::size_t infantry_count = mass_battle_session_ != nullptr
                                                ? mass_battle_session_->presentationSnapshot().states.size()
                                                : battlefield_runtime_ != nullptr
                                                    ? battlefield_runtime_->presentationSnapshot().states.size()
                                                    : 0U;
#else
        constexpr std::size_t infantry_count = 0U;
#endif
        (void)model.set("units", static_cast<std::int64_t>(infantry_count));
#if GENOMES_HAS_INFANTRY
        if (const auto mass = mass_battle_session_ != nullptr
                                  ? mass_battle_session_->massBattleSnapshot()
                                  : std::optional<gameplay::InfantryMassBattleSnapshot>{}; mass) {
            (void)model.set("diag_tick", static_cast<std::int64_t>(mass->tick));
            (void)model.set("diag_units", static_cast<std::int64_t>(mass->total_units));
            (void)model.set("diag_blue_units", static_cast<std::int64_t>(mass->blue_units));
            (void)model.set("diag_red_units", static_cast<std::int64_t>(mass->red_units));
            (void)model.set("diag_average_speed", std::to_string(mass->average_speed_mps));
            (void)model.set("diag_direction_changes",
                            static_cast<std::int64_t>(mass->direction_changes));
            (void)model.set("viability", "Mass battle: tick " + std::to_string(mass->tick) +
                                " units " + std::to_string(mass->total_units) +
                                " avg speed " + std::to_string(mass->average_speed_mps) +
                                " m/s turns " + std::to_string(mass->direction_changes));
        } else if (battlefield_runtime_ != nullptr) {
            const auto& viability = battlefield_runtime_->snapshot();
            (void)model.set("viability", "Combat slice: tick " + std::to_string(viability.tick) +
                                " fire " + std::to_string(viability.fired) +
                                " impact " + std::to_string(viability.impacts) +
                                " deaths " + std::to_string(viability.deaths));
        }
#endif
        (void)model.set("terrain", "Terrain: " + std::to_string(terrain_->width()) + " x " +
                            std::to_string(terrain_->height()) + " samples");
        (void)model.set("mesh", "Mesh: " + std::to_string(terrain_mesh_->vertices.size()) +
                            " vertices / " + std::to_string(terrain_mesh_->triangle_count()) + " triangles");
        (void)model.set("elevation", "Elevation: " + std::to_string(terrain_min_height_) + " .. " +
                            std::to_string(terrain_max_height_) + " m");
        (void)model.set("hash", static_cast<std::int64_t>(plan_->content_hash));
        (void)model.set("diag_world_hash", std::to_string(plan_->content_hash));
        (void)model.set("diag_world_revision",
                        std::to_string(world::artifactRevision(*plan_)));
        (void)model.set("diag_terrain_samples",
                        std::to_string(terrain_->width()) + " x " +
                        std::to_string(terrain_->height()));
        (void)model.set("diag_terrain_mesh",
                        std::to_string(terrain_mesh_->vertices.size()) + " vertices | " +
                        std::to_string(terrain_mesh_->triangle_count()) + " triangles");
        (void)model.set("diag_elevation", std::to_string(terrain_min_height_) + " .. " +
                                               std::to_string(terrain_max_height_) + " m");
        (void)model.set("diag_cell_size", std::to_string(terrain_->cellSize()) + " m");
        (void)model.set("diag_rivers",
                        static_cast<std::int64_t>(plan_->hydrology.rivers.size()));
        float total_width = 0.0F;
        float total_depth = 0.0F;
        for (const auto& river : plan_->hydrology.rivers) {
            total_width += river.width_m;
            total_depth += river.depth_m;
        }
        const float river_count = static_cast<float>(plan_->hydrology.rivers.size());
        (void)model.set("diag_river_width",
                        std::to_string(river_count > 0.0F ? total_width / river_count : 0.0F));
        (void)model.set("diag_river_depth",
                        std::to_string(river_count > 0.0F ? total_depth / river_count : 0.0F));
        (void)model.set("diag_wetness", std::to_string(
            terrain_ != nullptr && terrain_->width() > 0U && terrain_->height() > 0U
                ? static_cast<float>(hydrology_flood_cells_) /
                      static_cast<float>(terrain_->width() * terrain_->height())
                : 0.0F));
        (void)model.set("diag_crossings",
                        static_cast<std::int64_t>(plan_->hydrology.crossings.size()));
        (void)model.set("diag_water_cells",
                        static_cast<std::int64_t>(hydrology_water_cells_));
        (void)model.set("diag_flood_cells",
                        static_cast<std::int64_t>(hydrology_flood_cells_));
        (void)model.set("status",
            mode_ == BattlefieldSceneMode::InfantryMassBattle &&
                    mass_battle_profile_ != MassBattlePresentationProfile::Quality &&
                    !mass_battle_pose_atlas_ready_
                ? std::string{"Baking animation atlas..."}
                : std::string{"World plan ready for terrain, navigation and rendering."});
    } else if (scenario_ && scenario_->status().generation_pending) {
        (void)model.set("status", std::string{"Generating world on worker threads..."});
    } else if (scenario_ && scenario_->status().streaming_pending > 0U) {
        (void)model.set("status", std::string{"Streaming adjacent world region..."});
    } else if (region_streamer_ && region_streamer_->pending_count() > 0) {
        (void)model.set("status", std::string{"Generating world on worker threads..."});
    } else if (region_streamer_ && region_streamer_->failed()) {
        (void)model.set("error", "World generation failed: " +
                            std::string(region_streamer_->error().message));
    } else {
        (void)model.set("error", "World generation failed: " + generation_error_);
    }
#if GENOMES_HAS_INFANTRY
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle &&
        mass_battle_session_ != nullptr && infantry_model_artifact_ != nullptr &&
        mass_battle_profile_ != MassBattlePresentationProfile::Quality &&
        !mass_battle_pose_atlas_ready_) {
        (void)model.set("status", mass_battle_pose_atlas_failed_
            ? std::string{"Animation atlas unavailable; using fallback rendering."}
            : std::string{"Baking animation atlas..."});
    }
#endif
}

} // namespace genomes::game_scenes
