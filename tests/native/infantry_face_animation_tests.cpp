#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/FacePhenotype.hpp>

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace genomes::infantry;
    FacePhenotype identity{};
    const auto created = FaceAnimator::create(0xF00DU, identity);
    assert(created);
    FaceAnimator animator = created.value();
    const auto before = animator.identity();
    assert(animator.setExpression(FaceExpression::Fear, 0.8F));
    assert(animator.setExpression(FaceExpression::Anger, 0.2F));
    assert(animator.setLookTarget(genomes::foundation::Vec3{1.0F, 0.2F, 4.0F}, {}));
    for (int index = 0; index < 240; ++index) {
        assert(animator.step(1.0F / 60.0F));
    }
    assert(animator.output().valid());
    assert(animator.output().head_yaw <= 0.95F && animator.output().head_yaw >= -0.95F);
    assert(animator.output().eyelids_close >= 0.0F && animator.output().eyelids_close <= 1.0F);
    assert(animator.output().neck_flex >= 0.0F && animator.output().neck_flex <= 0.35F);
    assert(animator.output().hands_relax == 0.0F);
    assert(animator.identity().eye_spacing == before.eye_spacing);

    const auto invalid = animator.setExpression(
        FaceExpression::Pain, std::numeric_limits<float>::quiet_NaN());
    assert(!invalid);
    assert(animator.setLookTarget(std::nullopt, {}));
    animator.clearExpressions();
    assert(animator.step(0.0F));

    auto second = FaceAnimator::create(0xF00DU, identity);
    assert(second);
    for (int index = 0; index < 240; ++index) {
        assert(second.value().step(1.0F / 60.0F));
    }
    assert(std::abs(animator.state().next_blink_seconds - second.value().state().next_blink_seconds) < 1.0e-6F);
    assert(animator.state().blink_event_index == second.value().state().blink_event_index);
    return 0;
}
