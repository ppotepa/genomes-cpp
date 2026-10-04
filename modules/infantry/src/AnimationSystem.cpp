#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace genomes::infantry {

bool AnimationEvaluationHandle::failed() const noexcept {
    return completion_.failed() || (error_ != nullptr && error_->has_value());
}

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] float mix(float a, float b, float alpha) noexcept {
    return a + (b - a) * alpha;
}

[[nodiscard]] float smooth5(float value) noexcept {
    const float t=std::clamp(value,0.0F,1.0F);
    return t*t*t*(t*(t*6.0F-15.0F)+10.0F);
}

[[nodiscard]] RigQuaternion mixQuaternion(RigQuaternion a,
                                           RigQuaternion b,
                                           float alpha) noexcept {
    const double t=static_cast<double>(alpha);
    double x=static_cast<double>(a.x)+(static_cast<double>(b.x)-a.x)*t;
    double y=static_cast<double>(a.y)+(static_cast<double>(b.y)-a.y)*t;
    double z=static_cast<double>(a.z)+(static_cast<double>(b.z)-a.z)*t;
    double w=static_cast<double>(a.w)+(static_cast<double>(b.w)-a.w)*t;
    const double length=std::sqrt(x*x+y*y+z*z+w*w);
    if (length > 1.0e-6F && finite(length)) {
        x/=length;y/=length;z/=length;w/=length;
        return {static_cast<float>(x),static_cast<float>(y),static_cast<float>(z),static_cast<float>(w)};
    } else {
        return RigQuaternion::identity();
    }
}

[[nodiscard]] RigQuaternion multiply(RigQuaternion left, RigQuaternion right) noexcept {
    return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
            left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] RigQuaternion inverse(RigQuaternion value) noexcept {
    return {-value.x,-value.y,-value.z,value.w};
}

[[nodiscard]] RigQuaternion slerp(RigQuaternion a,RigQuaternion b,float alpha) noexcept {
    double cosine=static_cast<double>(a.x)*b.x+static_cast<double>(a.y)*b.y+
        static_cast<double>(a.z)*b.z+static_cast<double>(a.w)*b.w;
    if(cosine<0.0){b={-b.x,-b.y,-b.z,-b.w};cosine=-cosine;}
    if(cosine>.9995)return mixQuaternion(a,b,alpha);
    const double theta=std::acos(std::min(1.0,cosine));
    const double sine=std::sin(theta),t=static_cast<double>(alpha);
    const double left=std::sin((1.0-t)*theta)/sine,right=std::sin(t*theta)/sine;
    return mixQuaternion(
        {static_cast<float>(a.x*left+b.x*right),
         static_cast<float>(a.y*left+b.y*right),
         static_cast<float>(a.z*left+b.z*right),
         static_cast<float>(a.w*left+b.w*right)},
        RigQuaternion::identity(), 0.0F);
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 a,foundation::Vec3 b) noexcept {
    return {a.x+b.x,a.y+b.y,a.z+b.z};
}
[[nodiscard]] foundation::Vec3 sub(foundation::Vec3 a,foundation::Vec3 b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
[[nodiscard]] foundation::Vec3 scale(foundation::Vec3 a,float value) noexcept {
    return {a.x*value,a.y*value,a.z*value};
}
[[nodiscard]] float dot(foundation::Vec3 a,foundation::Vec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 a,foundation::Vec3 b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 a,foundation::Vec3 fallback) noexcept {
    const float length=std::sqrt(dot(a,a));return length>1e-8F?scale(a,1.0F/length):fallback;
}

[[nodiscard]] RigQuaternion basisQuaternion(foundation::Vec3 x,foundation::Vec3 y,
                                             foundation::Vec3 z) noexcept {
    const float m00=x.x,m01=y.x,m02=z.x,m10=x.y,m11=y.y,m12=z.y,m20=x.z,m21=y.z,m22=z.z;
    RigQuaternion q{};const float trace=m00+m11+m22;
    if(trace>0.0F){const float s=.5F/std::sqrt(trace+1.0F);q.w=.25F/s;
        q.x=(m21-m12)*s;q.y=(m02-m20)*s;q.z=(m10-m01)*s;}
    else if(m00>m11&&m00>m22){const float s=2.0F*std::sqrt(1.0F+m00-m11-m22);
        q.w=(m21-m12)/s;q.x=.25F*s;q.y=(m01+m10)/s;q.z=(m02+m20)/s;}
    else if(m11>m22){const float s=2.0F*std::sqrt(1.0F+m11-m00-m22);
        q.w=(m02-m20)/s;q.x=(m01+m10)/s;q.y=.25F*s;q.z=(m12+m21)/s;}
    else {const float s=2.0F*std::sqrt(1.0F+m22-m00-m11);
        q.w=(m10-m01)/s;q.x=(m02+m20)/s;q.y=(m12+m21)/s;q.z=.25F*s;}
    return q;
}

[[nodiscard]] RigQuaternion fullFrameOrientation(foundation::Vec3 direction,
                                                   foundation::Vec3 forward) noexcept {
    const auto y=scale(direction,-1.0F);
    auto z=sub(forward,scale(y,dot(forward,y)));
    if(dot(z,z)<1e-8F)z=sub({0,0,1},scale(y,y.z));
    if(dot(z,z)<1e-8F)z=sub({1,0,0},scale(y,y.x));
    z=normalized(z,{0,0,1});auto x=normalized(cross(y,z),{1,0,0});
    z=normalized(cross(x,y),{0,0,1});return basisQuaternion(x,y,z);
}

[[nodiscard]] foundation::Vec3 rotate(foundation::Vec3 value,RigQuaternion q) noexcept {
    const foundation::Vec3 t{2.0F*(q.y*value.z-q.z*value.y),
        2.0F*(q.z*value.x-q.x*value.z),2.0F*(q.x*value.y-q.y*value.x)};
    return {value.x+q.w*t.x+(q.y*t.z-q.z*t.y),
            value.y+q.w*t.y+(q.z*t.x-q.x*t.z),
            value.z+q.w*t.z+(q.x*t.y-q.y*t.x)};
}

struct ModelPose final {
    std::array<foundation::Vec3,kRigBoneCount> positions{};
    std::array<RigQuaternion,kRigBoneCount> rotations{};
};

[[nodiscard]] ModelPose modelPose(const std::array<RigTransform,kRigBoneCount>& bones) noexcept {
    ModelPose result{};
    for(std::size_t index=0;index<kRigBoneCount;++index){
        const auto parent=kCanonicalRigSchema[index].parent;
        if(parent==kInvalidBoneIndex){
            result.positions[index]=bones[index].translation;
            result.rotations[index]=bones[index].rotation;
        }else{
            result.positions[index]=add(result.positions[parent],rotate(bones[index].translation,
                result.rotations[parent]));
            result.rotations[index]=multiply(result.rotations[parent],bones[index].rotation);
        }
    }
    return result;
}

[[nodiscard]] RigQuaternion axisAngle(foundation::Vec3 axis, float angle) noexcept {
    const float half = angle * 0.5F;
    const float sine = std::sin(half);
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(half)};
}

[[nodiscard]] RigQuaternion eulerXYZ(float x,float y,float z) noexcept {
    const float c1=std::cos(x*.5F),c2=std::cos(y*.5F),c3=std::cos(z*.5F);
    const float s1=std::sin(x*.5F),s2=std::sin(y*.5F),s3=std::sin(z*.5F);
    return {s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,
            c1*c2*s3+s1*s2*c3,c1*c2*c3-s1*s2*s3};
}

void setEuler(std::array<RigTransform,kRigBoneCount>& bones,BoneId id,
              float x,float y=0.0F,float z=0.0F) noexcept {
    bones[boneIndex(id)].rotation=eulerXYZ(x,y,z);
}

void rotateBone(std::array<RigTransform, kRigBoneCount>& bones,
                BoneId id,
                foundation::Vec3 axis,
                float angle) noexcept {
    if (std::abs(angle) <= 1.0e-6F) {
        return;
    }
    const std::size_t index = boneIndex(id);
    bones[index].rotation = multiply(axisAngle(axis, angle), bones[index].rotation);
}

[[maybe_unused]] void applyLocomotionPose(std::array<RigTransform, kRigBoneCount>& bones,
                         const LocomotionState& state,
                         const PostureSample& posture) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    const float phase = state.phase * 2.0F * pi;
    const float stride = std::clamp(state.actual_speed_mps / 3.25F, 0.0F, 1.0F);
    const float leg_swing = std::sin(phase) * (0.42F + 0.22F * state.run_weight) * stride;
    const float opposite_swing = std::sin(phase + pi) * (0.42F + 0.22F * state.run_weight) * stride;
    const float arm_swing = std::sin(phase + pi) * 0.28F * stride;
    rotateBone(bones, BoneId::ThighL, {1.0F, 0.0F, 0.0F}, leg_swing);
    rotateBone(bones, BoneId::ThighR, {1.0F, 0.0F, 0.0F}, opposite_swing);
    rotateBone(bones, BoneId::ShinL, {1.0F, 0.0F, 0.0F}, -leg_swing * 0.45F - posture.knee_bend);
    rotateBone(bones, BoneId::ShinR, {1.0F, 0.0F, 0.0F}, -opposite_swing * 0.45F - posture.knee_bend);
    rotateBone(bones, BoneId::UpperArmL, {1.0F, 0.0F, 0.0F}, arm_swing);
    rotateBone(bones, BoneId::UpperArmR, {1.0F, 0.0F, 0.0F}, -arm_swing);
    rotateBone(bones, BoneId::ForeArmL, {1.0F, 0.0F, 0.0F}, posture.arm_relax * 0.35F);
    rotateBone(bones, BoneId::ForeArmR, {1.0F, 0.0F, 0.0F}, posture.arm_relax * 0.35F);
    rotateBone(bones, BoneId::SpineLower, {1.0F, 0.0F, 0.0F}, posture.torso_pitch);
    rotateBone(bones, BoneId::SpineUpper, {1.0F, 0.0F, 0.0F}, posture.torso_pitch * 0.65F);
    rotateBone(bones, BoneId::Chest, {1.0F, 0.0F, 0.0F}, posture.torso_pitch * 0.35F);
}

void sampleBipedTargets(AnimationPose& pose, const LocomotionController& controller,
                        const LocomotionState& state) noexcept {
    const auto& body=controller.body();
    const float height=body.height, leg=body.anatomy_leg_length;
    const float amplitude = state.settling ? std::max(state.amplitude, 0.35F)
                                           : state.amplitude;
    const float sprint=state.sprint_weight*amplitude;
    const bool moving=state.actual_speed_mps>.008F || state.settling;
    const float move_angle=std::atan2(std::sin(state.move_angle),
                                      std::cos(state.move_angle));
    const float direction_yaw=std::clamp(move_angle*.40F,-.55F,.55F);
    const float turn_yaw=std::clamp(state.turn_rate*.10F,-.30F,.30F);
    const float target_yaw=direction_yaw+turn_yaw*(moving?.45F:1.0F);
    const float c=std::cos(target_yaw),s=std::sin(target_yaw);
    const auto steer=[c,s](foundation::Vec3 value) noexcept {
        return foundation::Vec3{value.x*c+value.z*s,value.y,
                                -value.x*s+value.z*c};
    };
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        const auto foot=PostureProfile::sampleLowContact(
            state.phase+static_cast<double>(index)*.5,state.duty,state.cycle_m,
            state.lift_m,sprint);
        pose.foot_targets[index]=steer({sign*(pose.posture.stance_half/height-.003F*sprint),
            .045F+(moving?foot.lift/height*amplitude:0.0F),
            pose.posture.foot_z/height+(moving?foot.z/height*amplitude:0.0F)});
        pose.knee_targets[index]=steer({sign*pose.posture.knee_half/height,
            .045F+leg/height*.42F,leg/height*.82F+(moving?foot.z/height*.22F:0.0F)});
        pose.foot_plant[index]=moving?foot.plant:1.0F;
        pose.foot_support[index]=moving?foot.support:1.0F;
        const double wrapped=std::fmod(state.phase+static_cast<double>(index)*.5,1.0);
        const float u=static_cast<float>((wrapped<0.0?wrapped+1.0:wrapped)/state.duty);
        const float toe_off=moving&&foot.stance?smooth5((u-.76F)/(1.0F-.76F)):0.0F;
        const float push_pitch=(.16F+.08F*sprint)+
            (.05F-(.16F+.08F*sprint))*state.actual_crouch;
        const float swing_pitch=moving&&!foot.stance?push_pitch*(1.0F-smooth5(foot.swing/.65F)):0.0F;
        pose.foot_pitch[index]=toe_off*push_pitch+swing_pitch;
        pose.toe_pitch[index]=-pose.foot_pitch[index]*.95F;
        pose.foot_yaw[index]=sign*(pose.posture.foot_yaw-.025F*sprint)+target_yaw;
    }
}

void constrainBipedHips(std::array<RigTransform,kRigBoneCount>& bones,
                        const std::array<foundation::Vec3,2U>& foot_targets,
                        const BodyPhenotype& body) noexcept {
    auto& hips=bones[boneIndex(BoneId::Hips)];
    float normalized_y=hips.translation.y/body.height;
    const float normalized_x=hips.translation.x/body.height;
    const float normalized_z=hips.translation.z/body.height;
    for(std::size_t index=0U;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        const auto hip_offset=rotate({sign*.052F*body.hip_width_scale,-.015F,0.0F},hips.rotation);
        const float dx=foot_targets[index].x-normalized_x-hip_offset.x;
        const float dz=foot_targets[index].z-normalized_z-hip_offset.z;
        const float reach=body.anatomy_leg_length/body.height*.992F;
        const float max_y=foot_targets[index].y+
            std::sqrt(std::max(.0001F,reach*reach-dx*dx-dz*dz))-hip_offset.y;
        normalized_y=std::min(normalized_y,max_y);
    }
    hips.translation.y=normalized_y*body.height;
}

