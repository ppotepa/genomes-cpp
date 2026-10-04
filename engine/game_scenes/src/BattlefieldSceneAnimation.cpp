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
void BattlefieldScene::initialize_infantry_animation() {
    if (animation_job_.valid()) {
        if (!animation_job_.isComplete()) {
            animation_job_.cancel();
            return;
        }
        animation_job_ = {};
    }
    if (animation_system_ && infantry_model_artifact_) {
        const std::size_t expected_count = battlefield_runtime_ != nullptr
            ? battlefield_runtime_->presentationSnapshot().states.size()
            : mass_battle_session_ != nullptr ? mass_battle_session_->presentationSnapshot().states.size() : 0U;
        if (animation_agents_.size() == expected_count) return;
    }
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
    pose_stall_frames_ = 0U;
    last_pose_tick_ = 0U;
    last_pose_revision_ = 0U;
    pending_animation_context_.reset();
    pending_animation_error_.reset();
    if ((!battlefield_runtime_ && !mass_battle_session_) || !infantry_model_artifact_) {
        return;
    }
    auto animation = infantry::PresentationAnimation::create(64U);
    if (!animation) {
        generation_error_ = std::string(animation.error().message);
        return;
    }
        animation_system_ = std::move(animation.value());
        if (engine_services_ != nullptr && engine_services_->execution != nullptr) {
            animation_system_->bindScheduler(*engine_services_->execution);
        }
    const std::size_t expected_count = battlefield_runtime_ != nullptr
        ? battlefield_runtime_->presentationSnapshot().states.size()
        : mass_battle_session_->presentationSnapshot().states.size();
    animation_agents_.reserve(expected_count);
    const auto add_agent = [this](simulation::EntityId entity,
                                  std::uint8_t animation_variant,
                                  float animation_phase) {
        auto locomotion = infantry::LocomotionController::create(
            infantry_model_artifact_->phenotype.body);
        if (!locomotion) {
            generation_error_ = std::string(locomotion.error().message);
            return;
        }
        InfantryAnimationAgent agent{};
        agent.entity = entity;
        agent.locomotion = std::move(locomotion.value());
        agent.locomotion_state = agent.locomotion->initialState();
        if (!agent.locomotion->setState(*agent.locomotion_state,
                                        massBattleAnimationState(animation_variant), true)) {
            generation_error_ = "mass infantry animation preset rejected";
            return;
        }
        agent.locomotion_state->phase = std::clamp(
            static_cast<double>(animation_phase), 0.0, 0.99);
        agent.lod.setTier(infantry::AnimationLOD::Near);
        animation_agents_.push_back(std::move(agent));
    };
    if (mass_battle_session_ != nullptr) {
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            add_agent(state.entity, state.animation_variant, state.animation_phase);
        }
    } else {
        for (const infantry::InfantryRenderState& state : battlefield_runtime_->presentationSnapshot().states) {
            auto locomotion = infantry::LocomotionController::create(
                infantry_model_artifact_->phenotype.body);
            auto face = infantry::FaceAnimator::create(
                proc::Seed(foundation::stableHashCombine(
                    static_cast<std::uint64_t>(config_.seed), state.entity.packed())),
                infantry_model_artifact_->phenotype.face);
            if (!locomotion || !face) {
                generation_error_ = !locomotion ? std::string(locomotion.error().message)
                                                : std::string(face.error().message);
                continue;
            }
            InfantryAnimationAgent agent{};
            agent.entity = state.entity;
            agent.locomotion = std::move(locomotion.value());
            agent.locomotion_state = agent.locomotion->initialState();
            agent.face = std::move(face.value());
            agent.lod.setTier(infantry::AnimationLOD::Near);
            animation_agents_.push_back(std::move(agent));
        }
    }
}

