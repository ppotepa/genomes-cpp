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
void BattlefieldScene::schedule_mass_battle_presentation() {
    if (mass_battle_session_ == nullptr || !infantry_model_artifact_ ||
        mass_battle_presentation_scheduler_.active()) {
        return;
    }
    const auto states = std::make_shared<
        const std::vector<gameplay::BattlefieldUnitPresentation>>(
        mass_battle_session_->presentationSnapshot().states);
    if (states->empty()) return;
    const float model_height = std::max(0.01F, infantry_model_artifact_->phenotype.body.height);
    const foundation::StableId mesh_id = infantry_skinned_prototype_->mesh_id;
    const foundation::StableId blue_material = foundation::stable_id("material.infantry.blue");
    const foundation::StableId red_material = foundation::stable_id("material.infantry.red");
    if (engine_services_ == nullptr || engine_services_->execution == nullptr) {
        return;
    }
    mass_battle_presentation_scheduler_.schedule(
        states, terrain_, model_height, mesh_id, blue_material, red_material,
        mass_battle_session_->presentationSnapshot().metadata.tick);
}

void BattlefieldScene::consume_mass_battle_presentation() {
    const auto ready = mass_battle_presentation_scheduler_.take();
    if (ready) ready_mass_battle_presentation_ = ready;
}
#endif

void BattlefieldScene::build_presentation(SceneContext& context) {
#if GENOMES_HAS_INFANTRY
    if (mass_battle_session_ != nullptr && mass_battle_frame_terrain_ && terrain_ != nullptr) {
        const foundation::Vec3 center{0.0F, terrain_->sampleBilinear(0.0, 0.0) + 2.0F, 0.0F};
        constexpr float initial_pitch = 0.558505361F;
        const float half_extent = static_cast<float>(config_.map_size_m) * 0.5F;
        const float distance = std::clamp(
            half_extent / std::tan(camera_request_.lens.vertical_fov * 0.5F),
            rts_controls_.min_distance, rts_controls_.max_distance);
        camera_request_.target = center;
        camera_request_.position = {center.x, center.y + std::sin(initial_pitch) * distance,
                                    center.z + std::cos(initial_pitch) * distance};
    } else if (mass_battle_session_ != nullptr &&
               !mass_battle_session_->presentationSnapshot().states.empty()) {
        foundation::Vec3 minimum{std::numeric_limits<float>::max(),0.0F,
                                 std::numeric_limits<float>::max()};
        foundation::Vec3 maximum{std::numeric_limits<float>::lowest(),0.0F,
                                 std::numeric_limits<float>::lowest()};
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            minimum.x=std::min(minimum.x,state.position.x);
            minimum.z=std::min(minimum.z,state.position.z);
            maximum.x=std::max(maximum.x,state.position.x);
            maximum.z=std::max(maximum.z,state.position.z);
        }
        foundation::Vec3 center{(minimum.x+maximum.x)*0.5F,0.0F,
                                (minimum.z+maximum.z)*0.5F};
        center.y = terrain_ != nullptr ? terrain_->sampleBilinear(center.x, center.z) + 2.0F
                                       : 2.0F;
        constexpr float initial_pitch=0.558505361F;
        const float half_extent=std::max(maximum.x-minimum.x,maximum.z-minimum.z)*0.5F+10.0F;
        const float distance=std::clamp(
            half_extent / std::tan(camera_request_.lens.vertical_fov * 0.5F),
            rts_controls_.min_distance, rts_controls_.max_distance);
        camera_request_.target = center;
        camera_request_.position = {center.x,center.y+std::sin(initial_pitch)*distance,
                                    center.z+std::cos(initial_pitch)*distance};
    }
    if (mass_battle_session_ != nullptr)
        context.presentation.camera.revision = mass_battle_camera_revision_;