void applyReferenceBipedPose(AnimationPose& pose,const LocomotionController& controller,
                             const LocomotionState& state,float time) noexcept {
    constexpr float tau=6.28318530717958647692F;
    const auto& body=controller.body();
    const float depth=state.actual_crouch,run=state.run_weight;
    const bool moving=state.actual_speed_mps>.008F || state.settling;
    const float amplitude=moving ? std::max(state.amplitude,
                                            state.settling ? 0.35F : 0.0F) : 0.0F;
    const float sprint=state.sprint_weight*amplitude;
    const float move_angle=std::atan2(std::sin(state.move_angle),
                                      std::cos(state.move_angle));
    const float directional_yaw=std::clamp(move_angle*.40F,-.55F,.55F);
    const float turn_anticipation=std::clamp(state.turn_rate*.12F,-.35F,.35F) *
        (.35F+.65F*amplitude);
    const float directional_weight=.25F+.75F*amplitude;
    const float directional_roll=std::sin(move_angle)*(.018F+.022F*run)*directional_weight;
    pose.face.hands_relax=(.35F*run+.50F*sprint)*amplitude*
        (1.0F-smooth5((depth-.15F)/(.60F-.15F)));
    const float phase=static_cast<float>(state.phase);
    const float breath=std::sin(time*1.42F),phase_wave=std::cos(tau*phase);
    const float support_sum=pose.foot_support[0]+pose.foot_support[1];
    const float support_bias=moving&&support_sum>.01F?
        (pose.foot_support[0]-pose.foot_support[1])/support_sum:0.0F;
    const float yaw=phase_wave*((.034F+.020F*sprint)+(.017F-(.034F+.020F*sprint))*depth)*amplitude;
    const float roll=-support_bias*(.038F+(.018F-.038F)*depth);
    const float bob=moving?std::cos(tau*(phase+.04F*run)*2.0F)*body.height*
        (((.007F+(.014F-.007F)*run)+(.002F-(.007F+(.014F-.007F)*run))*depth))*amplitude:
        breath*.0012F*body.height;
    const float compression=body.anatomy_leg_length*run*(.045F+.03F*sprint)*(1.0F-depth)*amplitude;
    pose.bones[boneIndex(BoneId::Hips)].translation={support_bias*body.height*(.016F+(.006F-.016F)*depth),
        pose.posture.hip_y-compression+bob,pose.posture.hip_z};
    const float running_lean=(run*.065F+sprint*.045F)*(1.0F-depth);
    setEuler(pose.bones,BoneId::Hips,pose.posture.pelvis_pitch+running_lean,
             yaw+directional_yaw*directional_weight*.55F+turn_anticipation,
             roll+directional_roll);
    setEuler(pose.bones,BoneId::SpineLower,pose.posture.lower_pitch+running_lean*.55F+breath*.002F,
             -yaw*(.30F+.10F*sprint)+directional_yaw*directional_weight*.24F+
                 turn_anticipation*.65F,
             -roll*.35F+directional_roll*.55F);
    setEuler(pose.bones,BoneId::SpineUpper,pose.posture.upper_pitch+running_lean*.35F+breath*.003F,
             -yaw*(.38F+.22F*sprint)+directional_yaw*directional_weight*.16F+
                 turn_anticipation*.42F,
             -roll*.40F+directional_roll*.35F);
    setEuler(pose.bones,BoneId::Chest,pose.posture.chest_pitch+breath*.002F,
             -yaw*(.20F+.10F*sprint)+directional_yaw*directional_weight*.08F+
                 turn_anticipation*.24F,
             -roll*.20F+directional_roll*.20F);
    const float lean=pose.posture.pelvis_pitch+pose.posture.lower_pitch+
        pose.posture.upper_pitch+pose.posture.chest_pitch;
    setEuler(pose.bones,BoneId::Neck,-lean*.46F-running_lean*1.25F,-yaw*.07F,roll*.12F);
    setEuler(pose.bones,BoneId::Head,-lean*.13F-running_lean*.55F,-yaw*.05F,roll*.10F);

    const float lower=1.0F-std::exp(-2.0F*depth);
    const float clearance=std::clamp(std::max(0.0F,body.chest_width_scale-1.0F)*.08F,0.0F,.035F);
    constexpr float arm_angle=.3839724354387525F;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F,t=tau*(phase+static_cast<float>(index)*.5F);
        const float wave=std::cos(t)+.05F*std::sin(3.0F*t);
        const float forward=std::max(0.0F,-wave),back=std::max(0.0F,wave);
        const float pitch=wave*(.34F+(.66F-.34F)*run)-sprint*(.06F+.14F*forward);
        const float elbow=((.18F+(.88F-.18F)*run)+forward*(.18F+(.24F-.18F)*run));
        const float sprint_elbow=1.48F+.10F*back-.10F*forward;
        const float blended_elbow=elbow+(sprint_elbow-elbow)*sprint;
        const float low_pitch=-.12F+std::cos(t)*(.34F+(.10F-.34F)*depth);
        const float low_elbow=.23F+.26F*depth+forward*.12F;
        const float upper_x=(-.12F*depth)+
            (((pitch+(low_pitch-pitch)*lower)-(-.12F*depth))*amplitude);
        const float upper_z=sign*(-arm_angle+(.055F+.05F*depth)+
            (((.072F+(.095F-.072F)*run)+sprint*(.025F+clearance))+(
             .08F+.05F*depth-((.072F+(.095F-.072F)*run)+sprint*(.025F+clearance)))*lower-
             (.055F+.05F*depth))*amplitude);
        const BoneId upper=index==0U?BoneId::UpperArmL:BoneId::UpperArmR;
        const BoneId fore=index==0U?BoneId::ForeArmL:BoneId::ForeArmR;
        const BoneId clavicle=index==0U?BoneId::ClavicleL:BoneId::ClavicleR;
        const BoneId hand=index==0U?BoneId::HandL:BoneId::HandR;
        setEuler(pose.bones,upper,upper_x,0.0F,upper_z);
        const float fore_x=-((.065F+.34F*depth)+
            (((blended_elbow+(low_elbow-blended_elbow)*lower)-(.065F+.34F*depth))*amplitude));
        setEuler(pose.bones,fore,fore_x);
        const foundation::Vec3 axis{std::sin(arm_angle)*sign,-std::cos(arm_angle),0.0F};
        pose.bones[boneIndex(fore)].rotation=multiply(pose.bones[boneIndex(fore)].rotation,
            axisAngle(axis,sign*(.85F*run+.35F*sprint)*amplitude*(1.0F-lower)));
        setEuler(pose.bones,clavicle,0.0F,-wave*(.03F+(.065F-.03F)*run)*amplitude*(1.0F-lower),
                 -sign*.009F*std::sin(t)*amplitude*(1.0F-lower));
        setEuler(pose.bones,hand,-wave*(.045F+(.06F-.045F)*run)*amplitude*(1.0F-lower),
                 sign*.025F*sprint*amplitude);
    }
    auto& hips=pose.bones[boneIndex(BoneId::Hips)];
    float normalized_y=hips.translation.y/body.height;
    const float normalized_x=hips.translation.x/body.height;
    const float normalized_z=hips.translation.z/body.height;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        const auto hip_offset=rotate({sign*.052F*body.hip_width_scale,-.015F,0.0F},hips.rotation);
        const float dx=pose.foot_targets[index].x-normalized_x-hip_offset.x;
        const float dz=pose.foot_targets[index].z-normalized_z-hip_offset.z;
        const float reach=body.anatomy_leg_length/body.height*.992F;
        const float max_y=pose.foot_targets[index].y+
            std::sqrt(std::max(.0001F,reach*reach-dx*dx-dz*dz))-hip_offset.y;
        normalized_y=std::min(normalized_y,max_y);
    }
    hips.translation.y=normalized_y*body.height;
}

void solveFullFrameChain(AnimationPose& pose,const SkeletonData& skeleton,BoneId upper,
                         BoneId lower,BoneId tip,foundation::Vec3 target,
                         foundation::Vec3 pole,
                         std::optional<foundation::Vec3> orientation_hint=std::nullopt) noexcept {
    auto model=modelPose(pose.bones);
    const auto upper_index=boneIndex(upper),lower_index=boneIndex(lower),tip_index=boneIndex(tip);
    const auto rest=skeleton.bones();
    const auto rest_upper=rest[upper_index].world_bind.translation;
    const auto rest_lower=rest[lower_index].world_bind.translation;
    const auto rest_tip=rest[tip_index].world_bind.translation;
    const float upper_length=std::sqrt(dot(sub(rest_lower,rest_upper),sub(rest_lower,rest_upper)));
    const float lower_length=std::sqrt(dot(sub(rest_tip,rest_lower),sub(rest_tip,rest_lower)));
    const auto solution=TwoBoneIK::solve(model.positions[upper_index],target,pole,
                                         upper_length,lower_length);
    if(!solution)return;
    const auto axis=normalized(sub(target,model.positions[upper_index]),{0,-1,0});
    auto bend=sub(pole,model.positions[upper_index]);
    bend=normalized(sub(bend,scale(axis,dot(bend,axis))),{0,0,1});
    const auto upper_direction=normalized(sub(solution.value().joint_position,
        model.positions[upper_index]),{0,-1,0});
    const auto lower_direction=normalized(sub(solution.value().end_position,
        solution.value().joint_position),{0,-1,0});
    const auto upper_rest_direction=normalized(sub(rest_lower,rest_upper),{0,-1,0});
    const auto lower_rest_direction=normalized(sub(rest_tip,rest_lower),{0,-1,0});
    const auto forward=orientation_hint.value_or(bend);
    const auto upper_model=multiply(fullFrameOrientation(upper_direction,forward),
        inverse(fullFrameOrientation(upper_rest_direction,{0,0,1})));
    const auto lower_model=multiply(fullFrameOrientation(lower_direction,forward),
        inverse(fullFrameOrientation(lower_rest_direction,{0,0,1})));
    const auto upper_parent=kCanonicalRigSchema[upper_index].parent;
    pose.bones[upper_index].rotation=upper_parent==kInvalidBoneIndex?upper_model:
        multiply(inverse(model.rotations[upper_parent]),upper_model);
    pose.bones[lower_index].rotation=multiply(inverse(upper_model),lower_model);
}

void applyProneArmIK(AnimationPose& pose,const LocomotionController& controller,
                     const SkeletonData& skeleton) noexcept {
    const float height=controller.body().height;
    const auto rest=skeleton.bones();
    for(std::size_t index=0;index<2U;++index){
        const BoneId upper=index==0U?BoneId::UpperArmL:BoneId::UpperArmR;
        const BoneId lower=index==0U?BoneId::ForeArmL:BoneId::ForeArmR;
        const BoneId hand=index==0U?BoneId::HandL:BoneId::HandR;
        auto target=scale(pose.hand_targets[index],height);
        const auto pole=scale(pose.elbow_targets[index],height);
        solveFullFrameChain(pose,skeleton,upper,lower,hand,target,pole,
                            foundation::Vec3{0.0F,-1.0F,0.0F});
        const auto model=modelPose(pose.bones);
        const auto rest_direction=normalized(sub(rest[boneIndex(hand)].world_bind.translation,
            rest[boneIndex(lower)].world_bind.translation),{0,-1,0});
        const foundation::Vec3 bind_z{0,0,1};
        const auto bind_x=normalized(cross(rest_direction,bind_z),{1,0,0});
        const auto bind_frame=basisQuaternion(bind_x,rest_direction,bind_z);
        const auto ground_frame=basisQuaternion({1,0,0},{0,0,1},{0,-1,0});
        const auto hand_model=multiply(ground_frame,inverse(bind_frame));
        pose.bones[boneIndex(hand)].rotation=multiply(
            inverse(model.rotations[boneIndex(lower)]),hand_model);
    }
}

[[nodiscard]] foundation::Vec3 supportOffset(const AppearanceMesh* surface,
                                             std::string_view tag_name,
                                             foundation::Vec3 bone_point,
                                             RigQuaternion foot_model,
                                             float height) noexcept {
    if(surface==nullptr)return {0.0F,-height*.045F,0.0F};
    std::vector<std::uint32_t> selected;
    const std::string_view boot_tag=tag_name=="sole.L"?"boot.L":"boot.R";
    for(const auto& tag:surface->tags)
        if(tag.name==tag_name)
            selected.insert(selected.end(),tag.vertices.begin(),tag.vertices.end());
    for(const auto& tag:surface->tags){
        if(tag.name!=boot_tag)continue;
        const std::size_t step=std::max<std::size_t>(1U,tag.vertices.size()/64U);
        for(std::size_t index=0U;index<tag.vertices.size();index+=step)
            selected.push_back(tag.vertices[index]);
    }
    if(selected.empty())return {0.0F,-height*.045F,0.0F};
    foundation::Vec3 best{};float minimum=std::numeric_limits<float>::infinity();
    for(const auto vertex_index:selected){
        if(vertex_index>=surface->vertices.size())continue;
        const auto local=sub(surface->vertices[vertex_index].position,bone_point);
        if(local.y>height*.026F)continue;
        const auto rotated=rotate(local,foot_model);
        if(rotated.y<minimum){minimum=rotated.y;best=local;}
    }
    if(std::isfinite(minimum))return rotate(best,foot_model);
    return foundation::Vec3{0.0F,-height*.045F,0.0F};
}