void BattlefieldScene::evaluate_infantry_animation(const simulation::TickContext& context) {
    if (!animation_system_ || (!battlefield_runtime_ && !mass_battle_session_) ||
        !infantry_model_artifact_ ||
        animation_agents_.empty()) {
        return;
    }
    if (animation_job_.valid()) {
        // Keep only the newest simulation input while one pose evaluation is
        // active. Agent state is deliberately not mutated until that job has
        // completed, so the worker owns a stable input view.
        pending_animation_context_ = context;
        return;
    }

    const float map_size = std::max(1.0F, static_cast<float>(config_.map_size_m));
    const float near_distance = std::max(60.0F, map_size * 0.16F);
    const float mid_distance = std::max(near_distance * 2.0F, map_size * 0.42F);
    const float near2 = near_distance * near_distance;
    const float mid2 = mid_distance * mid_distance;

    std::vector<bool> live_selected(animation_agents_.size(), true);
    if (mass_battle_session_ != nullptr && mass_battle_pose_atlas_ready_ &&
        mass_battle_profile_ != MassBattlePresentationProfile::Quality) {
        struct Candidate final {
            std::size_t agent_index{0U};
            float distance_squared{0.0F};
            bool transition_pending{false};
            bool previously_live{false};
            std::uint64_t entity_key{0U};
        };
        std::unordered_map<std::uint64_t,
                           const gameplay::BattlefieldUnitPresentation*> states;
        states.reserve(mass_battle_session_->presentationSnapshot().states.size());
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        const auto was_live = [this](simulation::EntityId entity) {
            return std::find(live_animation_entities_.begin(),
                             live_animation_entities_.end(), entity) !=
                   live_animation_entities_.end();
        };
        std::vector<Candidate> candidates;
        candidates.reserve(animation_agents_.size());
        for (std::size_t index = 0U; index < animation_agents_.size(); ++index) {
            const auto& agent = animation_agents_[index];
            const auto found = states.find(agent.entity.packed());
            if (found == states.end() || !agent.locomotion_state) continue;
            const auto& state = *found->second;
            const float dx = state.position.x - camera_request_.position.x;
            const float dy = state.position.y - camera_request_.position.y;
            const float dz = state.position.z - camera_request_.position.z;
            const bool transition_pending = agent.locomotion_state->transition_active ||
                agent.locomotion_state->requested_state !=
                    massBattleAnimationState(state.animation_variant);
            candidates.push_back({index, dx * dx + dy * dy + dz * dz,
                                  transition_pending, was_live(agent.entity),
                                  agent.entity.packed()});
        }
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
                if (lhs.transition_pending != rhs.transition_pending)
                    return lhs.transition_pending;
                const float lhs_score = lhs.distance_squared *
                    (lhs.previously_live ? 0.90F : 1.0F);
                const float rhs_score = rhs.distance_squared *
                    (rhs.previously_live ? 0.90F : 1.0F);
                if (lhs_score != rhs_score) return lhs_score < rhs_score;
                return lhs.entity_key < rhs.entity_key;
            });
        std::fill(live_selected.begin(), live_selected.end(), false);
        live_animation_entities_.clear();
        const std::size_t selected_count =
            std::min(liveAnimationBudget(), candidates.size());
        live_animation_entities_.reserve(selected_count);
        for (std::size_t index = 0U; index < selected_count; ++index) {
            const Candidate& candidate = candidates[index];
            live_selected[candidate.agent_index] = true;
            live_animation_entities_.push_back(
                animation_agents_[candidate.agent_index].entity);
        }
    } else if (mass_battle_session_ != nullptr) {
        live_animation_entities_.clear();
        live_animation_entities_.reserve(animation_agents_.size());
        for (const auto& agent : animation_agents_)
            live_animation_entities_.push_back(agent.entity);
    }

    std::vector<infantry::AnimationEntity> entities;
    entities.reserve(animation_agents_.size());
    const auto process = [&](InfantryAnimationAgent& agent,
                             foundation::Vec3 position,
                             float heading,
                             infantry::AgentState state,
                             std::uint8_t animation_variant,
                             float animation_phase,
                             foundation::StableId action,
                             bool mass_battle) {
        if (!agent.locomotion || !agent.locomotion_state ||
            (!mass_battle && !agent.face)) {
            return;
        }
        const float dx = position.x - camera_request_.position.x;
        const float dy = position.y - camera_request_.position.y;
        const float dz = position.z - camera_request_.position.z;
        const float distance2 = dx * dx + dy * dy + dz * dz;
        infantry::AnimationLOD desired_lod =
            mass_battle && (mass_battle_profile_ == MassBattlePresentationProfile::Quality ||
                            liveAnimationBudget() >= animation_agents_.size() ||
                            !mass_battle_pose_atlas_ready_)
                ? infantry::AnimationLOD::Near
                : distance2 <= near2 ? infantry::AnimationLOD::Near
                : distance2 <= mid2 ? infantry::AnimationLOD::Mid
                                    : infantry::AnimationLOD::Far;
        if (mass_battle &&
            (agent.locomotion_state->transition_active ||
             agent.locomotion_state->requested_state !=
                 massBattleAnimationState(animation_variant))) {
            desired_lod = infantry::AnimationLOD::Near;
        }
        if (agent.lod.tier() != desired_lod) agent.lod.setTier(desired_lod);

        float distance = 0.0F;
        float speed = 0.0F;
        float move_angle = 0.0F;
        float turn_rate = 0.0F;
        if (agent.has_previous_motion) {
            const float dx = position.x - agent.previous_position.x;
            const float dz = position.z - agent.previous_position.z;
            distance = std::sqrt(dx * dx + dz * dz);
            speed = distance / std::max(1.0e-5F,
                                        static_cast<float>(context.fixed_dt_seconds));
            if (distance > 1.0e-5F) {
                const float travel_heading = std::atan2(dx, dz);
                move_angle = std::atan2(std::sin(travel_heading - heading),
                                        std::cos(travel_heading - heading));
            }
            const float heading_delta = std::atan2(
                std::sin(heading - agent.previous_heading),
                std::cos(heading - agent.previous_heading));
            turn_rate = heading_delta / std::max(1.0e-5F,
                                                static_cast<float>(context.fixed_dt_seconds));
        }
        if (mass_battle) {
            // The simulation changes posture at each local goal epoch.
            const auto requested = massBattleAnimationState(animation_variant);
            if (agent.locomotion_state->requested_state != requested) {
                (void)agent.locomotion->setState(
                    *agent.locomotion_state, requested, false);
            }
        } else {
            const auto requested_action = infantryActionAnimationState(action);
            if (requested_action.has_value()) {
                if (*requested_action == infantry::AnimationState::IDLE) {
                    (void)agent.locomotion->setRequested(
                        *agent.locomotion_state, {{0.0F}, {0.0F}, std::nullopt});
                } else {
                    (void)agent.locomotion->setState(
                        *agent.locomotion_state, *requested_action);
                }
            } else {
                switch (state) {
                case infantry::AgentState::Advance:
                    if (!agent.has_previous_motion) speed = agent.locomotion->body().run_speed;
                    (void)agent.locomotion->setRequested(
                        *agent.locomotion_state, {{0.0F}, {speed}, std::nullopt});
                    break;
                case infantry::AgentState::Engage:
                case infantry::AgentState::Dead:
                    (void)agent.locomotion->setState(*agent.locomotion_state,
                                                      infantry::AnimationState::CROUCH);
                    break;
                case infantry::AgentState::Idle:
                    (void)agent.locomotion->setRequested(
                        *agent.locomotion_state, {{0.0F}, {0.0F}, std::nullopt});
                    break;
                }
            }
        }
        infantry::LocomotionMotionContext motion{};
        motion.speed_mps = speed;
        motion.move_angle = move_angle;
        motion.turn_rate = turn_rate;
        motion.turning = std::abs(turn_rate) > 0.32F;
        motion.distance_m = distance;
        (void)agent.locomotion->step(*agent.locomotion_state, motion,
                                      context.fixed_dt_seconds);
        if (mass_battle) {
            agent.locomotion_state->phase = std::clamp(
                static_cast<double>(animation_phase), 0.0, 0.999999);
        }
        agent.previous_position = position;
        agent.previous_heading = heading;
        agent.has_previous_motion = true;

        if (!mass_battle && battlefield_runtime_ != nullptr) {
            if (const auto* weapon_tasks = battlefield_runtime_->weaponPoseTasks(agent.entity);
                weapon_tasks != nullptr && weapon_tasks->valid()) {
                agent.weapon_overlay = infantry_presentation::copyWeaponPoseTasks(
                    *weapon_tasks, position);
            } else {
                agent.weapon_overlay.reset();
            }
        } else if (mass_battle) {
            const bool wants_weapon_ready =
                massBattleAnimationArchetype(animation_variant) ==
                MassBattleAnimationArchetype::WeaponReady;
            const float readiness_delta =
                static_cast<float>(context.fixed_dt_seconds) * 2.5F;
            agent.weapon_readiness = std::clamp(
                agent.weapon_readiness +
                    (wants_weapon_ready ? readiness_delta : -readiness_delta),
                0.0F, 1.0F);
            if (agent.weapon_readiness > 1.0e-3F) {
            const float height = infantry_model_artifact_->phenotype.body.height;
            infantry::AnimationWeaponOverlay overlay{};
            overlay.weapon_id = agent.entity.packed();
            overlay.readiness = agent.weapon_readiness;
            overlay.primary = {infantry::AnimationHandOwner::Primary,
                               {0.18F * height, 0.70F * height, 0.38F * height},
                               agent.weapon_readiness, 0.75F, true};
            overlay.support = {infantry::AnimationHandOwner::Support,
                               {-0.12F * height, 0.69F * height, 0.49F * height},
                               agent.weapon_readiness, 0.7F, true};
            agent.weapon_overlay = overlay;
            } else {
                agent.weapon_overlay.reset();
            }
        } else {
            agent.weapon_overlay.reset();
        }

        infantry::AnimationEntity entity{};
        entity.semantic_id = foundation::stable_id("battlefield.infantry") ^ agent.entity.packed();
        entity.skeleton = &infantry_model_artifact_->skeleton;
        entity.gear = &infantry_model_artifact_->gear;
        entity.locomotion = &*agent.locomotion;
        entity.locomotion_state = &*agent.locomotion_state;
        entity.transition_runtime = &agent.transition_runtime;
        entity.face = agent.face ? &*agent.face : nullptr;
        entity.root_position = position;
        entity.look_target = foundation::Vec3{position.x + std::sin(heading) * 6.0F,
                                              position.y +
                                                  infantry_model_artifact_->phenotype.body.height * 0.62F,
                                              position.z + std::cos(heading) * 6.0F};
        entity.lod = agent.lod;
        entity.surface = &infantry_model_artifact_->appearance.body;
        entity.weapon_overlay = agent.weapon_overlay.has_value() ? &*agent.weapon_overlay : nullptr;
        if (!mass_battle) {
            // GroundSurfaceQuery retains its legacy void* callback ABI; the
            // adapter never mutates the const height field through that pointer.
            entity.ground_surface = {context.tick.value,
                                     const_cast<gameplay::WorldScenarioArtifact*>(
                                         world_artifacts_.get()),
                                     sample_battlefield_ground};
            entity.ground_runtime = &agent.ground_runtime;
        }
        entities.push_back(std::move(entity));
    };
    if (mass_battle_session_ != nullptr) {
        std::unordered_map<std::uint64_t,
                           const gameplay::BattlefieldUnitPresentation*> states;
        states.reserve(mass_battle_session_->presentationSnapshot().states.size());
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        for (std::size_t index = 0U; index < animation_agents_.size(); ++index) {
            if (!live_selected[index]) continue;
            InfantryAnimationAgent& agent = animation_agents_[index];
            const auto found = states.find(agent.entity.packed());
            if (found != states.end()) {
                const auto* state = found->second;
                process(agent, state->position, state->heading,
                        state->state, state->animation_variant,
                        state->animation_phase, 0U, true);
            }
        }
    } else {
        std::unordered_map<std::uint64_t, const infantry::InfantryRenderState*> states;
        states.reserve(battlefield_runtime_->presentationSnapshot().states.size());
        for (const auto& state : battlefield_runtime_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        for (InfantryAnimationAgent& agent : animation_agents_) {
            const auto state_found = states.find(agent.entity.packed());
            if (state_found != states.end()) {
                const auto* state = state_found->second;
                process(agent, state->position, state->heading,
                        state->state, 0U, 0.0F, state->action, false);
            }
        }
    }
    if (entities.empty()) {
        // A zero budget returns the whole crowd to atlas poses. Do not keep
        // rendering the previous live selection indefinitely.
        if (mass_battle_session_ != nullptr) {
            animation_poses_.clear();
            mass_battle_evaluated_poses_ = 0U;
            mass_battle_lod_counts_.fill(0U);
        }
        return;
    }
    if (engine_services_ != nullptr && engine_services_->execution != nullptr) {
        infantry::AnimationWorkSet work{std::move(entities), context.tick.value,
                                        static_cast<float>(context.fixed_dt_seconds)};
        animation_job_ = animation_system_->evaluateAsync(
            std::move(work), cancellation_, infantry::PresentationBudget{liveAnimationBudget()});
        return;
    }

    const auto result = animation_system_->evaluate(
        std::span<infantry::AnimationEntity>(entities), context.tick.value,
        context.fixed_dt_seconds, nullptr);
    if (!result) {
        generation_error_ = std::string(result.error().message);
        return;
    }
    publish_infantry_animation_result();
}