#endif
    context.publishCameraRequest(camera_request_);
    context.presentation.terrain_mesh = render_terrain_mesh_;
    context.presentation.water_mesh = render_water_mesh_;
    context.presentation.world_mesh = render_world_mesh_;
    if (!plan_) {
#if GENOMES_HAS_INFANTRY
        if (mass_battle_session_ == nullptr)
#endif
        {
            render_infantry_mesh_.reset();
            return;
        }
    }

#if GENOMES_HAS_INFANTRY
    if (battlefield_runtime_ != nullptr || mass_battle_session_ != nullptr) {
        if (infantry_model_artifact_) {
            if (!infantry_skinned_prototype_) {
                infantry_skinned_prototype_ = infantry_presentation::makePrototype(
                    *infantry_model_artifact_,
                    infantry_presentation::PrototypePreparation::OptimizeDrawOrder,
                    active_generation(),
                    infantry_presentation::WeaponPoseAttachment::RightHand,
                    battlefield_runtime_ != nullptr ? battlefield_runtime_->weaponArtifact()
                        : mass_battle_session_ != nullptr ? mass_battle_session_->weaponArtifact()
                                                           : nullptr);
            }
            if (infantry_skinned_prototype_) {
                const auto bind_palette = infantry_presentation::makeBindPalette(
                    infantry_model_artifact_->skeleton);
                const auto bind_local_poses = infantry_presentation::makeLocalPoses(
                    infantry_model_artifact_->skeleton, {});
                const bool mass_battle_atlas_requested =
                    mass_battle_session_ != nullptr &&
                    mass_battle_profile_ != MassBattlePresentationProfile::Quality;
                const bool mass_battle_atlas = mass_battle_atlas_requested &&
                                               mass_battle_pose_atlas_ready_;
                std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
                poses.reserve(animation_poses_.size());
                for (const auto& pose : animation_poses_)
                    poses.emplace(pose.semantic_id, &pose);
                std::optional<render::PoseSnapshotExchange::ReadLease> pose_read;
                std::unordered_map<foundation::StableId,
                                   const render::SkinnedBonePalette*> published_palettes;
                bool received_new_pose = false;
                if (mass_battle_session_ != nullptr) {
                    auto latest_pose = pose_exchange_.acquireLatestRead();
                    if (latest_pose) {
                        pose_read.emplace(std::move(latest_pose.value()));
                        const auto& snapshot = pose_read->snapshot();
                        received_new_pose = snapshot.metadata.tick > last_pose_tick_ ||
                                             snapshot.metadata.revision > last_pose_revision_;
                        if (received_new_pose) {
                            last_pose_tick_ = snapshot.metadata.tick;
                            last_pose_revision_ = snapshot.metadata.revision;
                        }
                        published_palettes.reserve(snapshot.palettes.size());
                        for (const auto& palette : snapshot.palettes) {
                            published_palettes.emplace(palette.instance_id, &palette);
                        }
                    }
                    if (received_new_pose) {
                        pose_stall_frames_ = 0U;
                    } else if (pose_stall_frames_ < 8U) {
                        ++pose_stall_frames_;
                    }
                }
                if (engine_services_ != nullptr && engine_services_->execution != nullptr &&
                    mass_battle_atlas_requested &&
                    !mass_battle_pose_atlas_ready_ &&
                    !mass_battle_pose_atlas_failed_ && !poses.empty()) {
                    request_mass_battle_pose_atlas();
                }
                if (mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                    mass_battle_atlas_requested && !mass_battle_pose_atlas_ready_ &&
                    context.render_capabilities.gpu_skinning && !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id =
                        foundation::stable_id("mesh.infantry.mass-battle.bind-fallback");
                    render_infantry_mesh_->revision = foundation::stableHashCombine(
                        infantry_skinned_prototype_->revision, render_infantry_mesh_->mesh_id);
                } else if (!mass_battle_atlas && !context.render_capabilities.gpu_skinning &&
                           !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id = infantry_skinned_prototype_->mesh_id;
                    render_infantry_mesh_->revision = infantry_skinned_prototype_->revision;
                }
                // Keep the last complete pose for a short presentation stall,
                // then fall back to the immutable atlas (handled above) or a
                // bind palette. This never blocks the frame thread and avoids
                // replaying unboundedly stale animation data.
                if (mass_battle_session_ != nullptr && pose_stall_frames_ >= 8U &&
                    !mass_battle_atlas) {
                    poses.clear();
                    published_palettes.clear();
                }
                if (mass_battle_atlas) {
                    for (const auto& mesh : mass_battle_pose_meshes_) {
                        if (mesh) context.presentation.instance_prototypes.push_back(mesh);
                    }
                    if (context.render_capabilities.gpu_skinning && !poses.empty()) {
                        context.presentation.skinned_prototypes.push_back(
                            infantry_skinned_prototype_);
                    }
                } else if (mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                           mass_battle_atlas_requested &&
                           !mass_battle_pose_atlas_ready_ &&
                           context.render_capabilities.gpu_skinning) {
                    if (render_infantry_mesh_) {
                        context.presentation.instance_prototypes.push_back(render_infantry_mesh_);
                    }
                    if (!poses.empty()) {
                        context.presentation.skinned_prototypes.push_back(
                            infantry_skinned_prototype_);
                    }
                } else if (!context.render_capabilities.gpu_skinning) {
                    context.presentation.instance_prototypes.push_back(render_infantry_mesh_);
                } else {
                    context.presentation.skinned_prototypes.push_back(
                        infantry_skinned_prototype_);
                }

                const float model_height =
                    std::max(0.01F, infantry_model_artifact_->phenotype.body.height);
                const foundation::StableId blue_material =
                    foundation::stable_id("material.infantry.blue");
                const foundation::StableId red_material =
                    foundation::stable_id("material.infantry.red");
                if (mass_battle_session_ != nullptr) {
                    mass_battle_archetype_counts_.fill(0U);
                    for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
                        const auto index = massBattleArchetypeIndex(state.animation_variant);
                        if (index < mass_battle_archetype_counts_.size()) {
                            ++mass_battle_archetype_counts_[index];
                        }
                    }
                }
                if (mass_battle_atlas) {
                    const auto& states=mass_battle_session_->presentationSnapshot().states;
                    context.presentation.instances.reserve(
                        context.presentation.instances.size()+states.size());
                    const foundation::Vec3 camera_position=context.presentation.camera.enabled
                        ? context.presentation.camera.position : camera_request_.position;
                    const foundation::Vec3 camera_target=context.presentation.camera.enabled
                        ? context.presentation.camera.target : camera_request_.target;
                    const float focal_pixels=static_cast<float>(std::max(1,context.framebuffer_height))*
                        0.5F/std::tan(camera_request_.lens.vertical_fov*0.5F);
                    std::array<bool,MassBattlePoseAtlasSize> used_pose_slots{};
                    std::size_t visible_units=0U;
                    for (const auto& state:states) {
                        foundation::Vec3 presentation_position=state.position;
                        if (terrain_!=nullptr) presentation_position.y=terrain_->sampleBilinear(
                            presentation_position.x,presentation_position.z)+0.02F;
                        const foundation::Vec3 center=presentation_position+
                            foundation::Vec3{0.0F,state.height*0.5F,0.0F};
                        const float shadow_dx=presentation_position.x-camera_target.x;
                        const float shadow_dz=presentation_position.z-camera_target.z;
                        const bool casts_shadow=mass_battle_profile_ ==
                            MassBattlePresentationProfile::Stress ||
                            shadow_dx*shadow_dx+shadow_dz*shadow_dz<=180.0F*180.0F;
                        // Preserve off-screen casters inside the existing
                        // shadow region. Stress mode intentionally submits the
                        // full population for renderer load measurements.
                        if (!massBattleUnitShouldRender(
                                context.presentation, mass_battle_profile_,
                                presentation_position, state.height, camera_target)) {
                            continue;
                        }
                        const float distance=std::max(0.01F,math::length(camera_position-center));
                        const float pixel_height=state.height*focal_pixels/distance;
                        const std::size_t variant=state.animation_variant%MassBattlePoseVariantCount;
                        const std::size_t native_phases=MassBattlePosePhaseCounts[variant];
                        const std::size_t visible_phases =
                            mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? native_phases
                                : pixel_height >= 8.0F ? native_phases
                                : pixel_height >= 3.0F ? std::min(native_phases, std::size_t{6U})
                                : pixel_height >= 1.5F ? std::min(native_phases, std::size_t{2U})
                                                        : 1U;
                        const std::size_t atlas_slot=massBattlePoseBucket(
                            variant,state.animation_phase,visible_phases);
                        const auto& mesh=mass_battle_pose_meshes_[atlas_slot];
                        const foundation::StableId object_id=
                            foundation::stable_id("entity.infantry")^state.entity.packed();
                        const auto pose_found = poses.find(
                            foundation::stable_id("battlefield.infantry")^
                            state.entity.packed());
                        const bool use_live_pose = context.render_capabilities.gpu_skinning &&
                                                   pose_found != poses.end();
                        if (!use_live_pose && !mesh) continue;
                        if (!use_live_pose) used_pose_slots[atlas_slot]=true;
                        ++visible_units;
                        std::uint32_t flags=render::RenderInstanceFlagDynamic|
                            render::RenderInstanceFlagReceiveShadow|
                            (state.team==infantry::Team::Red?render::RenderInstanceFlagTeamRed:0U);
                        if (casts_shadow) flags|=render::RenderInstanceFlagCastShadow;
                        if (use_live_pose) {
                            const auto published = published_palettes.find(object_id);
                            if (published != published_palettes.end()) {
                                context.presentation.skinned_palettes.push_back(
                                    *published->second);
                            } else {
                                render::SkinnedBonePalette palette{};
                                palette.instance_id = object_id;
                                palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                                palette.pose_revision = pose_found->second->revision;
                                palette.matrices = infantry_presentation::makePalette(
                                    infantry_model_artifact_->skeleton,
                                    std::span<const infantry::RigTransform>(
                                        pose_found->second->bones));
                                context.presentation.skinned_palettes.push_back(
                                    std::move(palette));
                            }
                        }
                        context.presentation.instances.push_back(
                            {object_id,use_live_pose ? infantry_skinned_prototype_->mesh_id
                                                     : mesh->mesh_id,
                             state.team==infantry::Team::Blue?blue_material:red_material,
                             presentation_position,
                             {state.height/model_height,state.height/model_height,state.height/model_height},
                             infantryPresentationYaw(state.heading),
                             mass_battle_session_->presentationSnapshot().metadata.tick,flags,
                             state.team==infantry::Team::Red
                                 ? foundation::Color{1.0F,0.78F,0.72F,1.0F}
                                 : foundation::Color{0.78F,0.87F,1.0F,1.0F}});
                    }
                    mass_battle_visible_units_=visible_units;
                    mass_battle_active_pose_slots_=static_cast<std::size_t>(std::count(
                        used_pose_slots.begin(),used_pose_slots.end(),true));
                    return;
                }

                const auto publish_live = [&](simulation::EntityId entity,
                                              infantry::Team team,
                                              foundation::Vec3 position,
                                              float heading,
                                              float height,
                                              std::uint64_t presentation_tick,
                                              const infantry::AnimationPose* pose,
                                              const render::SkinnedBonePalette* published_palette = nullptr,
                                              bool emit_instance = true) {
                    const foundation::StableId object_id =
                        foundation::stable_id("entity.infantry") ^ entity.packed();
                    render::SkinnedBonePalette palette{};
                    palette.instance_id = object_id;
                    palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                    palette.pose_revision = pose != nullptr ? pose->revision : 0U;
                    if (published_palette != nullptr) {
                        palette = *published_palette;
                        palette.instance_id = object_id;
                    } else if (pose != nullptr) {
                        const auto pose_span=std::span<const infantry::RigTransform>(pose->bones);
                        palette.matrices=infantry_presentation::makePalette(
                            infantry_model_artifact_->skeleton,pose_span);
                        // Mass Battle is consumed by the GPU skinning path;
                        // retaining a second local-space copy for every one
                        // of 2000 units only duplicates the same skeleton
                        // contract and creates avoidable allocator churn.
                        if (mass_battle_session_ == nullptr) {
                            palette.local_poses=infantry_presentation::makeLocalPoses(
                                infantry_model_artifact_->skeleton,pose_span);
                        }
                        palette.morph_weights[0]=pose->face.eyelids_close;
                        palette.morph_weights[1]=pose->face.eyelids_arc;
                        palette.morph_weights[2]=pose->face.neck_flex;
                        palette.morph_weights[3]=pose->face.hands_relax;
                    } else {
                        palette.matrices=bind_palette;
                        palette.local_poses=bind_local_poses;
                    }
                    context.presentation.skinned_palettes.push_back(std::move(palette));
                    if (!emit_instance) return;
                    const std::uint32_t instance_flags =
                        render::RenderInstanceFlagDynamic |
                        render::RenderInstanceFlagCastShadow |
                        render::RenderInstanceFlagReceiveShadow |
                        (team == infantry::Team::Red
                             ? render::RenderInstanceFlagTeamRed
                             : 0U);
                    context.presentation.instances.push_back(
                        {object_id,
                         infantry_skinned_prototype_->mesh_id,
                         team == infantry::Team::Blue ? blue_material : red_material,
                         position,
                         {height / model_height, height / model_height, height / model_height},
                         infantryPresentationYaw(heading),
                         presentation_tick,
                         instance_flags,
                         team == infantry::Team::Red
                             ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                             : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                };
                if (mass_battle_session_ != nullptr) {
                    const bool balanced_atlas_warmup =
                        mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                        mass_battle_atlas_requested && !mass_battle_pose_atlas_ready_;
                    consume_mass_battle_presentation();
                    if (balanced_atlas_warmup) {
                        // This asynchronous batch is the legacy all-skinned
                        // fallback. Drop it during atlas warmup so it cannot
                        // reintroduce 2000 GPU palettes after the static path
                        // has taken over.
                        ready_mass_battle_presentation_.reset();
                    }
                    if (!ready_mass_battle_presentation_) {
                        if (!balanced_atlas_warmup) schedule_mass_battle_presentation();
                    }
                    if (ready_mass_battle_presentation_) {
                        const auto ready = ready_mass_battle_presentation_;
                        context.presentation.instances.reserve(
                            context.presentation.instances.size() + ready->instances.size());
                        context.presentation.skinned_palettes.reserve(
                            context.presentation.skinned_palettes.size() + ready->states.size());
                        for (std::size_t index = 0U; index < ready->states.size(); ++index) {
                            const auto& state = ready->states[index];
                            const auto pose_found = poses.find(
                                foundation::stable_id("battlefield.infantry") ^
                                state.entity.packed());
                            const auto palette_found = published_palettes.find(
                                foundation::stable_id("entity.infantry") ^ state.entity.packed());
                            publish_live(state.entity, state.team, state.position, state.heading,
                                         state.height, ready->tick,
                                         pose_found != poses.end() ? pose_found->second : nullptr,
                                         palette_found != published_palettes.end()
                                             ? palette_found->second
                                             : nullptr,
                                         false);
                            context.presentation.instances.push_back(ready->instances[index]);
                        }
                        mass_battle_visible_units_ = ready->instances.size();
                        mass_battle_active_pose_slots_ = 0U;
                        ready_mass_battle_presentation_.reset();
                        return;
                    }
                    const auto& states = mass_battle_session_->presentationSnapshot().states;
                    context.presentation.instances.reserve(
                        context.presentation.instances.size() + states.size());
                    context.presentation.skinned_palettes.reserve(
                        context.presentation.skinned_palettes.size() +
                        (balanced_atlas_warmup ? poses.size() : states.size()));
                    const foundation::Vec3 camera_target =
                        context.presentation.camera.enabled
                            ? context.presentation.camera.target
                            : camera_request_.target;
                    std::size_t visible_units = 0U;
                    for (const auto& state : states) {
                        foundation::Vec3 position = state.position;
                        if (terrain_ != nullptr) {
                            position.y = terrain_->sampleBilinear(position.x, position.z) + 0.02F;
                        }
                        if (balanced_atlas_warmup &&
                            !massBattleUnitShouldRender(context.presentation,
                                                        mass_battle_profile_, position,
                                                        state.height, camera_target)) {
                            continue;
                        }
                        const auto pose_found = poses.find(
                            foundation::stable_id("battlefield.infantry") ^ state.entity.packed());
                        if (balanced_atlas_warmup &&
                            pose_found == poses.end() && render_infantry_mesh_) {
                            const foundation::StableId object_id =
                                foundation::stable_id("entity.infantry") ^ state.entity.packed();
                            const float shadow_dx = position.x - camera_target.x;
                            const float shadow_dz = position.z - camera_target.z;
                            const bool casts_shadow = shadow_dx * shadow_dx +
                                shadow_dz * shadow_dz <= 180.0F * 180.0F;
                            std::uint32_t flags = render::RenderInstanceFlagDynamic |
                                render::RenderInstanceFlagReceiveShadow |
                                (state.team == infantry::Team::Red
                                     ? render::RenderInstanceFlagTeamRed : 0U);
                            if (casts_shadow) flags |= render::RenderInstanceFlagCastShadow;
                            context.presentation.instances.push_back(
                                {object_id, render_infantry_mesh_->mesh_id,
                                 state.team == infantry::Team::Blue ? blue_material : red_material,
                                 position,
                                 {state.height / model_height, state.height / model_height,
                                  state.height / model_height},
                                 infantryPresentationYaw(state.heading),
                                 mass_battle_session_->presentationSnapshot().metadata.tick, flags,
                                 state.team == infantry::Team::Red
                                     ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                                     : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                        } else {
                            const auto palette_found = published_palettes.find(
                                foundation::stable_id("entity.infantry") ^ state.entity.packed());
                            publish_live(state.entity, state.team, position, state.heading,
                                         state.height, mass_battle_session_->presentationSnapshot().metadata.tick,
                                         pose_found != poses.end() ? pose_found->second : nullptr,
                                         palette_found != published_palettes.end()
                                             ? palette_found->second : nullptr);
                        }
                        ++visible_units;
                    }
                    mass_battle_visible_units_ = visible_units;
                    mass_battle_active_pose_slots_ = 0U;
                    return;
                }

                const std::uint64_t presentation_tick = battlefield_runtime_->snapshot().tick;
                const auto& presentation_states = battlefield_runtime_->presentationSnapshot().states;
                context.presentation.instances.reserve(context.presentation.instances.size() +
                                                       presentation_states.size());
                context.presentation.skinned_palettes.reserve(
                    context.presentation.skinned_palettes.size() + presentation_states.size());
                for (const infantry::InfantryRenderState& state : presentation_states) {
                    const foundation::StableId pose_id =
                        foundation::stable_id("battlefield.infantry") ^ state.entity.packed();
                    const auto pose_found = poses.find(pose_id);
                    publish_live(state.entity, state.team, state.position, state.heading,
                                 state.height, presentation_tick,
                                 pose_found != poses.end() ? pose_found->second : nullptr);
                }
                return;
            }
        }
        // There is deliberately no second, simplified infantry renderer here.
        // A failed compiler result leaves the scene without infantry until the
        // immutable model artifact can be rebuilt.
    }
#endif
}

} // namespace genomes::game_scenes