[[nodiscard]] RigQuaternion proneHandModelRotation(const SkeletonData& skeleton,
                                                    std::size_t side) noexcept {
    const BoneId lower = side == 0U ? BoneId::ForeArmL : BoneId::ForeArmR;
    const BoneId hand = side == 0U ? BoneId::HandL : BoneId::HandR;
    const auto bones = skeleton.bones();
    const auto direction = normalized(sub(bones[boneIndex(hand)].world_bind.translation,
                                          bones[boneIndex(lower)].world_bind.translation),
                                      {0.0F, -1.0F, 0.0F});
    const foundation::Vec3 bind_z{0.0F, 0.0F, 1.0F};
    const auto bind_x = normalized(cross(direction, bind_z), {1.0F, 0.0F, 0.0F});
    const auto bind_frame = basisQuaternion(bind_x, direction, bind_z);
    const auto ground_frame = basisQuaternion({1.0F, 0.0F, 0.0F},
                                               {0.0F, 0.0F, 1.0F},
                                               {0.0F, -1.0F, 0.0F});
    return multiply(ground_frame, inverse(bind_frame));
}

[[nodiscard]] foundation::Vec3 proneHandSupportOffset(const AppearanceMesh* surface,
                                                       std::string_view tag_name,
                                                       foundation::Vec3 bone_point,
                                                       RigQuaternion hand_model) noexcept {
    if (surface == nullptr) return {0.0F, -0.01F, 0.0F};
    foundation::Vec3 best{};
    float minimum = std::numeric_limits<float>::infinity();
    for (const auto& tag : surface->tags) {
        if (tag.name != tag_name) continue;
        for (const std::uint32_t vertex_index : tag.vertices) {
            if (vertex_index >= surface->vertices.size()) continue;
            const auto local = sub(surface->vertices[vertex_index].position, bone_point);
            const auto rotated = rotate(local, hand_model);
            if (rotated.y < minimum) {
                minimum = rotated.y;
                best = local;
            }
        }
    }
    return std::isfinite(minimum) ? rotate(best, hand_model)
                                  : foundation::Vec3{0.0F, -0.01F, 0.0F};
}

void appendClearanceSupport(std::array<foundation::Vec3, 4U>& supports,
                            std::uint8_t& count, foundation::Vec3 point) noexcept {
    if (!finite(point)) return;
    if (count < supports.size()) {
        supports[count++] = point;
        return;
    }
    std::size_t highest = 0U;
    for (std::size_t index = 1U; index < supports.size(); ++index)
        if (supports[index].y > supports[highest].y) highest = index;
    if (point.y < supports[highest].y) supports[highest] = point;
}

[[nodiscard]] foundation::Vec3 transformPoint(const RigTransform& transform,
                                               foundation::Vec3 point) noexcept;
[[nodiscard]] foundation::Vec3 poseBindPoint(foundation::Vec3 point, BoneId bone,
                                              const SkeletonData& skeleton,
                                              const ModelPose& pose) noexcept;
[[nodiscard]] foundation::Vec3 skinnedPoint(const AppearanceVertex& vertex,
                                            const SkeletonData& skeleton,
                                            const ModelPose& pose) noexcept;

void appendSurfaceClearanceSupports(GroundContactInput& input,
                                    const AnimationEntity& entity,
                                    const AnimationPose& pose,
                                    const SkeletonData& skeleton) noexcept {
    const ModelPose posed = modelPose(pose.bones);
    if (entity.surface != nullptr) {
        foundation::Vec3 lowest{};
        float lowest_y = std::numeric_limits<float>::infinity();
        foundation::Vec3 left_boot{};
        foundation::Vec3 right_boot{};
        float left_boot_y = std::numeric_limits<float>::infinity();
        float right_boot_y = std::numeric_limits<float>::infinity();
        const auto boot_region = static_cast<std::uint16_t>(
            AppearanceMaterialRegion::BootLeather);
        for (const auto& vertex : entity.surface->vertices) {
            const foundation::Vec3 point = skinnedPoint(vertex, skeleton, posed);
            if (point.y < lowest_y) {
                lowest_y = point.y;
                lowest = point;
            }
            if (vertex.material_region != boot_region) continue;
            if (point.x >= 0.0F && point.y < left_boot_y) {
                left_boot_y = point.y;
                left_boot = point;
            } else if (point.x < 0.0F && point.y < right_boot_y) {
                right_boot_y = point.y;
                right_boot = point;
            }
        }
        if (std::isfinite(lowest_y))
            appendClearanceSupport(input.gear_supports, input.gear_support_count,
                                   add(entity.root_position, lowest));
        if (std::isfinite(left_boot_y))
            appendClearanceSupport(input.gear_supports, input.gear_support_count,
                                   add(entity.root_position, left_boot));
        if (std::isfinite(right_boot_y))
            appendClearanceSupport(input.gear_supports, input.gear_support_count,
                                   add(entity.root_position, right_boot));
    }
    if (entity.gear == nullptr) return;
    const float fit_height = std::isfinite(entity.gear->fit.height) &&
                             entity.gear->fit.height > 0.0F
        ? entity.gear->fit.height : 1.0F;
    const auto bones = skeleton.bones();
    for (const auto& piece : entity.gear->pieces) {
        const std::size_t bone = boneIndex(piece.bone);
        if (bone >= bones.size() || !finite(piece.center) || !finite(piece.dimensions) ||
            piece.dimensions.y <= 0.0F) continue;
        const foundation::Vec3 center = scale(piece.center, fit_height);
        const foundation::Vec3 half = scale(piece.dimensions, fit_height * 0.5F);
        foundation::Vec3 lowest{};
        float lowest_y = std::numeric_limits<float>::infinity();
        for (const float x : {-half.x, half.x}) {
            for (const float z : {-half.z, half.z}) {
                const foundation::Vec3 point = poseBindPoint(
                    {center.x + x, center.y - half.y, center.z + z}, piece.bone,
                    skeleton, posed);
                if (point.y < lowest_y) {
                    lowest_y = point.y;
                    lowest = point;
                }
            }
        }
        if (std::isfinite(lowest_y))
            appendClearanceSupport(input.gear_supports, input.gear_support_count,
                                   add(entity.root_position, lowest));
    }
}

void applyBipedLegIK(AnimationPose& pose,const LocomotionController& controller,
                      const SkeletonData& skeleton,const AppearanceMesh* surface=nullptr) noexcept {
    const auto& body=controller.body();
    const float height=body.height;
    for(std::size_t index=0;index<2U;++index){
        const auto raw_target=scale(pose.foot_targets[index],height);
        auto target=raw_target;
        const auto pole=scale(pose.knee_targets[index],height);
        const BoneId thigh=index==0U?BoneId::ThighL:BoneId::ThighR;
        const BoneId shin=index==0U?BoneId::ShinL:BoneId::ShinR;
        const BoneId foot=index==0U?BoneId::FootL:BoneId::FootR;
        const BoneId toes=index==0U?BoneId::ToesL:BoneId::ToesR;
        solveFullFrameChain(pose,skeleton,thigh,shin,foot,target,pole);
        const auto model=modelPose(pose.bones);
        const auto lower_model=model.rotations[boneIndex(shin)];
        const auto ground_model=eulerXYZ(pose.foot_pitch[index],pose.foot_yaw[index],0.0F);
        const auto shin_relative=multiply(lower_model,
            eulerXYZ(pose.ankle_pitch[index],pose.ankle_yaw[index],0.0F));
        const auto foot_model=slerp(ground_model,shin_relative,pose.foot_relative[index]);
        const auto side=index==0U?"sole.L":"sole.R";
        const auto offset=supportOffset(surface,side,
                                        skeleton.bones()[boneIndex(foot)].world_bind.translation,
                                        foot_model,height);
        const float lift=std::max(0.0F,raw_target.y-height*.045F);
        target.y=.0015F-offset.y+lift;
        pose.foot_goals[index]=target;
        solveFullFrameChain(pose,skeleton,thigh,shin,foot,target,pole);
        const auto final_model=modelPose(pose.bones);
        const auto final_lower=final_model.rotations[boneIndex(shin)];
        const auto final_shin_relative=multiply(final_lower,
            eulerXYZ(pose.ankle_pitch[index],pose.ankle_yaw[index],0.0F));
        const auto final_foot_model=slerp(ground_model,final_shin_relative,pose.foot_relative[index]);
        pose.bones[boneIndex(foot)].rotation=multiply(inverse(lower_model),foot_model);
        pose.bones[boneIndex(foot)].rotation=multiply(inverse(final_lower),final_foot_model);
        pose.bones[boneIndex(toes)].rotation=eulerXYZ(pose.toe_pitch[index],0.0F,0.0F);
    }
}

void applyBipedBootClearanceIK(AnimationPose& pose,const AnimationEntity& entity,
                               const LocomotionController& controller) noexcept {
    if (!entity.ground_surface.valid() || entity.surface == nullptr) return;
    const auto& skeleton = *entity.skeleton;
    const auto& surface = *entity.surface;
    const float height = controller.body().height;
    for (std::size_t side = 0U; side < 2U; ++side) {
        const BoneId thigh = side == 0U ? BoneId::ThighL : BoneId::ThighR;
        const BoneId shin = side == 0U ? BoneId::ShinL : BoneId::ShinR;
        const BoneId foot = side == 0U ? BoneId::FootL : BoneId::FootR;
        const std::string_view sole_name = side == 0U ? "sole.L" : "sole.R";
        const std::string_view boot_name = side == 0U ? "boot.L" : "boot.R";
        std::vector<std::uint32_t> supports;
        for (const auto& tag : surface.tags) {
            if (tag.name == sole_name) {
                supports.insert(supports.end(), tag.vertices.begin(), tag.vertices.end());
            } else if (tag.name == boot_name) {
                const std::size_t step = std::max<std::size_t>(1U,
                    tag.vertices.size() / 64U);
                for (std::size_t index = 0U; index < tag.vertices.size(); index += step)
                    supports.push_back(tag.vertices[index]);
            }
        }
        for (std::size_t pass = 0U; pass < 2U; ++pass) {
            const auto model = modelPose(pose.bones);
            float minimum_clearance = std::numeric_limits<float>::infinity();
            for (const std::uint32_t vertex_index : supports) {
                if (vertex_index >= surface.vertices.size()) continue;
                const auto local = skinnedPoint(surface.vertices[vertex_index], skeleton, model);
                const auto world = add(entity.root_position, local);
                GroundSample sample{};
                if (!entity.ground_surface.sample(entity.ground_surface.context, world, sample) ||
                    !std::isfinite(sample.height) || !finite(sample.normal) ||
                    sample.normal.y <= 0.0F) continue;
                minimum_clearance = std::min(minimum_clearance, world.y - sample.height);
            }
            if (!std::isfinite(minimum_clearance) || minimum_clearance >= 0.0006F) break;
            const float correction = std::min(0.045F * height, 0.001F - minimum_clearance);
            pose.foot_goals[side].y += correction;
            solveFullFrameChain(pose, skeleton, thigh, shin, foot,
                pose.foot_goals[side], scale(pose.knee_targets[side], height));
            const auto corrected = modelPose(pose.bones);
            const auto lower = corrected.rotations[boneIndex(shin)];
            const auto ground = eulerXYZ(pose.foot_pitch[side], pose.foot_yaw[side], 0.0F);
            const auto shin_relative = multiply(lower,
                eulerXYZ(pose.ankle_pitch[side], pose.ankle_yaw[side], 0.0F));
            const auto foot_model = slerp(ground, shin_relative, pose.foot_relative[side]);
            pose.bones[boneIndex(foot)].rotation = multiply(inverse(lower), foot_model);
        }
    }
}

[[nodiscard]] bool applyProneGroundContact(AnimationPose& pose,
                                            const AnimationEntity& entity,
                                            const LocomotionController& controller,
                                            const LocomotionState& state) noexcept;

[[nodiscard]] bool applyGroundContact(AnimationPose& pose, const AnimationEntity& entity,
                                       const LocomotionController& controller,
                                       const LocomotionState& state) noexcept {
    if (!entity.ground_surface.valid()) {
        if (entity.ground_runtime != nullptr) entity.ground_runtime->reset();
        return false;
    }
    if (state.family == LocomotionFamily::Prone) {
        return applyProneGroundContact(pose, entity, controller, state);
    }
    if (state.family != LocomotionFamily::Biped) {
        if (entity.ground_runtime != nullptr) entity.ground_runtime->reset();
        return false;
    }
    const auto& body = controller.body();
    const float height = body.height;
    GroundContactInput input{};
    input.hips = add(entity.root_position,
                     pose.bones[boneIndex(BoneId::Hips)].translation);
    input.left_foot = add(entity.root_position, scale(pose.foot_targets[0], height));
    input.right_foot = add(entity.root_position, scale(pose.foot_targets[1], height));
    input.left_pole = add(entity.root_position, scale(pose.knee_targets[0], height));
    input.right_pole = add(entity.root_position, scale(pose.knee_targets[1], height));
    input.upper_leg_length = body.anatomy_leg_length * 0.50F;
    input.lower_leg_length = body.anatomy_leg_length * 0.50F;
    input.sole_offset = height * 0.0015F;
    input.morphology_key = controller.body().version;
    appendSurfaceClearanceSupports(input, entity, pose, *entity.skeleton);
    const auto solved = GroundContactSolver::solve(input, entity.ground_surface);
    if (!solved) {
        if (entity.ground_runtime != nullptr) entity.ground_runtime->reset();
        return false;
    }
    const auto& output = solved.value();
    pose.bones[boneIndex(BoneId::Hips)].translation.y += output.body_lift;
    const bool contacts_enabled = !state.treadmill && !state.turning &&
                                  std::abs(state.turn_rate) < 0.32F;
    const float max_drift = std::max(0.02F, input.max_replant_distance);
    for (std::size_t index = 0U; index < 2U; ++index) {
        const float weight = pose.foot_plant[index];
        foundation::Vec3 target = output.feet[index].target_position;
        if (entity.ground_runtime != nullptr) {
            entity.ground_runtime->feet[index].normal = output.feet[index].normal;
            const float foot_phase = static_cast<float>(state.phase) +
                                     static_cast<float>(index) * 0.5F;
            const bool stance = pose.foot_support[index] > 0.35F && weight > 0.35F;
            target = entity.ground_runtime->resolve(index, target, weight,
                                                    contacts_enabled, max_drift,
                                                    foot_phase, stance);
        }
        // A treadmill suppresses planted-contact reanchoring but preserves the
        // authored ankle trajectory; the surface still supplies its normal.
        if (!state.treadmill) {
            pose.foot_targets[index] = scale(sub(target, entity.root_position), 1.0F / height);
        }
        // Align the sole with the sampled terrain normal while preserving the
        // authored toe/foot progression from PostureProfile.
        const auto& normal = output.feet[index].normal;
        pose.foot_pitch[index] += std::atan2(normal.z, std::max(1.0e-4F, normal.y));
        pose.foot_yaw[index] += std::atan2(normal.x, std::max(1.0e-4F, normal.y)) *
                                (index == 0U ? 1.0F : -1.0F);
    }
    return true;
}