void BattlefieldScene::publish_infantry_animation_result() {
    if (!animation_system_) return;
    animation_poses_ = animation_system_->currentSnapshot().poses;
    if (infantry_model_artifact_) {
        if (auto write = pose_exchange_.acquireWrite(); write) {
            auto& snapshot = write.value().snapshot();
            snapshot.metadata.tick = animation_system_->currentSnapshot().simulation_tick;
            snapshot.metadata.scene_epoch = scene_epoch_;
            snapshot.metadata.revision = animation_system_->currentSnapshot().pose_revision;
            snapshot.palettes.reserve(animation_poses_.size());
            for (const auto& pose : animation_poses_) {
                render::SkinnedBonePalette palette{};
                if (mass_battle_session_ != nullptr) {
                    const foundation::StableId packed_entity = pose.semantic_id ^
                        foundation::stable_id("battlefield.infantry");
                    palette.instance_id = foundation::stable_id("entity.infantry") ^ packed_entity;
                } else {
                    palette.instance_id = pose.semantic_id;
                }
                palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                palette.pose_revision = pose.revision;
                const auto pose_span = std::span<const infantry::RigTransform>(pose.bones);
                palette.matrices = infantry_presentation::makePalette(
                    infantry_model_artifact_->skeleton, pose_span);
                if (mass_battle_session_ == nullptr) {
                    palette.local_poses = infantry_presentation::makeLocalPoses(
                        infantry_model_artifact_->skeleton, pose_span);
                }
                palette.morph_weights = {pose.face.eyelids_close, pose.face.eyelids_arc,
                                         pose.face.neck_flex, pose.face.hands_relax};
                snapshot.palettes.push_back(std::move(palette));
            }
            (void)pose_exchange_.publish(std::move(write.value()));
        }
    }
    if (mass_battle_session_ != nullptr) {
        mass_battle_lod_counts_.fill(0U);
        for (const auto& agent : animation_agents_) {
            const auto index = static_cast<std::size_t>(agent.lod.tier());
            if (index < mass_battle_lod_counts_.size()) ++mass_battle_lod_counts_[index];
        }
        mass_battle_evaluated_poses_ = animation_system_->lastStats().evaluated_count;
    }
}

