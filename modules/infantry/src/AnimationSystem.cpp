#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>

namespace genomes::infantry {

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
    return {static_cast<float>(a.x*left+b.x*right),static_cast<float>(a.y*left+b.y*right),
            static_cast<float>(a.z*left+b.z*right),static_cast<float>(a.w*left+b.w*right)};
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

void applyLocomotionPose(std::array<RigTransform, kRigBoneCount>& bones,
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
    const float sprint=state.sprint_weight*state.amplitude;
    const bool moving=state.actual_speed_mps>.008F;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        const auto foot=PostureProfile::sampleLowContact(
            state.phase+static_cast<double>(index)*.5,state.duty,state.cycle_m,
            state.lift_m,sprint);
        pose.foot_targets[index]={sign*(pose.posture.stance_half/height-.003F*sprint),
            .045F+(moving?foot.lift/height*state.amplitude:0.0F),
            pose.posture.foot_z/height+(moving?foot.z/height:0.0F)};
        pose.knee_targets[index]={sign*pose.posture.knee_half/height,
            .045F+leg/height*.42F,leg/height*.82F+(moving?foot.z/height*.22F:0.0F)};
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
        pose.foot_yaw[index]=sign*(pose.posture.foot_yaw-.025F*sprint);
    }
}

void applyReferenceBipedPose(AnimationPose& pose,const LocomotionController& controller,
                             const LocomotionState& state,float time) noexcept {
    constexpr float tau=6.28318530717958647692F;
    const auto& body=controller.body();
    const float depth=state.actual_crouch,run=state.run_weight;
    const bool moving=state.actual_speed_mps>.008F;
    const float amplitude=moving?state.amplitude:0.0F;
    const float sprint=state.sprint_weight*amplitude;
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
    setEuler(pose.bones,BoneId::Hips,pose.posture.pelvis_pitch+running_lean,yaw,roll);
    setEuler(pose.bones,BoneId::SpineLower,pose.posture.lower_pitch+running_lean*.55F+breath*.002F,
             -yaw*(.30F+.10F*sprint),-roll*.35F);
    setEuler(pose.bones,BoneId::SpineUpper,pose.posture.upper_pitch+running_lean*.35F+breath*.003F,
             -yaw*(.38F+.22F*sprint),-roll*.40F);
    setEuler(pose.bones,BoneId::Chest,pose.posture.chest_pitch+breath*.002F,
             -yaw*(.20F+.10F*sprint),-roll*.20F);
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

[[maybe_unused]] void applyProneArmIK(AnimationPose& pose,const LocomotionController& controller,
                     const SkeletonData& skeleton) noexcept {
    const float height=controller.body().height;
    const auto rest=skeleton.bones();
    for(std::size_t index=0;index<2U;++index){
        const BoneId upper=index==0U?BoneId::UpperArmL:BoneId::UpperArmR;
        const BoneId lower=index==0U?BoneId::ForeArmL:BoneId::ForeArmR;
        const BoneId hand=index==0U?BoneId::HandL:BoneId::HandR;
        auto target=scale(pose.hand_targets[index],height);
        target.y = 0.002F;
        const auto pole=scale(pose.elbow_targets[index],height);
        solveFullFrameChain(pose,skeleton,upper,lower,hand,target,pole,
                            foundation::Vec3{0,-1,0});
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
        if(tag.name==tag_name||tag.name==boot_tag||tag.name=="bootLeather")
            selected.insert(selected.end(),tag.vertices.begin(),tag.vertices.end());
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

void applySeatedPose(AnimationPose& pose,const LocomotionController& controller,
                     const SkeletonData& skeleton,const AppearanceMesh* surface=nullptr) noexcept {
    const auto& body=controller.body();const float height=body.height;
    const float leg=body.anatomy_leg_length/height,hip_half=.052F*body.hip_width_scale;
    pose.bones[boneIndex(BoneId::Hips)].translation={0.0F,
        (.045F+leg*.635F)*height,-leg*.202F*height};
    setEuler(pose.bones,BoneId::SpineLower,.12F);
    setEuler(pose.bones,BoneId::SpineUpper,.15F);
    setEuler(pose.bones,BoneId::Chest,.06F);
    setEuler(pose.bones,BoneId::Neck,-.15F);
    constexpr float arm_angle=.3839724354387525F;
    for(std::size_t index=0;index<2U;++index){
        const float sign=index==0U?1.0F:-1.0F;
        pose.foot_targets[index]={sign*(hip_half+.020F),.045F,leg*.073F};
        pose.knee_targets[index]={sign*(hip_half+.048F),.045F+leg*.32F,leg*.96F};
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
    // Prone fixture legs retain the authored crawl pose; contact targets are
    // published separately and are not a second rotation pass.
    // The pinned animation fixture uses the authored prone arm rotations. Its
    // flat contact surface leaves the hand targets as diagnostics; applying a
    // second native IK solve here would rotate the arms away from that pose.
}

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

[[nodiscard]] bool armBone(std::size_t index) noexcept {
    return (index>=boneIndex(BoneId::ClavicleL)&&index<=boneIndex(BoneId::HandL))||
           (index>=boneIndex(BoneId::ClavicleR)&&index<=boneIndex(BoneId::HandR));
}

void dampBipedPose(AnimationPose& pose,const AnimationPose* previous,
                   const LocomotionController& controller,const LocomotionState& state,
                   const SkeletonData& skeleton,float time,float dt) noexcept {
    (void)controller;(void)state;(void)skeleton;(void)time;
    std::array<RigTransform,kRigBoneCount> current{};
    std::array<RigTransform,kRigBoneCount> previous_target{};
    float current_hand_curl=0.0F;
    if(previous!=nullptr&&previous->evaluated){
        current=previous->damped_bones;
        previous_target=previous->target_bones;
        current_hand_curl=previous->face.hands_relax;
        // JS transports arm damping through the cyclic previousArmPose, not
        // through the previous frame's target pose. Reconstruct that moving
        // phase for every subsequent frame as well as the initial frame.
        if(state.actual_speed_mps>0.008F){
            const auto bind=skeleton.bones();
            auto previous_state=state;
            const float phase_delta=state.actual_speed_mps/
                std::max(0.01F,static_cast<float>(state.cycle_m))*dt;
            previous_state.phase-=phase_delta;
            previous_state.phase-=std::floor(previous_state.phase);
            AnimationPose moving_frame{};
            for(std::size_t bone=0U;bone<kRigBoneCount;++bone)
                moving_frame.bones[bone]=bind[bone].local_bind;
            moving_frame.posture=controller.posture(previous_state);
            sampleBipedTargets(moving_frame,controller,previous_state);
            applyReferenceBipedPose(moving_frame,controller,previous_state,time-dt);
            previous_target=moving_frame.bones;
        }
    }else if(state.actual_speed_mps==0.0F){
        AnimationPose initial{};const auto bind=skeleton.bones();
        for(std::size_t bone=0;bone<kRigBoneCount;++bone)initial.bones[bone]=bind[bone].local_bind;
        auto initial_state=state;initial_state.phase=0.0;initial_state.actual_speed_mps=0.0F;
        initial.posture=controller.posture(initial_state);sampleBipedTargets(initial,controller,initial_state);
        applyReferenceBipedPose(initial,controller,initial_state,0.0F);current=initial.bones;
        current_hand_curl=initial.face.hands_relax;
        auto previous_phase=state;previous_phase.phase=0.0;AnimationPose arm_target{};
        for(std::size_t bone=0;bone<kRigBoneCount;++bone)arm_target.bones[bone]=bind[bone].local_bind;
        arm_target.posture=controller.posture(previous_phase);sampleBipedTargets(arm_target,controller,previous_phase);
        applyReferenceBipedPose(arm_target,controller,previous_phase,time-dt);previous_target=arm_target.bones;
    }else{
        // JS setState(..., immediate=true) leaves the pose at the previous
        // gait phase. update() advances the phase, samples the new target,
        // and only then applies one damping step. Reconstruct that previous
        // sample here so the first moving frame has the same temporal lag.
        AnimationPose previous_frame{};
        const auto bind=skeleton.bones();
        for(std::size_t bone=0U;bone<kRigBoneCount;++bone)
            previous_frame.bones[bone]=bind[bone].local_bind;
        auto previous_state=state;
        const float phase_delta=state.actual_speed_mps/
            std::max(0.01F,static_cast<float>(state.cycle_m))*dt;
        previous_state.phase-=phase_delta;
        previous_state.phase-=std::floor(previous_state.phase);
        // setState(..., immediate=true) samples the pose before the JS
        // animator receives its per-frame motion speed. Preserve the snapped
        // gait profile (run weight), but make that first pose stationary.
        previous_state.actual_speed_mps=0.0F;
        previous_state.amplitude=0.0F;
        previous_frame.posture=controller.posture(previous_state);
        sampleBipedTargets(previous_frame,controller,previous_state);
        applyReferenceBipedPose(previous_frame,controller,previous_state,time-dt);
        current=previous_frame.bones;
        // The JS animator also refreshes its cyclic arm transport pose from
        // the moving gait, even though the damped body starts at the
        // stationary immediate pose. Keep these two snapshots distinct.
        auto moving_previous=state;
        moving_previous.phase=previous_state.phase;
        AnimationPose moving_frame{};
        for(std::size_t bone=0U;bone<kRigBoneCount;++bone)
            moving_frame.bones[bone]=bind[bone].local_bind;
        moving_frame.posture=controller.posture(moving_previous);
        sampleBipedTargets(moving_frame,controller,moving_previous);
        applyReferenceBipedPose(moving_frame,controller,moving_previous,time-dt);
        previous_target=moving_frame.bones;
        current_hand_curl=previous_frame.face.hands_relax;
    }
    const float hips_alpha=1.0F-std::exp(-18.0F*dt);
    current[boneIndex(BoneId::Hips)].translation={
        mix(current[boneIndex(BoneId::Hips)].translation.x,pose.target_bones[boneIndex(BoneId::Hips)].translation.x,hips_alpha),
        mix(current[boneIndex(BoneId::Hips)].translation.y,pose.target_bones[boneIndex(BoneId::Hips)].translation.y,hips_alpha),
        mix(current[boneIndex(BoneId::Hips)].translation.z,pose.target_bones[boneIndex(BoneId::Hips)].translation.z,hips_alpha)};
    for(std::size_t bone=0;bone<kRigBoneCount;++bone){
        auto value=current[bone].rotation;
        if(armBone(bone)){
            const auto transport=multiply(pose.target_bones[bone].rotation,
                                          inverse(previous_target[bone].rotation));
            value=multiply(transport,value);
        }
        current[bone].rotation=slerp(value,pose.target_bones[bone].rotation,
                                     1.0F-std::exp(-boneResponse(bone)*dt));
    }
    pose.damped_bones=current;
    pose.bones=current;
    pose.face.hands_relax=mix(current_hand_curl,pose.face.hands_relax,
                              1.0F-std::exp(-22.0F*dt));
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
           locomotion_state != nullptr && locomotion_state->valid() && finite(root_position) &&
           (!look_target.has_value() || finite(look_target.value())) && lod.spec().valid();
}

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
    if(!finite(target_hand_curl))return false;
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

foundation::Result<void, foundation::Error> AnimationSystem::evaluate(
    std::span<AnimationEntity> entities,
    std::uint64_t simulation_tick,
    float fixed_dt_seconds,
    jobs::JobSystem* jobs) {
    if (!finite(fixed_dt_seconds) || fixed_dt_seconds <= 0.0F || fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid animation fixed interval"});
    }
    for (const AnimationEntity& entity : entities) {
        if (!entity.valid()) {
            return foundation::Result<void, foundation::Error>::failure(invalidEntityError());
        }
    }

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
        entities.empty() ? 0U : (entities.size() + chunk_size_ - 1U) / chunk_size_;
    std::atomic<bool> failed{false};
    std::mutex error_mutex;
    foundation::Error first_error{};

    const auto processRange = [&](std::size_t begin, std::size_t end) noexcept {
        for (std::size_t index = begin; index < end; ++index) {
            if (!work[index].due || failed.load(std::memory_order_acquire)) {
                continue;
            }
            AnimationEntity& entity = entities[index];
            AnimationPose& pose = current_.poses[index];
            const PostureSample posture = entity.locomotion->posture(*entity.locomotion_state);
            pose.semantic_id = entity.semantic_id;
            pose.root_position = entity.root_position;
            pose.posture = posture;
            pose.locomotion_phase = entity.locomotion_state->phase;
            pose.revision = current_.pose_revision;
            pose.evaluated = true;
            if(entity.locomotion_state->family==LocomotionFamily::Biped)
                sampleBipedTargets(pose,*entity.locomotion,*entity.locomotion_state);

            const auto bones = entity.skeleton->bones();
            if (bones.size() != kRigBoneCount) {
                std::lock_guard lock(error_mutex);
                if (!failed.exchange(true, std::memory_order_acq_rel)) {
                    first_error = {foundation::ErrorCode::InvalidState,
                                   "animation skeleton does not match rig schema"};
                }
                continue;
            }
            for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
                pose.bones[bone] = bones[bone].local_bind;
            }
            pose.target_bones=pose.bones;
            pose.damped_bones=pose.bones;

            if(entity.locomotion_state->family==LocomotionFamily::Biped){
                applyReferenceBipedPose(pose,*entity.locomotion,*entity.locomotion_state,
                    static_cast<float>(simulation_tick+1U)*fixed_dt_seconds);
                pose.target_hand_curl=pose.face.hands_relax;
                pose.target_bones=pose.bones;
                const AnimationPose* previous_pose=index<previous_.poses.size()&&
                    previous_.poses[index].semantic_id==entity.semantic_id?&previous_.poses[index]:nullptr;
                dampBipedPose(pose,previous_pose,*entity.locomotion,*entity.locomotion_state,
                    *entity.skeleton,static_cast<float>(simulation_tick+1U)*fixed_dt_seconds,
                    fixed_dt_seconds);
                // JS damps the pose first, then constrains the hips against
                // the reachable foot targets before running contact IK.
                // Reapply the same reach constraint after native damping.
                {
                    const auto& body=entity.locomotion->body();
                    const float height=body.height;
                    auto& hips=pose.bones[boneIndex(BoneId::Hips)];
                    float normalized_y=hips.translation.y/height;
                    const float normalized_x=hips.translation.x/height;
                    const float normalized_z=hips.translation.z/height;
                    for(std::size_t side=0U;side<2U;++side){
                        const float sign=side==0U?1.0F:-1.0F;
                        const auto hip_offset=rotate({sign*.052F*body.hip_width_scale,-.015F,0.0F},hips.rotation);
                        const float dx=pose.foot_targets[side].x-normalized_x-hip_offset.x;
                        const float dz=pose.foot_targets[side].z-normalized_z-hip_offset.z;
                        const float reach=body.anatomy_leg_length/height*.992F;
                        const float max_y=pose.foot_targets[side].y+
                            std::sqrt(std::max(.0001F,reach*reach-dx*dx-dz*dz))-hip_offset.y;
                        normalized_y=std::min(normalized_y,max_y);
                    }
                    hips.translation.y=normalized_y*height;
                    // Keep the constrained value in the snapshot used as the
                    // next frame's damped pose, matching JS current.hips.
                    pose.damped_bones[boneIndex(BoneId::Hips)].translation=hips.translation;
                }
                applyBipedLegIK(pose,*entity.locomotion,*entity.skeleton,entity.surface);
            } else if(entity.locomotion_state->family==LocomotionFamily::Seated) {
                applySeatedPose(pose,*entity.locomotion,*entity.skeleton,entity.surface);
            } else if(entity.locomotion_state->family==LocomotionFamily::Prone) {
                applyPronePose(pose,*entity.locomotion,*entity.skeleton,*entity.locomotion_state,entity.surface);
            } else if(entity.locomotion_state->family!=LocomotionFamily::Rest) {
                applyLocomotionPose(pose.bones,*entity.locomotion_state,posture);
            }
            const float body_hands_relax=pose.face.hands_relax;

            const AnimationLODSpec spec = entity.lod.spec();
            if (entity.face != nullptr && spec.evaluate_face) {
                if (!entity.face->setLookTarget(entity.look_target, entity.root_position)) {
                    std::lock_guard lock(error_mutex);
                    if (!failed.exchange(true, std::memory_order_acq_rel)) {
                        first_error = {foundation::ErrorCode::InvalidState,
                                       "face look target rejected by animation stage"};
                    }
                    continue;
                }
                float remaining = fixed_dt_seconds *
                                  static_cast<float>(std::min<std::uint64_t>(
                                      work[index].elapsed_ticks, 1024U));
                while (remaining > 0.0F) {
                    const float step = std::min(remaining, 0.25F);
                    if (!entity.face->step(step)) {
                        std::lock_guard lock(error_mutex);
                        if (!failed.exchange(true, std::memory_order_acq_rel)) {
                            first_error = {foundation::ErrorCode::InvalidState,
                                           "face animation stage rejected fixed interval"};
                        }
                        break;
                    }
                    remaining -= step;
                }
                pose.face = entity.face->output();
                pose.face.hands_relax=body_hands_relax;
            }
            applyFacePose(pose.bones, pose.face);
            entity.lod.markEvaluated(simulation_tick);
        }
    };

    if (jobs != nullptr && chunk_count > 1U) {
        std::vector<jobs::JobHandle> handles;
        handles.reserve(chunk_count);
        for (std::size_t chunk = 0U; chunk < chunk_count; ++chunk) {
            const std::size_t begin = chunk * chunk_size_;
            const std::size_t end = std::min(begin + chunk_size_, entities.size());
            handles.push_back(jobs->submit([&, begin, end](jobs::JobContext&) {
                processRange(begin, end);
            }));
        }
        for (const jobs::JobHandle& handle : handles) {
            jobs->wait(handle);
            if (handle.failed()) {
                failed.store(true, std::memory_order_release);
            }
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
              current_.pose_revision};
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