void applyWeaponOverlay(AnimationPose& pose, const AnimationEntity& entity,
                        const LocomotionController& controller,
                        const SkeletonData& skeleton) noexcept {
    if (entity.weapon_overlay == nullptr || !entity.weapon_overlay->valid()) return;
    const auto& overlay = *entity.weapon_overlay;
    const float height = controller.body().height;
    pose.weapon_aim_direction = overlay.aim_direction;
    const bool occupies_hands =
        (overlay.primary.owner != AnimationHandOwner::Free && overlay.primary.weight > 1.0e-3F) ||
        (overlay.support.owner != AnimationHandOwner::Free && overlay.support.weight > 1.0e-3F);
    if (occupies_hands && overlay.readiness > 1.0e-3F) {
        const foundation::Vec3 aim = normalized(overlay.aim_direction,
                                                {0.0F, 0.0F, 1.0F});
        const float yaw = std::atan2(aim.x, aim.z);
        const float pitch = std::atan2(aim.y, std::max(1.0e-4F,
                                                       std::sqrt(aim.x * aim.x +
                                                                 aim.z * aim.z)));
        const float readiness = std::clamp(overlay.readiness, 0.0F, 1.0F);
        const float prone = pose.active_state == AnimationState::PRONE ||
            pose.active_state == AnimationState::PRONE_MOVE ? 1.0F : 0.0F;
        constexpr std::array<std::pair<BoneId, float>, 2U> kAimBones{{
            {BoneId::SpineUpper, 0.14F}, {BoneId::Chest, 0.20F}}};
        for (const auto [bone, weight] : kAimBones) {
            const float contribution = weight * readiness * (1.0F - prone * 0.75F);
            rotateBone(pose.bones, bone, {1.0F, 0.0F, 0.0F}, -pitch * contribution);
            rotateBone(pose.bones, bone, {0.0F, 1.0F, 0.0F}, yaw * contribution);
        }
    }
    // Rig side zero is L; a weapon's primary hand is conventionally R and
    // its support hand is L. The neutral task names stay weapon-oriented.
    const AnimationHandOverlayTask* tasks[2]{&overlay.support, &overlay.primary};
    for (std::size_t index = 0U; index < 2U; ++index) {
        const auto& task = *tasks[index];
        if (task.owner == AnimationHandOwner::Free || task.weight <= 1.0e-3F) continue;
        foundation::Vec3 target = task.target;
        if (overlay.targets_are_world) target = sub(target, overlay.root_position);
        target = scale(target, 1.0F / height);
        pose.hand_targets[index] = {
            mix(pose.hand_targets[index].x, target.x, task.weight),
            mix(pose.hand_targets[index].y, target.y, task.weight),
            mix(pose.hand_targets[index].z, target.z, task.weight)};
        pose.hand_plant[index] = std::max(pose.hand_plant[index], task.weight);
        pose.hand_owners[index] = task.owner;
        const BoneId upper = index == 0U ? BoneId::UpperArmL : BoneId::UpperArmR;
        const BoneId lower = index == 0U ? BoneId::ForeArmL : BoneId::ForeArmR;
        const BoneId hand = index == 0U ? BoneId::HandL : BoneId::HandR;
        const auto pole = pose.elbow_targets[index];
        solveFullFrameChain(pose, skeleton, upper, lower, hand,
                            scale(pose.hand_targets[index], height),
                            scale(pole, height));
        pose.target_hand_curl = std::max(pose.target_hand_curl,
                                         task.curl * task.weight);
    }
    pose.weapon_readiness = overlay.readiness;
    pose.weapon_recoil = overlay.recoil;
    const float recoil = std::clamp(overlay.recoil, 0.0F, 1.0F) *
                         std::clamp(overlay.readiness, 0.0F, 1.0F);
    if (recoil > 0.0F) {
        rotateBone(pose.bones, BoneId::UpperArmL, {1.0F, 0.0F, 0.0F}, -recoil * 0.06F);
        rotateBone(pose.bones, BoneId::UpperArmR, {1.0F, 0.0F, 0.0F}, -recoil * 0.06F);
        rotateBone(pose.bones, BoneId::ForeArmL, {1.0F, 0.0F, 0.0F}, recoil * 0.04F);
        rotateBone(pose.bones, BoneId::ForeArmR, {1.0F, 0.0F, 0.0F}, recoil * 0.04F);
    }
}

void applySeatedPose(AnimationPose& pose,const LocomotionController& controller,
                     const SkeletonData& skeleton,const AppearanceMesh* surface=nullptr,
                     const std::optional<foundation::Vec3>& seat_anchor=std::nullopt,
                     foundation::Vec3 root_position={}) noexcept {
    const auto& body=controller.body();const float height=body.height;
    const float leg=body.anatomy_leg_length/height,hip_half=.052F*body.hip_width_scale;
    pose.bones[boneIndex(BoneId::Hips)].translation={0.0F,
        (.045F+leg*.635F)*height,-leg*.202F*height};
    if (seat_anchor.has_value() && finite(seat_anchor.value())) {
        pose.bones[boneIndex(BoneId::Hips)].translation = scale(
            sub(seat_anchor.value(), root_position), 1.0F / height);
        pose.bones[boneIndex(BoneId::Hips)].translation.y +=
            .040F * body.waist_depth_scale;
    }
    setEuler(pose.bones,BoneId::SpineLower,.12F);
    setEuler(pose.bones,BoneId::SpineUpper,.15F);
    setEuler(pose.bones,BoneId::Chest,.06F);
    setEuler(pose.bones,BoneId::Neck,-.15F);
    constexpr float arm_angle=.3839724354387525F;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        pose.foot_targets[index]={sign*(hip_half+.020F),.045F,leg*.073F};
        pose.knee_targets[index]={sign*(hip_half+.048F),.045F+leg*.32F,leg*.96F};
        if (seat_anchor.has_value() && finite(seat_anchor.value())) {
            const float foot_lift = std::max(0.0F,
                pose.bones[boneIndex(BoneId::Hips)].translation.y - leg * .70F - .045F);
            pose.foot_targets[index].x += pose.bones[boneIndex(BoneId::Hips)].translation.x;
            pose.foot_targets[index].y = .045F + foot_lift;
            pose.foot_targets[index].z +=
                pose.bones[boneIndex(BoneId::Hips)].translation.z + leg * .202F;
            pose.knee_targets[index].x += pose.bones[boneIndex(BoneId::Hips)].translation.x;
            pose.knee_targets[index].z +=
                pose.bones[boneIndex(BoneId::Hips)].translation.z + leg * .202F;
            pose.foot_plant[index] = foot_lift > .001F ? 0.0F : 1.0F;
            pose.foot_support[index] = pose.foot_plant[index];
        }
        const BoneId upper=index==0U?BoneId::UpperArmL:BoneId::UpperArmR;
        const BoneId fore=index==0U?BoneId::ForeArmL:BoneId::ForeArmR;
        setEuler(pose.bones,upper,-.34F,0.0F,sign*(-arm_angle+.09F));
        setEuler(pose.bones,fore,-1.04F);
    }
    pose.target_bones=pose.bones;
    applyBipedLegIK(pose,controller,skeleton,surface);
}

void applyPronePose(AnimationPose& pose,const LocomotionController& controller,
                    const SkeletonData& skeleton,const LocomotionState& state,
                    const AppearanceMesh* surface=nullptr) noexcept {
    (void)surface;
    constexpr float tau=6.28318530717958647692F;
    const auto& body=controller.body();const float height=body.height;
    const float leg=body.anatomy_leg_length/height,arm=body.arm_length/height;
    const float neck_y=skeleton.bones()[boneIndex(BoneId::Neck)].world_bind.translation.y/height;
    const float torso=neck_y-body.hip_y,hip_half=.052F*body.hip_width_scale;
    const float shoulder_half=.128F*body.shoulder_width_scale;
    const bool moving=state.family_moving;
    const float wave=moving?std::sin(tau*static_cast<float>(state.phase)):0.0F;
    pose.bones[boneIndex(BoneId::Hips)].translation={wave*.002F*height,
        (std::max(.070F*body.waist_depth_scale,.082F*body.leg_thickness_scale)+.012F)*height,
        -.008F*height};
    setEuler(pose.bones,BoneId::Hips,1.5707963267948966F,0.0F,wave*.021F);
    setEuler(pose.bones,BoneId::SpineLower,-.025F,0.0F,-wave*.012F);
    setEuler(pose.bones,BoneId::SpineUpper,-.055F,0.0F,-wave*.018F);
    setEuler(pose.bones,BoneId::Neck,-.58F);setEuler(pose.bones,BoneId::Head,-.18F);
    constexpr float arm_angle=.3839724354387525F;
    const double cycle=controller.proneCycleMeters()/height;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        const auto foot=PostureProfile::sampleLowContact(state.phase+static_cast<double>(index)*.5,
            .62F,cycle,leg*.026F);
        const auto hand=PostureProfile::sampleLowContact(state.phase+static_cast<double>(index)*.5+.5,
            .66F,cycle,arm*.030F);
        const float recovery=moving?16.0F*foot.swing*foot.swing*(1.0F-foot.swing)*(1.0F-foot.swing):0.0F;
        pose.foot_targets[index]={sign*(hip_half+.050F+recovery*.020F),.045F+(moving?foot.lift:0.0F),
            -leg*.685F+(moving?foot.z:0.0F)};
        pose.foot_plant[index]=moving?foot.plant:1.0F;
        pose.foot_support[index]=moving?foot.support:1.0F;
        pose.foot_relative[index]=1.0F;pose.ankle_pitch[index]=.055F;
        pose.ankle_yaw[index]=sign*.075F;
        pose.knee_targets[index]={sign*(hip_half+.235F),.055F,-leg*.30F};
        pose.hand_targets[index]={sign*(shoulder_half+.018F),.026F+(moving?hand.lift:0.0F),
            torso*.94F+arm*.43F+(moving?hand.z:0.0F)};
        pose.hand_lift[index]=moving?hand.lift:0.0F;
        pose.hand_plant[index]=moving?hand.plant:1.0F;
        pose.elbow_targets[index]={sign*(shoulder_half+.12F),.055F,torso*.94F};
        const BoneId upper=index==0U?BoneId::UpperArmL:BoneId::UpperArmR;
        const BoneId fore=index==0U?BoneId::ForeArmL:BoneId::ForeArmR;
        setEuler(pose.bones,upper,0.0F,0.0F,sign*(-arm_angle+.055F));
        setEuler(pose.bones,fore,-.065F);
    }
    pose.target_bones=pose.bones;
    // The authored crawl pose remains the no-surface baseline. A valid terrain
    // stage may apply the persistent hand/foot contact IK after this sampler.
}

