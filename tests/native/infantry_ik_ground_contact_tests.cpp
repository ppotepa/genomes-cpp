#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>
#include <genomes/infantry/GroundContact.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <array>
#include <cassert>
#include <cmath>

namespace {

bool flat_surface(void*, genomes::foundation::Vec3 position,
                  genomes::infantry::GroundSample& output) noexcept {
    output.height = 0.05F + position.x * 0.02F;
    output.normal = {-0.02F, 1.0F, 0.0F};
    return true;
}

bool constant_surface(void* context, genomes::foundation::Vec3,
                      genomes::infantry::GroundSample& output) noexcept {
    output.height = *static_cast<const float*>(context);
    output.normal = {0.0F, 1.0F, 0.0F};
    return true;
}

bool raised_slope_surface(void* context, genomes::foundation::Vec3 position,
                          genomes::infantry::GroundSample& output) noexcept {
    output.height = *static_cast<const float*>(context) + position.x * 0.05F;
    output.normal = {-0.05F, 1.0F, 0.0F};
    return true;
}

bool invalid_surface(void*, genomes::foundation::Vec3,
                     genomes::infantry::GroundSample&) noexcept {
    return false;
}

} // namespace

int main() {
    using namespace genomes::infantry;
    const auto reachable = TwoBoneIK::solve({0.0F, 1.0F, 0.0F}, {0.0F, 0.1F, 0.6F},
                                             {1.0F, 0.0F, 0.0F}, 0.6F, 0.6F);
    assert(reachable);
    assert(reachable.value().reachable);
    assert(reachable.value().residual_error < 1.0e-5F);
    const auto unreachable = TwoBoneIK::solve({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 2.0F},
                                               {1.0F, 0.0F, 0.0F}, 0.5F, 0.5F);
    assert(unreachable && !unreachable.value().reachable);
    assert(unreachable.value().residual_error > 0.0F);

    GroundContactInput input{};
    input.hips = {0.0F, 1.0F, 0.0F};
    input.left_foot = {-0.2F, 0.0F, 0.1F};
    input.right_foot = {0.2F, 0.0F, 0.1F};
    input.upper_leg_length = 0.55F;
    input.lower_leg_length = 0.55F;
    input.morphology_key = 17U;
    GroundSurfaceQuery surface{9U, nullptr, flat_surface};
    const auto contact = GroundContactSolver::solve(input, surface);
    assert(contact && contact.value().valid());
    assert(contact.value().surface_revision == 9U);
    assert(std::abs(contact.value().body_lift) < 1.0e-6F);
    assert(contact.value().feet[0].target_position.y > 0.0F);
    assert(contact.value().feet[0].normal.y > 0.0F);

    float raised_height = 1.10F;
    const GroundSurfaceQuery raised_surface{11U, &raised_height, constant_surface};
    const auto raised_contact = GroundContactSolver::solve(input, raised_surface);
    assert(raised_contact && raised_contact.value().valid());
    assert(raised_contact.value().body_lift > 0.10F);
    assert(raised_contact.value().body_lift <= input.max_body_lift);
    assert(raised_contact.value().clearance >= 0.0F);

    GroundContactCache cache;
    const auto cache_key = groundContactCacheKey(input, surface);
    cache.store(cache_key, contact.value());
    assert(cache.size() == 1U && cache.find(cache_key) != nullptr);
    surface.revision = 10U;
    assert(groundContactCacheKey(input, surface) != cache_key);
    const auto changed = GroundContactSolver::solve(input, surface);
    assert(changed && changed.value().surface_revision == 10U);

    // A low body with a raised gear/boot support must include that support in
    // the bounded lift calculation, even when the two feet remain on lower
    // terrain.
    GroundContactInput clearance_input = input;
    clearance_input.hips = {0.0F, 0.10F, 0.0F};
    clearance_input.left_foot = {-0.2F, 0.0F, 0.0F};
    clearance_input.right_foot = {0.2F, 0.0F, 0.0F};
    float slope_base = 0.04F;
    const GroundSurfaceQuery slope_surface{12U, &slope_base, raised_slope_surface};
    const auto without_support = GroundContactSolver::solve(clearance_input, slope_surface);
    assert(without_support);
    clearance_input.gear_supports[0] = {3.0F, 0.0F, 0.0F};
    clearance_input.gear_support_count = 1U;
    const auto with_support = GroundContactSolver::solve(clearance_input, slope_surface);
    assert(with_support);
    assert(with_support.value().body_lift > without_support.value().body_lift + 0.02F);

    // Persistent contacts follow the authored stance/swing cycle. A planted
    // foot may hold small drift, but a large drift reanchors and the swing
    // phase must release the old world-space point before the next replant.
    GroundContactRuntime contact_cycle{};
    const genomes::foundation::Vec3 planted{0.0F, 0.05F, 0.0F};
    for (int frame = 0; frame < 8; ++frame) {
        const auto resolved = contact_cycle.resolve(0U, planted, 1.0F, true, 0.20F,
                                                    0.05F + frame * 0.01F, true);
        assert(std::abs(resolved.x - planted.x) < 1.0e-6F);
    }
    assert(contact_cycle.feet[0].active);
    assert(contact_cycle.feet[0].locked);
    const auto held = contact_cycle.resolve(0U, {0.025F, 0.05F, 0.0F}, 1.0F, true,
                                            0.20F, 0.14F, true);
    assert(held.x < 0.025F);
    assert(contact_cycle.feet[0].reanchors == 0U);
    const auto reanchored = contact_cycle.resolve(0U, {0.35F, 0.05F, 0.0F}, 1.0F, true,
                                                  0.20F, 0.15F, true);
    assert(std::abs(reanchored.x - 0.35F) < 1.0e-6F);
    assert(contact_cycle.feet[0].reanchors == 1U);
    const auto swing = contact_cycle.resolve(0U, {0.42F, 0.05F, 0.0F}, 0.0F, true,
                                             0.20F, 0.42F, false);
    assert(std::abs(swing.x - 0.42F) < 1.0e-6F);
    assert(!contact_cycle.feet[0].active && !contact_cycle.feet[0].locked);
    const auto replanted = contact_cycle.resolve(0U, {0.50F, 0.05F, 0.0F}, 1.0F, true,
                                                 0.20F, 0.65F, true);
    assert(std::abs(replanted.x - 0.50F) < 1.0e-6F);
    assert(contact_cycle.feet[0].active);
    assert(!contact_cycle.feet[0].locked);
    for (int frame = 0; frame < 8; ++frame) {
        (void)contact_cycle.resolve(0U, {0.50F, 0.05F, 0.0F}, 1.0F, true, 0.20F,
                                    0.66F + frame * 0.01F, true);
    }
    assert(contact_cycle.feet[0].locked);
    const auto wrapped_replant = contact_cycle.resolve(
        0U, {0.58F, 0.05F, 0.0F}, 1.0F, true, 0.20F, 0.02F, true);
    assert(std::abs(wrapped_replant.x - 0.58F) < 1.0e-6F);
    assert(contact_cycle.feet[0].reanchors == 2U);
    assert(!contact_cycle.feet[0].locked);

    const auto hand_contact = contact_cycle.resolveHand(
        0U, {1.10F, 0.05F, 0.0F}, 1.0F, true, 0.20F, 0.02F, true);
    assert(std::abs(hand_contact.x - 1.10F) < 1.0e-6F);
    assert(contact_cycle.hands[0].active);
    assert(std::abs(contact_cycle.hands[0].point.x - 1.10F) < 1.0e-6F);
    assert(std::abs(contact_cycle.feet[0].point.x - 0.58F) < 1.0e-6F);

    // The animation integration must publish the bounded lift before the
    // second leg IK pass. `damped_bones` remains the body-only pose; `bones`
    // contains the contact-adjusted result.
    const auto genome = InfantryGenome::generate(0xC0A7AC7U, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto controller_result = LocomotionController::create(phenotype.value().body);
    const auto rig_result = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(controller_result && rig_result);
    auto controller = controller_result.value();
    auto locomotion_state = controller.initialState();
    auto rig = rig_result.value();
    const auto face_result = FaceAnimator::create(0xA11CEU, phenotype.value().face);
    assert(face_result);
    auto face = face_result.value();
    AnimationTransitionRuntime transition_runtime{};
    AnimationEntity entity{};
    entity.semantic_id = 73U;
    entity.skeleton = &rig;
    entity.locomotion = &controller;
    entity.locomotion_state = &locomotion_state;
    entity.transition_runtime = &transition_runtime;
    entity.face = &face;
    entity.lod.setTier(AnimationLOD::Near);
    auto system_result = AnimationSystem::create(1U);
    assert(system_result);
    auto system = system_result.value();
    std::array<AnimationEntity, 1U> entities{entity};
    constexpr float fixed_dt = 1.0F / 60.0F;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 0U, fixed_dt));
    const auto baseline = system.currentSnapshot().poses.front();
    const auto hips_index = boneIndex(BoneId::Hips);
    float contact_height = baseline.damped_bones[hips_index].translation.y + 0.08F -
                           phenotype.value().body.height * 0.0015F;
    GroundContactRuntime contact_runtime{};
    entities[0].ground_surface = {12U, &contact_height, raised_slope_surface};
    entities[0].ground_runtime = &contact_runtime;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 1U, fixed_dt));
    const auto& grounded = system.currentSnapshot().poses.front();
    const float published_lift = grounded.bones[hips_index].translation.y -
                                 grounded.damped_bones[hips_index].translation.y;
    assert(published_lift > 0.05F);
    assert(published_lift <= 0.20F + 1.0e-5F);
    assert(grounded.bones[hips_index].translation.y > baseline.bones[hips_index].translation.y);

    // Turning keeps the geometric terrain solve (lift and normal alignment),
    // but releases persistent contact memory for this transient motion.
    locomotion_state.turning = true;
    locomotion_state.turn_rate = 1.0F;
    locomotion_state.treadmill = false;
    contact_height = grounded.damped_bones[hips_index].translation.y + 0.10F -
                     phenotype.value().body.height * 0.0015F;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 2U, fixed_dt));
    const auto& turning = system.currentSnapshot().poses.front();
    assert(turning.bones[hips_index].translation.y > turning.damped_bones[hips_index].translation.y +
           0.05F);
    assert(std::abs(turning.foot_yaw[0]) > 1.0e-5F);
    assert(!contact_runtime.feet[0].active && !contact_runtime.feet[1].active);

    // Treadmill motion also updates terrain targets and lift without retaining
    // a world-space foot lock.
    locomotion_state.turning = false;
    locomotion_state.turn_rate = 0.0F;
    locomotion_state.treadmill = true;
    contact_height = turning.damped_bones[hips_index].translation.y + 0.07F -
                     phenotype.value().body.height * 0.0015F;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 3U, fixed_dt));
    const auto& treadmill = system.currentSnapshot().poses.front();
    assert(treadmill.bones[hips_index].translation.y > treadmill.damped_bones[hips_index].translation.y +
           0.03F);
    assert(treadmill.foot_targets[0].y > 0.0F);
    assert(std::abs(treadmill.foot_targets[0].y - turning.foot_targets[0].y) > 1.0e-4F);
    assert(!contact_runtime.feet[0].active && !contact_runtime.feet[1].active);

    // Stable movement retains the existing contact lock behavior.
    locomotion_state.treadmill = false;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 4U, fixed_dt));
    assert(contact_runtime.feet[0].active || contact_runtime.feet[1].active);

    // Surface-derived boot/gear supports participate in the same bounded body
    // lift path as terrain feet.
    AppearanceMesh clearance_surface{};
    AppearanceVertex clearance_vertex{};
    clearance_vertex.position = {3.0F, 0.0F, 0.0F};
    clearance_vertex.material_region =
        static_cast<std::uint16_t>(AppearanceMaterialRegion::BootLeather);
    clearance_surface.vertices.push_back(clearance_vertex);
    entities[0].surface = &clearance_surface;
    contact_height = grounded.damped_bones[hips_index].translation.y +
                     0.03F - 3.0F * 0.05F - phenotype.value().body.height * 0.0015F;
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 5U, fixed_dt));
    const auto& clearance = system.currentSnapshot().poses.front();
    const float clearance_lift = clearance.bones[hips_index].translation.y -
                                 clearance.damped_bones[hips_index].translation.y;
    assert(clearance_lift > 0.01F);
    assert(clearance_lift <= 0.20F + 1.0e-5F);
    entities[0].surface = nullptr;

    // An invalid surface never adds terrain correction and safely releases the
    // runtime that was active in the preceding stable tick.
    entities[0].ground_surface = {13U, nullptr, invalid_surface};
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 6U, fixed_dt));
    const auto& invalid = system.currentSnapshot().poses.front();
    assert(std::abs(invalid.bones[hips_index].translation.y -
                    invalid.damped_bones[hips_index].translation.y) < 1.0e-6F);
    assert(!contact_runtime.feet[0].active && !contact_runtime.feet[1].active);
    assert(!contact_runtime.hands[0].active && !contact_runtime.hands[1].active);

    // Head clearance is evaluated after the face look stage. A deliberately
    // high valid surface makes the bounded post-look lift observable without
    // involving foot contacts (the seated family has no ground solve).
    AppearanceMesh head_surface{};
    AppearanceVertex head_vertex{};
    head_vertex.position = rig.bones()[boneIndex(BoneId::Head)].world_bind.translation;
    head_vertex.influence_count = 1U;
    head_vertex.influences[0] = {
        static_cast<std::uint16_t>(boneIndex(BoneId::Head)), 1.0F};
    head_surface.vertices.push_back(head_vertex);
    const auto* helmet = EquipmentCatalog::findItem("helmet_standard");
    assert(helmet != nullptr);
    GearArtifact head_gear{};
    head_gear.fit.height = phenotype.value().body.height;
    head_gear.pieces.push_back({EquipmentSlot::Head, helmet->id, BoneId::Head,
                                {0.0F, 0.0F, 0.0F}, {0.20F, 0.20F, 0.20F}, {}, 0U});
    float high_surface = 10.0F;
    entities[0].surface = &head_surface;
    entities[0].gear = &head_gear;
    entities[0].ground_surface = {15U, &high_surface, constant_surface};
    entities[0].look_target = genomes::foundation::Vec3{4.0F, 0.4F, 8.0F};
    assert(controller.setState(locomotion_state, AnimationState::SITTING, true));
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 8U, fixed_dt));
    const auto& post_look = system.currentSnapshot().poses.front();
    const float post_look_lift = post_look.bones[hips_index].translation.y -
                                 post_look.damped_bones[hips_index].translation.y;
    assert(post_look_lift > 0.0F);
    assert(post_look_lift <= phenotype.value().body.height * 0.030F + 1.0e-5F);
    entities[0].surface = nullptr;
    entities[0].gear = nullptr;
    entities[0].look_target.reset();

    // Prone terrain uses independent persistent contacts for hands and feet;
    // all four authored supports are sampled on the uneven surface.
    contact_height = 0.12F;
    entities[0].ground_surface = {14U, &contact_height, raised_slope_surface};
    assert(controller.setState(locomotion_state, AnimationState::PRONE, true));
    assert(controller.step(locomotion_state, fixed_dt));
    assert(system.evaluate(entities, 9U, fixed_dt));
    const auto& prone = system.currentSnapshot().poses.front();
    assert(contact_runtime.feet[0].active && contact_runtime.feet[1].active);
    assert(contact_runtime.hands[0].active && contact_runtime.hands[1].active);
    assert(prone.foot_targets[0].y > 0.0F && prone.hand_targets[0].y > 0.0F);
    return 0;
}
