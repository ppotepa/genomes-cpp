#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace {
float quaternion_length(genomes::infantry::RigQuaternion q) {
    return std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
}
} // namespace

int main() {
    using namespace genomes::infantry;
    const auto genome=InfantryGenome::generate(0xA11CEU,1.0F); assert(genome);
    const auto phenotype=PhenotypeResolver::resolve(genome.value()); assert(phenotype);
    const auto controller_result=LocomotionController::create(phenotype.value().body);
    const auto rig_result=RigBuilder::build(phenotype.value().body,phenotype.value().face);
    assert(controller_result&&rig_result);
    auto controller=controller_result.value(); auto state=controller.initialState();
    auto rig=rig_result.value(); AnimationTransitionRuntime runtime{};
    AnimationEntity entity{}; entity.semantic_id=41U; entity.skeleton=&rig;
    entity.locomotion=&controller; entity.locomotion_state=&state;
    entity.transition_runtime=&runtime; entity.lod.setTier(AnimationLOD::Near);
    auto system_result=AnimationSystem::create(1U); assert(system_result);
    auto system=system_result.value(); std::array<AnimationEntity,1U> entities{entity};
    std::uint64_t tick=0U;
    const auto evaluate=[&] {
        assert(controller.step(state,1.0F/60.0F));
        assert(system.evaluate(entities,tick++,1.0F/60.0F));
        return system.currentSnapshot().poses.front();
    };

    static_assert(static_cast<unsigned>(AnimationState::REST)==0U);
    static_assert(static_cast<unsigned>(AnimationState::PRONE_MOVE)==8U);
    auto pose=evaluate(); assert(pose.active_state==AnimationState::IDLE);
    const auto revision=state.animation_request_revision;
    assert(controller.setState(state,AnimationState::IDLE));
    assert(state.animation_request_revision==revision);
    assert(controller.setState(state,AnimationState::IDLE,true));
    assert(state.animation_request_revision==revision+1U); pose=evaluate();

    assert(controller.setState(state,AnimationState::PRONE));
    bool saw_crouch=false,saw_support=false,saw_target=false;
    float previous_total=0.0F,previous_local=0.0F;
    auto previous_stage=AnimationTransitionStage::None;
    auto previous_hips=pose.damped_bones[boneIndex(BoneId::Hips)].translation;
    for(int frame=0;frame<120&&state.transition_active;++frame){
        pose=evaluate(); saw_crouch|=pose.transition_stage==AnimationTransitionStage::Crouch;
        saw_support|=pose.transition_stage==AnimationTransitionStage::Support;
        saw_target|=pose.transition_stage==AnimationTransitionStage::Target;
        assert(pose.transition_progress+1.0e-6F>=previous_total);
        if(previous_stage!=AnimationTransitionStage::None&&pose.transition_stage!=previous_stage&&
           state.transition_active) assert(pose.transition_stage_progress<=previous_local+0.15F);
        previous_total=pose.transition_progress; previous_local=pose.transition_stage_progress;
        previous_stage=pose.transition_stage;
        const auto hips=pose.damped_bones[boneIndex(BoneId::Hips)].translation;
        assert(std::abs(hips.y-previous_hips.y)<phenotype.value().body.height*.20F);
        previous_hips=hips;
        for(const auto& bone:pose.damped_bones)
            assert(std::abs(quaternion_length(bone.rotation)-1.0F)<2.0e-4F);
    }
    assert(saw_crouch&&saw_support&&saw_target);
    assert(!state.transition_active&&state.active_state==AnimationState::PRONE);

    assert(controller.setState(state,AnimationState::IDLE)); pose=evaluate();
    const auto before=pose.damped_bones[boneIndex(BoneId::Hips)].translation;
    assert(controller.setState(state,AnimationState::SITTING)); pose=evaluate();
    const auto after=pose.damped_bones[boneIndex(BoneId::Hips)].translation;
    assert(std::abs(after.y-before.y)<phenotype.value().body.height*.20F);
    const float frozen=runtime.total_duration; auto changed=state.transition_profile;
    changed.sitting_seconds=.01F; assert(controller.setTransitionProfile(state,changed));
    evaluate(); assert(std::abs(runtime.total_duration-frozen)<1.0e-6F);

    // Interrupt a live prone exit from the currently damped body. The new
    // request starts its own local clock and freezes its request profile.
    assert(controller.setState(state,AnimationState::IDLE,true)); evaluate();
    assert(controller.setState(state,AnimationState::PRONE)); evaluate();
    const auto interrupted_source=runtime.current;
    auto interruption_profile=state.transition_profile;
    interruption_profile.prone_exit_support_seconds=.73F;
    assert(controller.setState(state,AnimationState::WALK,false,interruption_profile));
    const auto changed_after_request=state.transition_profile;
    auto mutated_profile=changed_after_request;
    mutated_profile.prone_exit_support_seconds=.01F;
    assert(controller.setTransitionProfile(state,mutated_profile));
    pose=evaluate();
    assert(runtime.stage_elapsed<=1.0F/60.0F+1.0e-5F);
    assert(std::abs(runtime.profile.prone_exit_support_seconds-.73F)<1.0e-6F);
    const auto hips_source=interrupted_source.bones[boneIndex(BoneId::Hips)].translation;
    const auto hips_after=runtime.current.bones[boneIndex(BoneId::Hips)].translation;
    assert(std::abs(hips_after.y-hips_source.y)<phenotype.value().body.height*.20F);

    AnimationTransitionProfile zero{};
    zero.locomotion_seconds=zero.crouch_seconds=zero.crouch_walk_seconds=0.0F;
    zero.sitting_seconds=zero.prone_crouch_seconds=zero.prone_support_seconds=0.0F;
    zero.prone_seconds=zero.prone_exit_support_seconds=zero.prone_exit_crouch_seconds=0.0F;
    zero.prone_exit_seconds=0.0F;
    assert(controller.setState(state,AnimationState::PRONE_MOVE,false,zero)); pose=evaluate();
    assert(!state.transition_active&&state.active_state==AnimationState::PRONE_MOVE);
    const double phase=state.phase; assert(controller.setState(state,AnimationState::WALK,true));
    assert(state.phase==phase);
    pose=evaluate(); assert(state.active_state==AnimationState::WALK&&!state.transition_active);
    assert(controller.setTransitionProfile(state,AnimationTransitionProfile{}));

    constexpr std::array<AnimationState,9U> states{AnimationState::REST,AnimationState::IDLE,
        AnimationState::WALK,AnimationState::RUN,AnimationState::CROUCH,
        AnimationState::CROUCH_WALK,AnimationState::SITTING,AnimationState::PRONE,
        AnimationState::PRONE_MOVE};
    for(const auto from:states){
        assert(controller.setState(state,from,true)); evaluate();
        for(const auto target:states){
            assert(controller.setState(state,target));
            for(int frame=0;frame<160&&state.transition_active;++frame)evaluate();
            assert(!state.transition_active&&state.active_state==target);
        }
    }
    AnimationEntity invalid=entities.front(); invalid.transition_runtime=nullptr;
    assert(!invalid.valid());
    return 0;
}