struct ProneTerrainSample final {
    foundation::Vec3 target{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
};

[[nodiscard]] bool sampleProneTerrainTarget(const GroundSurfaceQuery& surface,
                                            foundation::Vec3 candidate,
                                            float offset,
                                            ProneTerrainSample& output) noexcept {
    GroundSample sample{};
    if (!surface.sample(surface.context, candidate, sample) ||
        !std::isfinite(sample.height) || !finite(sample.normal) || sample.normal.y <= 0.0F) {
        return false;
    }
    output.target = {candidate.x, sample.height + offset, candidate.z};
    output.normal = normalized(sample.normal, {0.0F, 1.0F, 0.0F});
    return finite(output.target) && finite(output.normal);
}

[[nodiscard]] bool applyProneGroundContact(AnimationPose& pose,
                                            const AnimationEntity& entity,
                                            const LocomotionController& controller,
                                            const LocomotionState& state) noexcept {
    if (!entity.ground_surface.valid()) {
        if (entity.ground_runtime != nullptr) entity.ground_runtime->reset();
        return false;
    }
    const float height = controller.body().height;
    std::array<ProneTerrainSample, 4U> samples{};
    for (std::size_t index = 0U; index < 2U; ++index) {
        const foundation::Vec3 foot = add(
            entity.root_position, scale(pose.foot_targets[index], height));
        const foundation::Vec3 hand = add(
            entity.root_position, scale(pose.hand_targets[index], height));
        if (!sampleProneTerrainTarget(entity.ground_surface, foot, height * 0.0015F,
                                      samples[index]) ||
            !sampleProneTerrainTarget(entity.ground_surface, hand, height * 0.0010F,
                                      samples[index + 2U])) {
            if (entity.ground_runtime != nullptr) entity.ground_runtime->reset();
            return false;
        }
    }

    const bool contacts_enabled = !state.treadmill && !state.turning &&
                                  std::abs(state.turn_rate) < 0.32F;
    constexpr float max_drift = 0.60F;
    for (std::size_t index = 0U; index < 2U; ++index) {
        foundation::Vec3 foot_target = samples[index].target;
        if (entity.ground_runtime != nullptr) {
            entity.ground_runtime->feet[index].normal = samples[index].normal;
            const float phase = static_cast<float>(state.phase) +
                                static_cast<float>(index) * 0.5F;
            const bool stance = pose.foot_support[index] > 0.35F &&
                                pose.foot_plant[index] > 0.35F;
            foot_target = entity.ground_runtime->resolve(index, foot_target,
                                                          pose.foot_plant[index],
                                                          contacts_enabled, max_drift,
                                                          phase, stance);
        }
        pose.foot_targets[index] = scale(sub(foot_target, entity.root_position),
                                          1.0F / height);
        pose.foot_goals[index] = pose.foot_targets[index];
        pose.foot_pitch[index] += std::atan2(samples[index].normal.z,
                                             std::max(1.0e-4F, samples[index].normal.y));
        pose.foot_yaw[index] += std::atan2(samples[index].normal.x,
                                           std::max(1.0e-4F, samples[index].normal.y)) *
                                (index == 0U ? 1.0F : -1.0F);

        foundation::Vec3 hand_target = samples[index + 2U].target;
        if (entity.ground_runtime != nullptr) {
            entity.ground_runtime->hands[index].normal = samples[index + 2U].normal;
            const float phase = static_cast<float>(state.phase) +
                                static_cast<float>(index) * 0.5F + 0.5F;
            const bool stance = pose.hand_plant[index] > 0.35F;
            hand_target = entity.ground_runtime->resolveHand(index, hand_target,
                                                              pose.hand_plant[index],
                                                              contacts_enabled, max_drift,
                                                              phase, stance);
        }
        const BoneId hand = index == 0U ? BoneId::HandL : BoneId::HandR;
        const auto hand_offset = proneHandSupportOffset(
            entity.surface, index == 0U ? "hand.L" : "hand.R",
            entity.skeleton->bones()[boneIndex(hand)].world_bind.translation,
            proneHandModelRotation(*entity.skeleton, index));
        hand_target.y += 0.002F - height * 0.001F - hand_offset.y +
                         pose.hand_lift[index] * height;
        pose.hand_targets[index] = scale(sub(hand_target, entity.root_position),
                                         1.0F / height);
    }

    return true;
}

void applyProneContactIK(AnimationPose& pose, const AnimationEntity& entity,
                         const LocomotionController& controller,
                         const SkeletonData& skeleton) noexcept {
    const float height = controller.body().height;
    for (std::size_t index = 0U; index < 2U; ++index) {
        const BoneId thigh = index == 0U ? BoneId::ThighL : BoneId::ThighR;
        const BoneId shin = index == 0U ? BoneId::ShinL : BoneId::ShinR;
        const BoneId foot = index == 0U ? BoneId::FootL : BoneId::FootR;
        solveFullFrameChain(pose, skeleton, thigh, shin, foot,
                            scale(pose.foot_targets[index], height),
                            scale(pose.knee_targets[index], height));
    }
    applyProneArmIK(pose, controller, skeleton);
    if (entity.surface == nullptr || !entity.ground_surface.valid()) return;
    for (std::size_t side = 0U; side < 2U; ++side) {
        const std::string_view tag_name = side == 0U ? "sleeve.L" : "sleeve.R";
        for (std::size_t pass = 0U; pass < 2U; ++pass) {
            const ModelPose posed = modelPose(pose.bones);
            float minimum_clearance = std::numeric_limits<float>::infinity();
            for (const auto& tag : entity.surface->tags) {
                if (tag.name != tag_name) continue;
                const std::size_t step = std::max<std::size_t>(
                    1U, tag.vertices.size() / 60U);
                for (std::size_t index = 0U; index < tag.vertices.size(); index += step) {
                    const std::uint32_t vertex_index = tag.vertices[index];
                    if (vertex_index >= entity.surface->vertices.size()) continue;
                    const auto point = add(entity.root_position, skinnedPoint(
                        entity.surface->vertices[vertex_index], skeleton, posed));
                    GroundSample ground{};
                    if (entity.ground_surface.sample(entity.ground_surface.context, point,
                                                     ground) && std::isfinite(ground.height)) {
                        minimum_clearance = std::min(minimum_clearance,
                                                     point.y - ground.height);
                    }
                }
            }
            if (!std::isfinite(minimum_clearance) || minimum_clearance >= 0.001F) break;
            const float correction = std::min(0.080F * height,
                                               3.2F * (0.002F - minimum_clearance));
            pose.elbow_targets[side].y += correction / height;
            applyProneArmIK(pose, controller, skeleton);
        }
    }
}

void applyProneBodyClearance(AnimationPose& pose, const AnimationEntity& entity,
                             const LocomotionController& controller,
                             const SkeletonData& skeleton) noexcept {
    if (entity.surface == nullptr || !entity.ground_surface.valid()) return;
    constexpr std::array<std::pair<std::string_view, std::size_t>, 7U> body_supports{{
        {"jacket", 38U}, {"head", 38U}, {"neck", 38U}, {"leg.L", 28U},
        {"sleeve.L", 60U}, {"leg.R", 28U}, {"sleeve.R", 60U}}};
    const ModelPose posed = modelPose(pose.bones);
    float minimum_clearance = std::numeric_limits<float>::infinity();
    for (const auto& [name, sample_count] : body_supports) {
        for (const auto& tag : entity.surface->tags) {
            if (tag.name != name) continue;
            const std::size_t step = std::max<std::size_t>(1U,
                                                           tag.vertices.size() / sample_count);
            for (std::size_t index = 0U; index < tag.vertices.size(); index += step) {
                const std::uint32_t vertex_index = tag.vertices[index];
                if (vertex_index >= entity.surface->vertices.size()) continue;
                const auto point = add(entity.root_position, skinnedPoint(
                    entity.surface->vertices[vertex_index], skeleton, posed));
                GroundSample ground{};
                if (entity.ground_surface.sample(entity.ground_surface.context, point, ground) &&
                    std::isfinite(ground.height)) {
                    minimum_clearance = std::min(minimum_clearance,
                                                  point.y - ground.height);
                }
            }
        }
    }
    if (entity.gear != nullptr) {
        const float fit_height = std::isfinite(entity.gear->fit.height) &&
                                 entity.gear->fit.height > 0.0F
            ? entity.gear->fit.height : 1.0F;
        const auto bones = skeleton.bones();
        for (const auto& piece : entity.gear->pieces) {
            const std::size_t bone = boneIndex(piece.bone);
            if (bone >= bones.size() || !finite(piece.center) || !finite(piece.dimensions) ||
                piece.dimensions.y <= 0.0F) continue;
            const auto center = scale(piece.center, fit_height);
            const auto half = scale(piece.dimensions, fit_height * 0.5F);
            for (const float x : {-half.x, half.x}) {
                for (const float z : {-half.z, half.z}) {
                    const auto local = poseBindPoint(
                        {center.x + x, center.y - half.y, center.z + z}, piece.bone,
                        skeleton, posed);
                    const auto point = add(entity.root_position, local);
                    GroundSample ground{};
                    if (entity.ground_surface.sample(entity.ground_surface.context, point,
                                                     ground) && std::isfinite(ground.height)) {
                        minimum_clearance = std::min(minimum_clearance,
                                                     point.y - ground.height);
                    }
                }
            }
        }
    }
    if (!std::isfinite(minimum_clearance)) return;
    const float body_lift = std::clamp(0.001F - minimum_clearance,
                                       0.0F, controller.body().height * 0.085F);
    if (!(body_lift > 0.0F)) return;
    pose.bones[boneIndex(BoneId::Hips)].translation.y += body_lift;
    applyProneContactIK(pose, entity, controller, skeleton);
}

[[nodiscard]] bool proneState(AnimationState state) noexcept;
[[nodiscard]] AnimationBodyPose sampleBody(const AnimationEntity& entity,
                                            AnimationTransitionStage stage,
                                            AnimationState state, float time) noexcept;
[[nodiscard]] foundation::Vec3 blendVec(foundation::Vec3 a, foundation::Vec3 b,
                                        float t) noexcept;
[[nodiscard]] AnimationBodyPose blendBody(const AnimationBodyPose& a,
                                           const AnimationBodyPose& b,
                                           float t) noexcept;

[[nodiscard]] float boneResponse(std::size_t index) noexcept {
    if(index==boneIndex(BoneId::Head)||index==boneIndex(BoneId::Neck))return 10.0F;
    if(index<=boneIndex(BoneId::Chest))return 14.0F;
    if(index==boneIndex(BoneId::ClavicleL)||index==boneIndex(BoneId::ClavicleR)||
       index==boneIndex(BoneId::UpperArmL)||index==boneIndex(BoneId::UpperArmR))return 17.0F;
    if(index==boneIndex(BoneId::ForeArmL)||index==boneIndex(BoneId::ForeArmR)||
       index==boneIndex(BoneId::HandL)||index==boneIndex(BoneId::HandR))return 15.0F;
    if(index==boneIndex(BoneId::ToesL)||index==boneIndex(BoneId::ToesR))return 22.0F;
    return 26.0F;
}

void dampBody(AnimationBodyPose& current, const AnimationBodyPose& target, float dt) noexcept {
    const float hips_alpha = 1.0F - std::exp(-18.0F * dt);
    const float scalar_alpha = 1.0F - std::exp(-22.0F * dt);
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
        const float alpha = 1.0F - std::exp(-boneResponse(bone) * dt);
        current.bones[bone].rotation = slerp(current.bones[bone].rotation,
                                             target.bones[bone].rotation, alpha);
        current.bones[bone].scale = blendVec(current.bones[bone].scale,
                                             target.bones[bone].scale, alpha);
        current.bones[bone].translation = bone == boneIndex(BoneId::Hips)
            ? blendVec(current.bones[bone].translation, target.bones[bone].translation, hips_alpha)
            : target.bones[bone].translation;
    }
    current.foot_targets = target.foot_targets; current.knee_targets = target.knee_targets;
    current.hand_targets = target.hand_targets; current.elbow_targets = target.elbow_targets;
    for (std::size_t side = 0U; side < 2U; ++side) {
        current.foot_plant[side] = target.foot_plant[side];
        current.foot_support[side] = target.foot_support[side];
        current.foot_pitch[side] = target.foot_pitch[side];
        current.toe_pitch[side] = target.toe_pitch[side];
        current.foot_yaw[side] = target.foot_yaw[side];
        current.hand_plant[side] = target.hand_plant[side];
        current.hand_lift[side] = target.hand_lift[side];
        current.foot_relative[side] = target.foot_relative[side];
        current.ankle_pitch[side] = target.ankle_pitch[side];
        current.ankle_yaw[side] = target.ankle_yaw[side];
    }
    current.hand_curl = mix(current.hand_curl, target.hand_curl, scalar_alpha);
    current.prone_weight = mix(current.prone_weight, target.prone_weight, scalar_alpha);
    current.hand_ik_weight = mix(current.hand_ik_weight, target.hand_ik_weight, scalar_alpha);
    current.gait_weight = mix(current.gait_weight, target.gait_weight, scalar_alpha);
    current.posture = target.posture;
}

void buildSchedule(AnimationTransitionRuntime& runtime, AnimationState from,
                   AnimationState target) noexcept {
    runtime.stage_count = 0U; runtime.stage_index = 0U; runtime.stage_elapsed = 0.0F;
    runtime.total_elapsed = 0.0F; runtime.total_duration = 0.0F;
    const auto add = [&](AnimationTransitionStage stage, AnimationState state, float duration) {
        if (!(duration > 0.0F)) return;
        if (runtime.stage_count >= runtime.schedule.size()) return;
        runtime.schedule[runtime.stage_count++] = {stage, state, duration};
        runtime.total_duration += duration;
    };
    const bool from_prone = proneState(from), to_prone = proneState(target);
    const auto& p = runtime.profile;
    if (from_prone != to_prone) {
        if (to_prone) {
            add(AnimationTransitionStage::Crouch, AnimationState::CROUCH, p.prone_crouch_seconds);
            add(AnimationTransitionStage::Support, AnimationState::CROUCH, p.prone_support_seconds);
            add(AnimationTransitionStage::Target, target, p.prone_seconds);
        } else {
            add(AnimationTransitionStage::Support, AnimationState::CROUCH, p.prone_exit_support_seconds);
            add(AnimationTransitionStage::Crouch, AnimationState::CROUCH, p.prone_exit_crouch_seconds);
            add(AnimationTransitionStage::Target, target, p.prone_exit_seconds);
        }
    } else if (from == AnimationState::SITTING || target == AnimationState::SITTING) {
        add(AnimationTransitionStage::Target, target, p.sitting_seconds);
    } else if ((from == AnimationState::PRONE && target == AnimationState::PRONE_MOVE) ||
               (from == AnimationState::PRONE_MOVE && target == AnimationState::PRONE)) {
        add(AnimationTransitionStage::Target, target, p.locomotion_seconds);
    } else if (from == AnimationState::CROUCH_WALK || target == AnimationState::CROUCH_WALK) {
        add(AnimationTransitionStage::Target, target, p.crouch_walk_seconds);
    } else if (from == AnimationState::CROUCH || target == AnimationState::CROUCH) {
        add(AnimationTransitionStage::Target, target, p.crouch_seconds);
    } else {
        add(AnimationTransitionStage::Target, target, p.locomotion_seconds);
    }
}

[[nodiscard]] AnimationBodyPose sampleSettledBody(const AnimationEntity& entity,
                                                  AnimationState state,
                                                  float time) noexcept {
    return sampleBody(entity, AnimationTransitionStage::Target, state, time);
}