void BattlefieldScene::request_mass_battle_pose_atlas() {
    if (mass_battle_session_ == nullptr || mass_battle_pose_atlas_ready_ ||
        mass_battle_pose_atlas_failed_ ||
        mass_battle_presentation_scheduler_.atlasActive() || !infantry_skinned_prototype_ ||
        !infantry_model_artifact_ || animation_poses_.empty()) {
        return;
    }
    std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
    poses.reserve(animation_poses_.size());
    for (const auto& pose : animation_poses_) {
        poses.emplace(pose.semantic_id, &pose);
    }
    // AnimationPose contains the complete rig arrays; keeping all atlas
    // samples on the frame stack overflows the relatively small application
    // thread stack. The atlas job already owns heap state, so keep samples
    // there as well.
    auto samples = std::make_shared<
        std::array<std::optional<infantry::AnimationPose>, MassBattlePoseAtlasSize>>();
    std::array<float, MassBattlePoseAtlasSize> sample_errors{};
    sample_errors.fill(std::numeric_limits<float>::max());
    for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
        const std::size_t variant = state.animation_variant % MassBattlePoseVariantCount;
        const std::size_t phases = MassBattlePosePhaseCounts[variant];
        const std::size_t bucket = massBattlePoseBucket(variant, state.animation_phase, phases);
        const float target = (static_cast<float>(bucket - massBattlePoseOffset(variant)) + 0.5F) /
                             static_cast<float>(phases);
        const float direct = std::abs(state.animation_phase - target);
        const float error = std::min(direct, 1.0F - direct);
        const auto found = poses.find(foundation::stable_id("battlefield.infantry") ^
                                      state.entity.packed());
        if (found != poses.end() &&
            found->second->active_state == massBattleAnimationState(state.animation_variant) &&
            found->second->requested_state == found->second->active_state &&
            found->second->transition_stage == infantry::AnimationTransitionStage::None &&
            (variant != 7U || found->second->weapon_readiness > 0.9F) &&
            (variant != 0U || found->second->weapon_readiness < 0.1F) &&
            error < sample_errors[bucket]) {
            (*samples)[bucket] = *found->second;
            sample_errors[bucket] = error;
        }
    }
    for (std::size_t variant = 0U; variant < MassBattlePoseVariantCount; ++variant) {
        const std::size_t begin = massBattlePoseOffset(variant);
        const std::size_t end = begin + MassBattlePosePhaseCounts[variant];
        // Small populations may not cover every animation phase. Reuse the
        // nearest phase of the same stance; never borrow another stance.
        for (std::size_t slot = begin; slot < end; ++slot) {
            if ((*samples)[slot]) continue;
            std::size_t nearest = end;
            for (std::size_t candidate = begin; candidate < end; ++candidate) {
                if ((*samples)[candidate] &&
                    (nearest == end || std::abs(static_cast<int>(candidate) - static_cast<int>(slot)) <
                                           std::abs(static_cast<int>(nearest) - static_cast<int>(slot)))) {
                    nearest = candidate;
                }
            }
            if (nearest == end) return;
            (*samples)[slot] = (*samples)[nearest];
        }
    }

    if (engine_services_ != nullptr && engine_services_->execution != nullptr) {
        mass_battle_presentation_scheduler_.scheduleAtlas(
            infantry_skinned_prototype_, infantry_model_artifact_, samples);
    }
}
#endif


} // namespace genomes::game_scenes