void updateRuntime(AnimationEntity& entity, float time, float dt) noexcept {
    auto& runtime = *entity.transition_runtime;
    auto& state = *entity.locomotion_state;
    const auto sample = [&](AnimationTransitionStage stage, AnimationState value) {
        return sampleBody(entity, stage, value, time);
    };
    if (!runtime.initialized || state.animation_snap_requested) {
        runtime.reset(); runtime.initialized = true; runtime.profile = state.transition_request_profile;
        runtime.requested_state = state.requested_state; runtime.settled_state = state.requested_state;
        runtime.handled_request_revision = state.animation_request_revision;
        // setState(immediate) in the reference samples the current pose before
        // the first update, whose motion speed has not yet been supplied.
        LocomotionState initial_state = state;
        initial_state.phase = 0.0;
        initial_state.actual_speed_mps = 0.0F;
        initial_state.amplitude = 0.0F;
        initial_state.settling = false;
        AnimationEntity initial_entity = entity;
        initial_entity.locomotion_state = &initial_state;
        runtime.current = sampleBody(initial_entity, AnimationTransitionStage::Target,
                                     state.requested_state, std::max(0.0F, time - dt));
        runtime.stage_from = runtime.stage_target = runtime.current;
        state.active_state = state.requested_state; state.transition_active = false;
        state.transition_stage = AnimationTransitionStage::None;
        state.transition_progress = state.transition_stage_progress = 1.0F;
        state.animation_snap_requested = false;
        dampBody(runtime.current, sampleSettledBody(entity, state.requested_state, time), dt);
        if (state.family == LocomotionFamily::Biped) {
            constrainBipedHips(runtime.current.bones, runtime.current.foot_targets,
                               entity.locomotion->body());
        }
        return;
    }
    if (runtime.handled_request_revision != state.animation_request_revision) {
        const AnimationState previous_request = runtime.requested_state;
        runtime.handled_request_revision = state.animation_request_revision;
        runtime.profile = state.transition_request_profile;
        runtime.requested_state = state.requested_state;
        runtime.stage_from = runtime.current;
        buildSchedule(runtime, previous_request, runtime.requested_state);
        runtime.active = runtime.stage_count != 0U;
        if (runtime.active) {
            runtime.stage_target = sample(runtime.schedule[0].stage, runtime.schedule[0].state);
        } else {
            // A zero-duration schedule is an atomic snap. It must not leave a
            // one-frame intermediate stage or apply damping repeatedly.
            runtime.stage_target = sampleSettledBody(entity, runtime.requested_state, time);
            runtime.current = runtime.stage_from = runtime.stage_target;
            runtime.settled_state = runtime.requested_state;
        }
    }

    float remaining = dt;
    while (runtime.active) {
        const auto& step = runtime.schedule[runtime.stage_index];
        if (step.duration <= 0.0F) {
            runtime.stage_from = runtime.stage_target;
            runtime.stage_elapsed = 0.0F;
            if (++runtime.stage_index >= runtime.stage_count) {
                runtime.active = false;
                runtime.settled_state = runtime.requested_state;
                break;
            }
            runtime.stage_target = sample(runtime.schedule[runtime.stage_index].stage,
                                          runtime.schedule[runtime.stage_index].state);
            continue;
        }

        if (remaining <= 0.0F) break;
        const float advance = std::min(remaining, step.duration - runtime.stage_elapsed);
        runtime.stage_elapsed += advance;
        runtime.total_elapsed += advance;
        remaining -= advance;
        if (runtime.stage_elapsed < step.duration) break;

        runtime.stage_from = runtime.stage_target;
        runtime.stage_elapsed = 0.0F;
        if (++runtime.stage_index >= runtime.stage_count) {
            runtime.active = false;
            runtime.settled_state = runtime.requested_state;
            break;
        }
        const auto& next = runtime.schedule[runtime.stage_index];
        runtime.stage_target = sample(next.stage, next.state);
    }

    if (!runtime.active) {
        state.active_state = runtime.settled_state;
        dampBody(runtime.current, sampleSettledBody(entity, runtime.settled_state, time), dt);
    } else {
        const auto& step = runtime.schedule[runtime.stage_index];
        const float local = step.duration > 0.0F
            ? std::clamp(runtime.stage_elapsed / step.duration, 0.0F, 1.0F) : 1.0F;
        dampBody(runtime.current, blendBody(runtime.stage_from, runtime.stage_target,
                                            smooth5(local)), dt);
    }
    if (state.family == LocomotionFamily::Biped) {
        constrainBipedHips(runtime.current.bones, runtime.current.foot_targets,
                           entity.locomotion->body());
    }
    state.transition_active = runtime.active;
    state.transition_stage = runtime.active ? runtime.schedule[runtime.stage_index].stage
                                            : AnimationTransitionStage::None;
    state.transition_progress = runtime.active && runtime.total_duration > 0.0F
        ? std::clamp(runtime.total_elapsed / runtime.total_duration, 0.0F, 1.0F) : 1.0F;
    state.transition_stage_progress = runtime.active
        ? (runtime.schedule[runtime.stage_index].duration > 0.0F
            ? std::clamp(runtime.stage_elapsed / runtime.schedule[runtime.stage_index].duration,
                         0.0F, 1.0F) : 1.0F) : 1.0F;
}

void applyFacePose(std::array<RigTransform, kRigBoneCount>& bones,
                   const FaceOutput& face) noexcept {
    rotateBone(bones, BoneId::Head, {0.0F, 1.0F, 0.0F}, face.head_yaw);
    rotateBone(bones, BoneId::Head, {1.0F, 0.0F, 0.0F}, face.head_pitch);
    rotateBone(bones, BoneId::Jaw, {1.0F, 0.0F, 0.0F}, face.jaw_rotation);
    rotateBone(bones, BoneId::EyeL, {0.0F, 1.0F, 0.0F}, face.eye_yaw);
    rotateBone(bones, BoneId::EyeR, {0.0F, 1.0F, 0.0F}, face.eye_yaw);
    rotateBone(bones, BoneId::EyeL, {1.0F, 0.0F, 0.0F}, face.eye_pitch);
    rotateBone(bones, BoneId::EyeR, {1.0F, 0.0F, 0.0F}, face.eye_pitch);
}

[[nodiscard]] foundation::Error invalidEntityError() noexcept {
    return {foundation::ErrorCode::InvalidArgument, "invalid animation entity contract"};
}

} // namespace

bool AnimationEntity::valid() const noexcept {
    return skeleton != nullptr && skeleton->valid() && locomotion != nullptr &&
           locomotion_state != nullptr && locomotion_state->valid() &&
           transition_runtime != nullptr && finite(root_position) &&
           (!look_target.has_value() || finite(look_target.value())) &&
           (!seat_anchor.has_value() || finite(seat_anchor.value())) &&
           (weapon_overlay == nullptr || weapon_overlay->valid()) && lod.spec().valid();
}

namespace {

[[nodiscard]] bool proneState(AnimationState state) noexcept {
    return state == AnimationState::PRONE || state == AnimationState::PRONE_MOVE;
}

[[nodiscard]] LocomotionState samplingState(const LocomotionState& source,
                                             AnimationState state) noexcept {
    LocomotionState result = source;
    result.requested_state = state;
    result.active_state = state;
    result.transition_active = false;
    switch (state) {
    case AnimationState::IDLE:
        result.family = LocomotionFamily::Biped; result.actual_crouch = 0.0F;
        result.actual_speed_mps = 0.0F; result.amplitude = 0.0F; break;
    case AnimationState::WALK:
    case AnimationState::RUN:
        result.family = LocomotionFamily::Biped; result.actual_crouch = 0.0F; break;
    case AnimationState::CROUCH:
        result.family = LocomotionFamily::Biped; result.actual_crouch = 0.60F;
        result.actual_speed_mps = 0.0F; result.amplitude = 0.0F; break;
    case AnimationState::CROUCH_WALK:
        result.family = LocomotionFamily::Biped; result.actual_crouch = 0.60F; break;
    case AnimationState::SITTING:
        result.family = LocomotionFamily::Seated; result.actual_speed_mps = 0.0F; break;
    case AnimationState::PRONE:
        result.family = LocomotionFamily::Prone; result.family_moving = false;
        result.actual_speed_mps = 0.0F; break;
    case AnimationState::PRONE_MOVE:
        result.family = LocomotionFamily::Prone; result.family_moving = true; break;
    case AnimationState::REST:
        result.family = LocomotionFamily::Rest; result.actual_speed_mps = 0.0F; break;
    }
    return result;
}

[[nodiscard]] foundation::Vec3 transformPoint(const RigTransform& transform,
                                               foundation::Vec3 point) noexcept {
    point = {point.x * transform.scale.x, point.y * transform.scale.y,
             point.z * transform.scale.z};
    return add(transform.translation, rotate(point, transform.rotation));
}

[[nodiscard]] foundation::Vec3 poseBindPoint(foundation::Vec3 point, BoneId bone,
                                              const SkeletonData& skeleton,
                                              const ModelPose& pose) noexcept {
    const std::size_t index = boneIndex(bone);
    const auto bones = skeleton.bones();
    if (index >= bones.size()) return point;
    const foundation::Vec3 local = transformPoint(bones[index].inverse_bind, point);
    return add(pose.positions[index], rotate(local, pose.rotations[index]));
}

[[nodiscard]] foundation::Vec3 skinnedPoint(const AppearanceVertex& vertex,
                                            const SkeletonData& skeleton,
                                            const ModelPose& pose) noexcept {
    const auto bones = skeleton.bones();
    foundation::Vec3 result{};
    float total_weight = 0.0F;
    for (std::size_t influence = 0U;
         influence < vertex.influence_count && influence < vertex.influences.size();
         ++influence) {
        const auto& skin = vertex.influences[influence];
        if (skin.bone_index >= bones.size() || !(skin.weight > 0.0F)) continue;
        const std::size_t index = skin.bone_index;
        const foundation::Vec3 local = transformPoint(bones[index].inverse_bind,
                                                       vertex.position);
        result = add(result, scale(add(pose.positions[index],
                                       rotate(local, pose.rotations[index])), skin.weight));
        total_weight += skin.weight;
    }
    return total_weight > 1.0e-6F ? scale(result, 1.0F / total_weight) : vertex.position;
}

void applyPostLookHeadClearance(AnimationPose& pose, const AnimationEntity& entity,
                                const LocomotionController& controller,
                                const SkeletonData& skeleton) noexcept {
    if (!entity.ground_surface.valid()) return;
    const float height = controller.body().height;
    if (!(height > 0.0F) || !std::isfinite(height)) return;

    const ModelPose posed = modelPose(pose.bones);
    float required_lift = 0.0F;
    const auto consider = [&](foundation::Vec3 local_point) noexcept {
        const foundation::Vec3 world_point = add(entity.root_position, local_point);
        GroundSample sample{};
        if (!entity.ground_surface.sample(entity.ground_surface.context, world_point,
                                          sample) || !std::isfinite(sample.height) ||
            !finite(sample.normal) || sample.normal.y <= 0.0F) {
            return;
        }
        required_lift = std::max(required_lift,
                                 sample.height + height * 0.0005F - world_point.y);
    };

    if (entity.surface != nullptr) {
        const std::uint16_t head_index = static_cast<std::uint16_t>(boneIndex(BoneId::Head));
        const std::uint16_t neck_index = static_cast<std::uint16_t>(boneIndex(BoneId::Neck));
        for (const auto& vertex : entity.surface->vertices) {
            float head_weight = 0.0F;
            for (std::size_t influence = 0U;
                 influence < vertex.influence_count && influence < vertex.influences.size();
                 ++influence) {
                if (vertex.influences[influence].bone_index == head_index ||
                    vertex.influences[influence].bone_index == neck_index) {
                    head_weight += std::max(0.0F, vertex.influences[influence].weight);
                }
            }
            if (head_weight >= 0.35F)
                consider(skinnedPoint(vertex, skeleton, posed));
        }
    }

    if (entity.gear != nullptr) {
        const float fit_height = std::isfinite(entity.gear->fit.height) &&
                                 entity.gear->fit.height > 0.0F
            ? entity.gear->fit.height : 1.0F;
        for (const auto& piece : entity.gear->pieces) {
            if (piece.slot != EquipmentSlot::Head || !finite(piece.center) ||
                !finite(piece.dimensions) || piece.dimensions.x <= 0.0F ||
                piece.dimensions.y <= 0.0F || piece.dimensions.z <= 0.0F) {
                continue;
            }
            const foundation::Vec3 center = scale(piece.center, fit_height);
            const foundation::Vec3 half = scale(piece.dimensions, fit_height * 0.5F);
            for (const float x : {-half.x, half.x}) {
                for (const float y : {-half.y, half.y}) {
                    for (const float z : {-half.z, half.z}) {
                        consider(poseBindPoint({center.x + x, center.y + y, center.z + z},
                                               piece.bone, skeleton, posed));
                    }
                }
            }
        }
    }

    const float lift = std::clamp(required_lift, 0.0F, height * 0.030F);
    if (!(lift > 0.0F)) return;
    pose.bones[boneIndex(BoneId::Hips)].translation.y += lift;
    if (proneState(pose.active_state))
        applyProneContactIK(pose, entity, controller, skeleton);
    else if (pose.active_state == AnimationState::IDLE ||
             pose.active_state == AnimationState::WALK ||
             pose.active_state == AnimationState::RUN ||
             pose.active_state == AnimationState::CROUCH ||
             pose.active_state == AnimationState::CROUCH_WALK ||
             pose.active_state == AnimationState::SITTING)
        applyBipedLegIK(pose, controller, skeleton, entity.surface);
}

void applySupportPose(AnimationPose& pose, const LocomotionController& controller,
                      const SkeletonData& skeleton) noexcept {
    const auto& body = controller.body();
    const float height = body.height;
    const float leg = body.anatomy_leg_length / height;
    const float shoulder = 0.128F * body.shoulder_width_scale;
    const float hip = 0.052F * body.hip_width_scale;
    pose.bones[boneIndex(BoneId::Hips)].translation =
        {0.0F, (0.12F + leg * 0.34F) * height, -leg * 0.18F * height};
    setEuler(pose.bones, BoneId::Hips, 0.93F);
    setEuler(pose.bones, BoneId::SpineLower, -0.18F);
    setEuler(pose.bones, BoneId::SpineUpper, -0.22F);
    setEuler(pose.bones, BoneId::Chest, -0.08F);
    setEuler(pose.bones, BoneId::Neck, -0.28F);
    setEuler(pose.bones, BoneId::Head, -0.10F);
    for (std::size_t side = 0U; side < 2U; ++side) {
        const float sign = side == 0U ? 1.0F : -1.0F;
        pose.foot_targets[side] = {sign * (hip + 0.08F), 0.045F, -leg * 0.42F};
        pose.knee_targets[side] = {sign * (hip + 0.22F), 0.10F, -leg * 0.08F};
        pose.hand_targets[side] = {sign * (shoulder + 0.08F), 0.018F, leg * 0.34F};
        pose.elbow_targets[side] = {sign * (shoulder + 0.18F), 0.075F, leg * 0.13F};
        pose.foot_plant[side] = 1.0F;
        pose.foot_support[side] = 1.0F;
        pose.hand_plant[side] = 1.0F;
        pose.foot_relative[side] = 1.0F;
    }
    applyBipedLegIK(pose, controller, skeleton);
    applyProneArmIK(pose, controller, skeleton);
}

[[nodiscard]] AnimationBodyPose bodyFromPose(const AnimationPose& pose) noexcept {
    AnimationBodyPose body{};
    body.posture = pose.posture; body.bones = pose.bones;
    body.foot_targets = pose.foot_targets; body.knee_targets = pose.knee_targets;
    body.hand_targets = pose.hand_targets; body.elbow_targets = pose.elbow_targets;
    body.foot_plant = pose.foot_plant; body.foot_support = pose.foot_support;
    body.foot_pitch = pose.foot_pitch; body.toe_pitch = pose.toe_pitch;
    body.foot_yaw = pose.foot_yaw; body.hand_plant = pose.hand_plant;
    body.hand_lift = pose.hand_lift; body.foot_relative = pose.foot_relative;
    body.ankle_pitch = pose.ankle_pitch; body.ankle_yaw = pose.ankle_yaw;
    body.hand_curl = pose.target_hand_curl;
    return body;
}

void copyBody(AnimationPose& pose, const AnimationBodyPose& body) noexcept {
    pose.posture = body.posture; pose.bones = body.bones;
    pose.foot_targets = body.foot_targets; pose.knee_targets = body.knee_targets;
    pose.hand_targets = body.hand_targets; pose.elbow_targets = body.elbow_targets;
    pose.foot_plant = body.foot_plant; pose.foot_support = body.foot_support;
    pose.foot_pitch = body.foot_pitch; pose.toe_pitch = body.toe_pitch;
    pose.foot_yaw = body.foot_yaw; pose.hand_plant = body.hand_plant;
    pose.hand_lift = body.hand_lift; pose.foot_relative = body.foot_relative;
    pose.ankle_pitch = body.ankle_pitch; pose.ankle_yaw = body.ankle_yaw;
    pose.target_hand_curl = body.hand_curl; pose.face.hands_relax = body.hand_curl;
    pose.foot_goals = body.foot_targets;
}

[[nodiscard]] AnimationBodyPose sampleBody(const AnimationEntity& entity,
                                            AnimationTransitionStage stage,
                                            AnimationState state, float time) noexcept {
    AnimationPose pose{};
    const auto sampled = samplingState(*entity.locomotion_state, state);
    const auto bind = entity.skeleton->bones();
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone)
        pose.bones[bone] = bind[bone].local_bind;
    pose.posture = entity.locomotion->posture(sampled);
    if (stage == AnimationTransitionStage::Support) {
        applySupportPose(pose, *entity.locomotion, *entity.skeleton);
    } else if (sampled.family == LocomotionFamily::Biped) {
        sampleBipedTargets(pose, *entity.locomotion, sampled);
        applyReferenceBipedPose(pose, *entity.locomotion, sampled, time);
        pose.target_hand_curl = pose.face.hands_relax;
    } else if (sampled.family == LocomotionFamily::Seated) {
        applySeatedPose(pose, *entity.locomotion, *entity.skeleton, entity.surface,
                        entity.seat_anchor, entity.root_position);
    } else if (sampled.family == LocomotionFamily::Prone) {
        applyPronePose(pose, *entity.locomotion, *entity.skeleton, sampled, entity.surface);
    }
    AnimationBodyPose result = bodyFromPose(pose);
    result.prone_weight = stage == AnimationTransitionStage::Support ? 0.63F :
        (sampled.family == LocomotionFamily::Prone ? 1.0F : 0.0F);
    result.hand_ik_weight = stage == AnimationTransitionStage::Support ? 1.0F :
        (sampled.family == LocomotionFamily::Prone ? 1.0F : 0.0F);
    result.gait_weight = sampled.actual_speed_mps > 0.008F || sampled.settling
        ? 1.0F : 0.0F;
    return result;
}

[[nodiscard]] foundation::Vec3 blendVec(foundation::Vec3 a, foundation::Vec3 b,
                                        float t) noexcept {
    return {mix(a.x, b.x, t), mix(a.y, b.y, t), mix(a.z, b.z, t)};
}

[[nodiscard]] AnimationBodyPose blendBody(const AnimationBodyPose& a,
                                           const AnimationBodyPose& b,
                                           float t) noexcept {
    AnimationBodyPose result = b;
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
        result.bones[bone].translation = blendVec(a.bones[bone].translation,
                                                  b.bones[bone].translation, t);
        result.bones[bone].rotation = slerp(a.bones[bone].rotation, b.bones[bone].rotation, t);
        result.bones[bone].scale = blendVec(a.bones[bone].scale, b.bones[bone].scale, t);
    }
    for (std::size_t side = 0U; side < 2U; ++side) {
        result.foot_targets[side] = blendVec(a.foot_targets[side], b.foot_targets[side], t);
        result.knee_targets[side] = blendVec(a.knee_targets[side], b.knee_targets[side], t);
        result.hand_targets[side] = blendVec(a.hand_targets[side], b.hand_targets[side], t);
        result.elbow_targets[side] = blendVec(a.elbow_targets[side], b.elbow_targets[side], t);
#define BLEND_CHANNEL(name) result.name[side] = mix(a.name[side], b.name[side], t)
        BLEND_CHANNEL(foot_plant); BLEND_CHANNEL(foot_support); BLEND_CHANNEL(foot_pitch);
        BLEND_CHANNEL(toe_pitch); BLEND_CHANNEL(foot_yaw); BLEND_CHANNEL(hand_plant);
        BLEND_CHANNEL(hand_lift); BLEND_CHANNEL(foot_relative); BLEND_CHANNEL(ankle_pitch);
        BLEND_CHANNEL(ankle_yaw);
#undef BLEND_CHANNEL
    }
    result.hand_curl = mix(a.hand_curl, b.hand_curl, t);
    result.prone_weight = mix(a.prone_weight, b.prone_weight, t);
    result.hand_ik_weight = mix(a.hand_ik_weight, b.hand_ik_weight, t);
    result.gait_weight = mix(a.gait_weight, b.gait_weight, t);
    return result;
}

} // namespace

bool AnimationPose::valid() const noexcept {
    if (!finite(root_position) || !finite(locomotion_phase) || locomotion_phase < 0.0F ||
        locomotion_phase >= 1.0F || !face.valid()) {
        return false;
    }
    for(std::size_t index=0;index<2U;++index){
        if(!finite(foot_targets[index])||!finite(foot_goals[index])||
           !finite(knee_targets[index])||
           !finite(hand_targets[index])||!finite(elbow_targets[index])||
           !finite(foot_plant[index])||!finite(foot_support[index])||
           !finite(foot_pitch[index])||!finite(toe_pitch[index])||!finite(foot_yaw[index])||
           !finite(hand_plant[index])||!finite(hand_lift[index])||!finite(foot_relative[index])||
           !finite(ankle_pitch[index])||!finite(ankle_yaw[index]))return false;
    }
    const float aim_length = std::sqrt(dot(weapon_aim_direction, weapon_aim_direction));
    if(!finite(target_hand_curl) || !finite(weapon_readiness) || weapon_readiness < 0.0F ||
       weapon_readiness > 1.0F || !finite(weapon_recoil) || weapon_recoil < 0.0F ||
       !finite(weapon_aim_direction) || !finite(aim_length) || aim_length <= 1.0e-6F ||
       !finite(transition_progress) || transition_progress < 0.0F || transition_progress > 1.0F)
        return false;
    if (!finite(transition_stage_progress) || transition_stage_progress < 0.0F ||
        transition_stage_progress > 1.0F) return false;
    const auto valid_bones=[](const auto& transforms) noexcept {
        for(const RigTransform& bone:transforms)
            if(!finite(bone.translation)||!finite(bone.scale)||!finite(bone.rotation.x)||
               !finite(bone.rotation.y)||!finite(bone.rotation.z)||!finite(bone.rotation.w))return false;
        return true;
    };
    if(!valid_bones(target_bones)||!valid_bones(damped_bones)||!valid_bones(bones))return false;
    return true;
}

foundation::Result<AnimationSystem, foundation::Error> AnimationSystem::create(
    std::size_t chunk_size) {
    if (chunk_size == 0U || chunk_size > 1U << 20U) {
        return foundation::Result<AnimationSystem, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid animation chunk size"});
    }
    return foundation::Result<AnimationSystem, foundation::Error>::success(
        AnimationSystem(chunk_size));
}

AnimationEvaluationHandle AnimationSystem::evaluateAsync(
    AnimationWorkSet work,
    jobs::JobSystem& jobs,
    jobs::CancelToken cancellation,
    PresentationBudget budget) {
    if (work.entities.size() > budget.max_entities) {
        work.entities.resize(budget.max_entities);
    }
    auto error = std::make_shared<std::optional<foundation::Error>>();
    auto group = std::make_shared<jobs::JobGroup>(jobs);
    jobs::JobOptions options;
    // Animation::evaluate retains its compatibility synchronous wrapper, but
    // the asynchronous presentation entry point is scheduled on the owner
    // lane. Its chunk barrier therefore cannot consume a worker while waiting
    // for child work; the same central scheduler still executes the chunks.
    options.lane = jobs::ExecutionLane::Main;
    options.work_class = jobs::WorkClass::Presentation;
    options.cancellation = cancellation;
    jobs::JobSystem* scheduler = &jobs;
    (void)group->submit(
        [this, work = std::move(work), error, group, scheduler, cancellation](
            jobs::JobContext&) mutable {
            (void)group;
            // The evaluator's chunk work is submitted to the same central
            // scheduler. JobGroup::wait is cooperative on a worker, so this
            // preserves parallel chunk evaluation without creating a private
            // pool or blocking an available worker thread.
            const auto result = evaluate(std::move(work), scheduler, cancellation);
            if (!result) {
                *error = result.error();
            }
        },
        options);
    return AnimationEvaluationHandle(group->completion(), std::move(error));
}

foundation::Result<void, foundation::Error> AnimationSystem::evaluate(
    std::span<AnimationEntity> entities,
    std::uint64_t simulation_tick,
    float fixed_dt_seconds,
    jobs::JobSystem* jobs,
    jobs::CancelToken cancellation) {
    if (!finite(fixed_dt_seconds) || fixed_dt_seconds <= 0.0F || fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid animation fixed interval"});
    }
    for (const AnimationEntity& entity : entities) {
        if (!entity.valid()) {
            return foundation::Result<void, foundation::Error>::failure(invalidEntityError());
        }
    }
    std::unordered_set<const AnimationTransitionRuntime*> runtimes;
    runtimes.reserve(entities.size());
    for (const auto& entity : entities) {
        if (!runtimes.insert(entity.transition_runtime).second)
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "animation entities must not share transition runtime"});
    }

    const auto evaluation_started = std::chrono::steady_clock::now();
    previous_ = std::move(current_);
    current_ = {};
    current_.simulation_tick = simulation_tick;
    current_.previous_simulation_tick = previous_.simulation_tick;
    current_.pose_revision = previous_.pose_revision + 1U;
    current_.poses = previous_.poses;
    current_.poses.resize(entities.size());
    for (std::size_t index = 0U; index < entities.size(); ++index) {
        if (current_.poses[index].semantic_id != entities[index].semantic_id) {
            current_.poses[index] = {};
            current_.poses[index].semantic_id = entities[index].semantic_id;
        }
    }

    struct WorkItem final {
        bool due{false};
        std::uint64_t elapsed_ticks{1U};
    };
    std::vector<WorkItem> work(entities.size());
    std::uint32_t due_count = 0U;
    for (std::size_t index = 0U; index < entities.size(); ++index) {
        work[index].due = entities[index].lod.due(simulation_tick);
        work[index].elapsed_ticks = entities[index].lod.elapsedTicks(simulation_tick);
        due_count += work[index].due ? 1U : 0U;
    }

    const std::size_t chunk_count =
        entities.empty() ? 0U : entities.size() / chunk_size_ +
            (entities.size() % chunk_size_ != 0U ? 1U : 0U);
    std::atomic<bool> failed{false};
    std::mutex error_mutex;
    std::size_t first_error_index = std::numeric_limits<std::size_t>::max();
    foundation::Error first_error{};
    const auto recordError = [&](std::size_t index, foundation::Error error) noexcept {
        std::lock_guard lock(error_mutex);
        failed.store(true, std::memory_order_release);
        if (index < first_error_index) {
            first_error_index = index;
            first_error = std::move(error);
        }
    };

    const auto processRange = [&](std::size_t begin, std::size_t end) noexcept {
        for (std::size_t index = begin; index < end; ++index) {
            AnimationEntity& entity = entities[index];
            const float animation_time =
                static_cast<float>(simulation_tick + 1U) * fixed_dt_seconds;
            const float transition_dt = fixed_dt_seconds * static_cast<float>(
                std::min<std::uint64_t>(work[index].elapsed_ticks, 1024U));
            updateRuntime(entity, animation_time, transition_dt);
            if (!work[index].due) continue;
            AnimationPose& pose = current_.poses[index];
            const PostureSample posture = entity.locomotion->posture(*entity.locomotion_state);
            pose.semantic_id = entity.semantic_id;
            pose.root_position = entity.root_position;
            pose.posture = posture;
            pose.locomotion_phase = entity.locomotion_state->phase;
            pose.requested_state = entity.locomotion_state->requested_state;
            pose.active_state = entity.locomotion_state->active_state;
            pose.transition_stage = entity.locomotion_state->transition_stage;
            pose.transition_progress = entity.locomotion_state->transition_progress;
            pose.transition_stage_progress = entity.locomotion_state->transition_stage_progress;
            pose.hand_owners = {AnimationHandOwner::Free, AnimationHandOwner::Free};
            pose.weapon_readiness = 0.0F;
            pose.weapon_recoil = 0.0F;
            pose.weapon_aim_direction = {0.0F, 0.0F, 1.0F};
            pose.revision = current_.pose_revision;
            pose.evaluated = true;
            const auto bones = entity.skeleton->bones();
            if (bones.size() != kRigBoneCount) {
                recordError(index, {foundation::ErrorCode::InvalidState,
                                    "animation skeleton does not match rig schema"});
                continue;
            }
            copyBody(pose, entity.transition_runtime->current);
            pose.damped_bones = pose.bones;
            AnimationBodyPose desired = sampleSettledBody(
                entity, entity.transition_runtime->settled_state, animation_time);
            if (entity.transition_runtime->active) {
                const auto& step = entity.transition_runtime->schedule[
                    entity.transition_runtime->stage_index];
                const float local = step.duration > 0.0F
                    ? entity.transition_runtime->stage_elapsed / step.duration : 1.0F;
                desired = blendBody(entity.transition_runtime->stage_from,
                                    entity.transition_runtime->stage_target, smooth5(local));
            }
            pose.target_bones = desired.bones;
            pose.target_hand_curl = desired.hand_curl;
            if (pose.transition_stage != AnimationTransitionStage::Support) {
                const bool ground_applied = applyGroundContact(
                    pose, entity, *entity.locomotion, *entity.locomotion_state);
                if (!proneState(pose.active_state)) {
                    applyBipedLegIK(pose, *entity.locomotion, *entity.skeleton, entity.surface);
                    applyBipedBootClearanceIK(pose, entity, *entity.locomotion);
                } else if (ground_applied) {
                    applyProneContactIK(pose, entity, *entity.locomotion, *entity.skeleton);
                    applyProneBodyClearance(pose, entity, *entity.locomotion,
                                            *entity.skeleton);
                }
            }
            applyWeaponOverlay(pose, entity, *entity.locomotion, *entity.skeleton);
            const float body_hands_relax=pose.face.hands_relax;

            const AnimationLODSpec spec = entity.lod.spec();
            if (entity.face != nullptr && spec.evaluate_face) {
                if (!entity.face->setLookTarget(entity.look_target, entity.root_position)) {
                    recordError(index, {foundation::ErrorCode::InvalidState,
                                        "face look target rejected by animation stage"});
                    continue;
                }
                float remaining = fixed_dt_seconds *
                                  static_cast<float>(std::min<std::uint64_t>(
                                      work[index].elapsed_ticks, 1024U));
                while (remaining > 0.0F) {
                    const float step = std::min(remaining, 0.25F);
                    if (!entity.face->step(step)) {
                        recordError(index, {foundation::ErrorCode::InvalidState,
                                            "face animation stage rejected fixed interval"});
                        break;
                    }
                    remaining -= step;
                }
                pose.face = entity.face->output();
                pose.face.hands_relax=body_hands_relax;
            }
            applyFacePose(pose.bones, pose.face);
            // Look rotates the head after body/ground IK. Re-check the final
            // head and head-gear volume before publishing the pose so steep
            // terrain cannot clip the post-look result.
            applyPostLookHeadClearance(pose, entity, *entity.locomotion, *entity.skeleton);
            entity.lod.markEvaluated(simulation_tick);
        }
    };

    if (jobs != nullptr && chunk_count > 1U) {
        jobs::JobGraphBuilder graph;
        std::vector<jobs::JobGraphNode> chunks;
        chunks.reserve(chunk_count);
        for (std::size_t chunk = 0U; chunk < chunk_count; ++chunk) {
            const std::size_t begin = chunk * chunk_size_;
            const std::size_t end = begin + std::min(entities.size() - begin, chunk_size_);
            jobs::JobOptions options;
            options.work_class = jobs::WorkClass::Presentation;
            options.cancellation = cancellation;
            chunks.push_back(graph.add(
                [&, begin, end](jobs::JobContext&) { processRange(begin, end); }, options));
        }
        auto completion = std::move(graph).build().run(*jobs);
        completion.wait();
        if (completion.failed()) {
            failed.store(true, std::memory_order_release);
        }
    } else {
        processRange(0U, entities.size());
    }

    if (failed.load(std::memory_order_acquire)) {
        return foundation::Result<void, foundation::Error>::failure(
            first_error.code == foundation::ErrorCode::None
                ? foundation::Error{foundation::ErrorCode::Internal,
                                    "animation batch job failed"}
                : first_error);
    }

    stats_ = {simulation_tick,
              static_cast<std::uint32_t>(entities.size()),
              due_count,
              due_count,
              static_cast<std::uint32_t>(chunk_count),
              current_.pose_revision,
              std::chrono::duration_cast<foundation::Nanoseconds>(
                  std::chrono::steady_clock::now() - evaluation_started)};
    return foundation::Result<void, foundation::Error>::success();
}

AnimationPose AnimationSystem::interpolate(const AnimationPose& previous,
                                           const AnimationPose& current,
                                           float alpha) noexcept {
    const float t = std::clamp(finite(alpha) ? alpha : 0.0F, 0.0F, 1.0F);
    AnimationPose result = current;
    result.semantic_id = current.semantic_id != 0U ? current.semantic_id : previous.semantic_id;
    result.root_position = {mix(previous.root_position.x, current.root_position.x, t),
                            mix(previous.root_position.y, current.root_position.y, t),
                            mix(previous.root_position.z, current.root_position.z, t)};
    result.posture.crouch = mix(previous.posture.crouch, current.posture.crouch, t);
    result.posture.hip_height = mix(previous.posture.hip_height, current.posture.hip_height, t);
    result.posture.torso_pitch = mix(previous.posture.torso_pitch, current.posture.torso_pitch, t);
    result.posture.knee_bend = mix(previous.posture.knee_bend, current.posture.knee_bend, t);
    result.posture.ankle_pitch = mix(previous.posture.ankle_pitch, current.posture.ankle_pitch, t);
    result.posture.arm_relax = mix(previous.posture.arm_relax, current.posture.arm_relax, t);
    result.posture.pelvis_pitch = mix(previous.posture.pelvis_pitch, current.posture.pelvis_pitch, t);
    result.posture.hip_y = mix(previous.posture.hip_y, current.posture.hip_y, t);
    result.posture.hip_z = mix(previous.posture.hip_z, current.posture.hip_z, t);
    result.posture.lower_pitch = mix(previous.posture.lower_pitch, current.posture.lower_pitch, t);
    result.posture.upper_pitch = mix(previous.posture.upper_pitch, current.posture.upper_pitch, t);
    result.posture.chest_pitch = mix(previous.posture.chest_pitch, current.posture.chest_pitch, t);
    result.posture.stance_half = mix(previous.posture.stance_half, current.posture.stance_half, t);
    result.posture.foot_z = mix(previous.posture.foot_z, current.posture.foot_z, t);
    result.posture.foot_yaw = mix(previous.posture.foot_yaw, current.posture.foot_yaw, t);
    result.posture.knee_half = mix(previous.posture.knee_half, current.posture.knee_half, t);
    result.locomotion_phase = mix(previous.locomotion_phase, current.locomotion_phase, t);
    result.target_hand_curl=mix(previous.target_hand_curl,current.target_hand_curl,t);
    result.weapon_readiness = mix(previous.weapon_readiness, current.weapon_readiness, t);
    result.weapon_recoil = mix(previous.weapon_recoil, current.weapon_recoil, t);
    const foundation::Vec3 blended_aim_direction{
        mix(previous.weapon_aim_direction.x, current.weapon_aim_direction.x, t),
        mix(previous.weapon_aim_direction.y, current.weapon_aim_direction.y, t),
        mix(previous.weapon_aim_direction.z, current.weapon_aim_direction.z, t)};
    result.weapon_aim_direction =
        dot(blended_aim_direction, blended_aim_direction) > 1.0e-8F
            ? blended_aim_direction : current.weapon_aim_direction;
    for(std::size_t index=0;index<2U;++index){
        result.foot_targets[index]={mix(previous.foot_targets[index].x,current.foot_targets[index].x,t),
            mix(previous.foot_targets[index].y,current.foot_targets[index].y,t),
            mix(previous.foot_targets[index].z,current.foot_targets[index].z,t)};
        result.foot_goals[index]={mix(previous.foot_goals[index].x,current.foot_goals[index].x,t),
            mix(previous.foot_goals[index].y,current.foot_goals[index].y,t),
            mix(previous.foot_goals[index].z,current.foot_goals[index].z,t)};
        result.knee_targets[index]={mix(previous.knee_targets[index].x,current.knee_targets[index].x,t),
            mix(previous.knee_targets[index].y,current.knee_targets[index].y,t),
            mix(previous.knee_targets[index].z,current.knee_targets[index].z,t)};
        result.hand_targets[index]={mix(previous.hand_targets[index].x,current.hand_targets[index].x,t),
            mix(previous.hand_targets[index].y,current.hand_targets[index].y,t),
            mix(previous.hand_targets[index].z,current.hand_targets[index].z,t)};
        result.elbow_targets[index]={mix(previous.elbow_targets[index].x,current.elbow_targets[index].x,t),
            mix(previous.elbow_targets[index].y,current.elbow_targets[index].y,t),
            mix(previous.elbow_targets[index].z,current.elbow_targets[index].z,t)};
        result.foot_plant[index]=mix(previous.foot_plant[index],current.foot_plant[index],t);
        result.foot_support[index]=mix(previous.foot_support[index],current.foot_support[index],t);
        result.foot_pitch[index]=mix(previous.foot_pitch[index],current.foot_pitch[index],t);
        result.toe_pitch[index]=mix(previous.toe_pitch[index],current.toe_pitch[index],t);
        result.foot_yaw[index]=mix(previous.foot_yaw[index],current.foot_yaw[index],t);
        result.hand_plant[index]=mix(previous.hand_plant[index],current.hand_plant[index],t);
        result.hand_lift[index]=mix(previous.hand_lift[index],current.hand_lift[index],t);
        result.foot_relative[index]=mix(previous.foot_relative[index],current.foot_relative[index],t);
        result.ankle_pitch[index]=mix(previous.ankle_pitch[index],current.ankle_pitch[index],t);
        result.ankle_yaw[index]=mix(previous.ankle_yaw[index],current.ankle_yaw[index],t);
    }
    for (std::size_t channel = 0U; channel < kFaceChannelCount; ++channel) {
        result.face.channels[channel] = mix(previous.face.channels[channel], current.face.channels[channel], t);
    }
    result.face.eyelids_close = mix(previous.face.eyelids_close, current.face.eyelids_close, t);
    result.face.eyelids_arc = mix(previous.face.eyelids_arc, current.face.eyelids_arc, t);
    result.face.neck_flex = mix(previous.face.neck_flex, current.face.neck_flex, t);
    result.face.hands_relax = mix(previous.face.hands_relax, current.face.hands_relax, t);
    result.face.jaw_rotation = mix(previous.face.jaw_rotation, current.face.jaw_rotation, t);
    result.face.head_yaw = mix(previous.face.head_yaw, current.face.head_yaw, t);
    result.face.head_pitch = mix(previous.face.head_pitch, current.face.head_pitch, t);
    result.face.eye_yaw = mix(previous.face.eye_yaw, current.face.eye_yaw, t);
    result.face.eye_pitch = mix(previous.face.eye_pitch, current.face.eye_pitch, t);
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
        result.target_bones[bone].translation = {
            mix(previous.target_bones[bone].translation.x,current.target_bones[bone].translation.x,t),
            mix(previous.target_bones[bone].translation.y,current.target_bones[bone].translation.y,t),
            mix(previous.target_bones[bone].translation.z,current.target_bones[bone].translation.z,t)};
        result.target_bones[bone].rotation=mixQuaternion(previous.target_bones[bone].rotation,
                                                         current.target_bones[bone].rotation,t);
        result.damped_bones[bone].translation = {
            mix(previous.damped_bones[bone].translation.x,current.damped_bones[bone].translation.x,t),
            mix(previous.damped_bones[bone].translation.y,current.damped_bones[bone].translation.y,t),
            mix(previous.damped_bones[bone].translation.z,current.damped_bones[bone].translation.z,t)};
        result.damped_bones[bone].rotation=mixQuaternion(previous.damped_bones[bone].rotation,
                                                         current.damped_bones[bone].rotation,t);
        result.bones[bone].translation = {
            mix(previous.bones[bone].translation.x, current.bones[bone].translation.x, t),
            mix(previous.bones[bone].translation.y, current.bones[bone].translation.y, t),
            mix(previous.bones[bone].translation.z, current.bones[bone].translation.z, t)};
        result.bones[bone].rotation =
            mixQuaternion(previous.bones[bone].rotation, current.bones[bone].rotation, t);
        result.bones[bone].scale = {
            mix(previous.bones[bone].scale.x, current.bones[bone].scale.x, t),
            mix(previous.bones[bone].scale.y, current.bones[bone].scale.y, t),
            mix(previous.bones[bone].scale.z, current.bones[bone].scale.z, t)};
    }
    result.evaluated = previous.evaluated || current.evaluated;
    return result;
}

} // namespace genomes::infantry
