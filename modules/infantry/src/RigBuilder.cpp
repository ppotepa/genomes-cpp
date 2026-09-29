#include <genomes/infantry/RigBuilder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec3;

[[nodiscard]] Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float lengthSquared(Vec3 value) noexcept { return dot(value, value); }

[[nodiscard]] float length(Vec3 value) noexcept { return std::sqrt(lengthSquared(value)); }

[[nodiscard]] Vec3 normalized(Vec3 value, Vec3 fallback = {0.0F, 1.0F, 0.0F}) noexcept {
    const float magnitude = length(value);
    return magnitude > 1.0e-6F && std::isfinite(magnitude) ? multiply(value, 1.0F / magnitude)
                                                            : fallback;
}

[[nodiscard]] Vec3 mix(Vec3 left, Vec3 right, float amount) noexcept {
    return add(left, multiply(subtract(right, left), amount));
}

[[nodiscard]] double mix(double left, double right, double amount) noexcept {
    return left + (right - left) * amount;
}

[[nodiscard]] double smooth(double value) noexcept {
    const double t = std::clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] double bell(double x, double y = 0.0) noexcept {
    return std::exp(-x * x - y * y);
}

struct ReferenceFaceLayout final {
    struct Section final {
        double y;
        double radius_x;
        double radius_z;
        double center_z;
    };
    struct Eye final {
        double x;
        double y;
        double z;
        double height;
        double radius;
    };
    struct Brow final { Vec3 inner; Vec3 outer; };

    explicit ReferenceFaceLayout(const BodyPhenotype& body, const FacePhenotype& face)
        : face_(face) {
        const double neck_x = .0285 * body.neck_scale;
        const double neck_z = .024 * body.neck_scale;
        neck_joint_y = mix(.837, .843, std::clamp((body.neck_scale - .76) / .56, 0.0, 1.0));
        constexpr std::array<std::array<double, 4>, 12> base{{
            {{.881,.034,.030,.013}}, {{.890,.039,.037,.010}},
            {{.900,.041,.041,.008}}, {{.913,.045,.044,.005}},
            {{.925,.049,.046,.003}}, {{.938,.048,.046,.002}},
            {{.948,.047,.046,.001}}, {{.959,.0465,.046,0}},
            {{.973,.043,.043,-.001}}, {{.986,.032,.034,-.003}},
            {{.995,.015,.018,-.004}}, {{.999,0,0,-.004}}
        }};
        std::vector<Section> shaped;
        shaped.reserve(base.size());
        for (const auto& value : base) {
            const double y0 = value[0];
            const double low = smooth(std::clamp((.928-y0)/.058, 0.0, 1.0));
            const double cheek = bell((y0-(.928+face.cheekbone_y))/.020);
            const double temple = bell((y0-.951)/.020);
            const double forehead = smooth((y0-.948)/.038);
            const double chin = bell((y0-.887)/.015);
            (void)cheek;
            double y = y0;
            if (y0 < .925) y = .925+(y0-.925)*face.jaw_length_scale+face.chin_height*chin*.65;
            if (y0 > .947) y = .999-(.999-y0)*face.head_length_scale;
            double scale = mix(1, face.jaw_width_scale, low) *
                           mix(1, face.jaw_angle, low*low*.40);
            scale = mix(scale, face.temple_width_scale, temple*.55);
            scale = mix(scale, face.forehead_width_scale, forehead*.65);
            shaped.push_back({y, value[1]*scale*face.head_width_scale*mix(1,body.head_scale,.4),
                              value[2]*face.head_depth_scale*mix(1,body.head_scale,.4), value[3]});
        }
        shaped[0].y = std::clamp(shaped[0].y, .8755, .8855);
        for (std::size_t i=1; i<shaped.size(); ++i)
            shaped[i].y = std::max(shaped[i].y, shaped[i-1].y+.003);
        chin_y = shaped[0].y;
        head_pivot_y = std::clamp(chin_y+.018, .892, .904);
        levels.push_back({.828,neck_x*1.14,neck_z*1.12,-.002});
        levels.push_back({.841,neck_x*1.06,neck_z*1.04,-.002});
        levels.push_back({.853,neck_x,neck_z,-.002});
        for (const double t : {.2,.4,.6,.8}) {
            const double blend=smooth(t);
            levels.push_back({mix(.853,chin_y,t),mix(neck_x*.99,shaped[0].radius_x,blend),
                              mix(neck_z*1.01,shaped[0].radius_z,blend),
                              mix(.001,shaped[0].center_z,blend)});
        }
        levels.insert(levels.end(), shaped.begin(), shaped.end());
        slopes.resize(levels.size());
        for (std::size_t i=0; i<levels.size(); ++i) {
            const auto component = [](const Section& s, std::size_t k) {
                return k==0?s.y:(k==1?s.radius_x:(k==2?s.radius_z:s.center_z));
            };
            for (std::size_t k=0; k<4; ++k) {
                if (i==0) slopes[i][k]=(component(levels[1],k)-component(levels[0],k))/(levels[1].y-levels[0].y);
                else if (i+1==levels.size()) slopes[i][k]=(component(levels[i],k)-component(levels[i-1],k))/(levels[i].y-levels[i-1].y);
                else {
                    const double dl=(component(levels[i],k)-component(levels[i-1],k))/(levels[i].y-levels[i-1].y);
                    const double dr=(component(levels[i+1],k)-component(levels[i],k))/(levels[i+1].y-levels[i].y);
                    slopes[i][k]=dl*dr<=0?0:2*dl*dr/(dl+dr);
                }
            }
        }
        mouth_y=std::clamp(static_cast<double>(face.mouth_y_ratio),chin_y+.014,face.eye_y_ratio-.031);
        const Section mouth_section=section(mouth_y);
        mouth_half=std::clamp(static_cast<double>(face.mouth_width_ratio)*.5,.007,mouth_section.radius_x*.57);
        mouth_z=frontZ(0,mouth_y);
        const double eye_w=.0083*face.eye_width_scale, eye_h=.0034*face.eye_height_scale;
        const double radius=std::max(eye_w*1.12,eye_h*1.65)*.82;
        const double rx=section(face.eye_y_ratio).radius_x;
        const double spacing=std::clamp(static_cast<double>(face.eye_spacing_ratio),radius*1.20+.003,
                                        std::max(radius*1.20+.003,rx*.66));
        for (std::size_t side=0; side<2; ++side) {
            const double sign=side==0?1:-1;
            const double x=sign*spacing, y=face.eye_y_ratio+sign*face.eye_asymmetry*.5;
            const double z=frontZ(x,y)-radius*.80+face.eye_depth*.16;
            eyes[side]={x,y,z,eye_h,radius};
            const double by=std::max(face.brow_y_ratio+sign*face.brow_asymmetry*.5,y+eye_h+.006);
            const double inner_x=sign*std::max(.006,spacing-.007+face.brow_spacing*.5);
            const double outer_x=sign*(spacing+.008+face.brow_spacing*.5);
            brows[side]={{static_cast<float>(inner_x),static_cast<float>(by),static_cast<float>(frontZ(inner_x,by)+.0012)},
                         {static_cast<float>(outer_x),static_cast<float>(by+face.brow_tilt*.006),static_cast<float>(frontZ(outer_x,by+face.brow_tilt*.006)+.0012)}};
        }
    }

    [[nodiscard]] Section section(double y) const noexcept {
        std::size_t i=0;
        while (i+1<levels.size() && levels[i+1].y<y) ++i;
        if (i+1>=levels.size()) return {y,0,0,levels.back().center_z};
        const double span=levels[i+1].y-levels[i].y;
        const double t=std::clamp((y-levels[i].y)/span,0.0,1.0),t2=t*t,t3=t2*t;
        const auto hermite=[&](double a,double b,double ma,double mb){return (2*t3-3*t2+1)*a+(t3-2*t2+t)*ma*span+(-2*t3+3*t2)*b+(t3-t2)*mb*span;};
        return {y,std::max(0.0,hermite(levels[i].radius_x,levels[i+1].radius_x,slopes[i][1],slopes[i+1][1])),
                std::max(0.0,hermite(levels[i].radius_z,levels[i+1].radius_z,slopes[i][2],slopes[i+1][2])),
                hermite(levels[i].center_z,levels[i+1].center_z,slopes[i][3],slopes[i+1][3])};
    }
    [[nodiscard]] double frontZ(double x,double y) const noexcept {
        const Section p=section(y); const double sx=std::clamp(x/std::max(.00001,p.radius_x),-.9999,.9999);
        const double cz=std::sqrt(std::max(0.0,1-sx*sx));
        const double chin=bell((y-(chin_y+.014))/.020),side=smooth((std::abs(sx)-.10)/.72);
        const double px=p.radius_x*sx*(1+(face_.chin_width_scale-1)*.30*chin*side);
        double z=p.center_z+p.radius_z*cz;
        if(cz>0&&y>.882){const double front=smooth((cz-.02)/.48);const double cheek=bell((std::abs(px)-.030*face_.cheekbone_scale)/.014,(y-(.925+face_.cheekbone_y))/.016);
            z+=(.0014+face_.cheek_fullness*.42+(face_.cheekbone_scale-1)*.0018)*cheek*front;
            z+=face_.chin_projection*.30*bell(px/.023,(y-(chin_y+.014))/.020)*front;
            z+=face_.brow_ridge*bell((std::abs(px)-face_.eye_spacing_ratio)/.017,(y-(face_.eye_y_ratio+.010))/.009)*cz;
            z+=face_.midface_projection*.38*bell(px/.034,(y-.918)/.024)*cz;
            z+=face_.forehead_slope*smooth((y-.947)/.045)*cz;}
        return z;
    }
    [[nodiscard]] double globeFront(const Eye& eye,double x,double y) const noexcept {const double d=(x-eye.x)*(x-eye.x)+(y-eye.y)*(y-eye.y);return eye.z+std::sqrt(std::max(0.0,eye.radius*eye.radius-d));}
    const FacePhenotype& face_; std::vector<Section> levels; std::vector<std::array<double,4>> slopes;
    double neck_joint_y{},chin_y{},head_pivot_y{},mouth_y{},mouth_half{},mouth_z{};
    std::array<Eye,2> eyes{}; std::array<Brow,2> brows{};
};

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] RigQuaternion normalize(RigQuaternion value) noexcept {
    const float magnitude = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z +
                                      value.w * value.w);
    if (!(magnitude > 1.0e-6F) || !std::isfinite(magnitude)) {
        return RigQuaternion::identity();
    }
    const float inverse = 1.0F / magnitude;
    return {value.x * inverse, value.y * inverse, value.z * inverse, value.w * inverse};
}

[[nodiscard]] RigQuaternion conjugate(RigQuaternion value) noexcept {
    return {-value.x, -value.y, -value.z, value.w};
}

[[nodiscard]] RigQuaternion multiply(RigQuaternion left, RigQuaternion right) noexcept {
    return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
            left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] Vec3 rotate(RigQuaternion rotation, Vec3 value) noexcept {
    const RigQuaternion vector{value.x, value.y, value.z, 0.0F};
    const RigQuaternion result = multiply(multiply(normalize(rotation), vector),
                                          conjugate(normalize(rotation)));
    return {result.x, result.y, result.z};
}

[[nodiscard]] RigTransform compose(const RigTransform& parent,
                                   const RigTransform& local) noexcept {
    RigTransform result{};
    result.scale = {parent.scale.x * local.scale.x, parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = normalize(multiply(parent.rotation, local.rotation));
    const Vec3 scaled_local{local.translation.x * parent.scale.x,
                            local.translation.y * parent.scale.y,
                            local.translation.z * parent.scale.z};
    result.translation = add(parent.translation, rotate(parent.rotation, scaled_local));
    return result;
}

[[nodiscard]] RigTransform inverse(const RigTransform& transform) noexcept {
    RigTransform result{};
    result.scale = {1.0F / transform.scale.x, 1.0F / transform.scale.y,
                    1.0F / transform.scale.z};
    result.rotation = conjugate(normalize(transform.rotation));
    const Vec3 translated = multiply(transform.translation, -1.0F);
    result.translation = rotate(result.rotation,
                                {translated.x * result.scale.x, translated.y * result.scale.y,
                                 translated.z * result.scale.z});
    return result;
}

[[nodiscard]] foundation::StableId hashInput(const BodyPhenotype& body,
                                              const FacePhenotype& face) noexcept {
    foundation::StableId hash = foundation::stable_id("infantry.rig.v1");
    const auto addFloat = [&hash](float value) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value));
    };
    addFloat(body.height);
    addFloat(body.shoulder_width);
    addFloat(body.chest_depth);
    addFloat(body.hip_width);
    addFloat(body.arm_length);
    addFloat(body.leg_length);
    addFloat(body.pelvis.x);
    addFloat(body.pelvis.y);
    addFloat(body.pelvis.z);
    addFloat(body.chest.x);
    addFloat(body.chest.y);
    addFloat(body.chest.z);
    addFloat(body.head.x);
    addFloat(body.head.y);
    addFloat(body.head.z);
    addFloat(body.left_hand.x);
    addFloat(body.left_hand.y);
    addFloat(body.left_hand.z);
    addFloat(body.right_hand.x);
    addFloat(body.right_hand.y);
    addFloat(body.right_hand.z);
    addFloat(body.left_foot.x);
    addFloat(body.left_foot.y);
    addFloat(body.left_foot.z);
    addFloat(body.right_foot.x);
    addFloat(body.right_foot.y);
    addFloat(body.right_foot.z);
    addFloat(face.eye_spacing);
    addFloat(face.eye_radius);
    addFloat(face.brow_y);
    addFloat(face.eye_y);
    addFloat(face.nose_y);
    addFloat(face.nose_length);
    addFloat(face.nose_width);
    addFloat(face.mouth_y);
    addFloat(face.mouth_width);
    addFloat(face.upper_lip);
    addFloat(face.lower_lip);
    addFloat(face.jaw_width);
    addFloat(face.hairline_y);
    addFloat(face.head_width);
    addFloat(face.head_depth);
    addFloat(face.head_length_scale);
    addFloat(face.forehead_width_scale);
    addFloat(face.forehead_slope);
    addFloat(face.temple_width_scale);
    addFloat(face.brow_ridge);
    addFloat(face.jaw_length_scale);
    addFloat(face.jaw_angle);
    addFloat(face.chin_width_scale);
    addFloat(face.chin_height);
    addFloat(face.chin_projection);
    addFloat(face.cheekbone_scale);
    addFloat(face.cheekbone_y);
    addFloat(face.cheek_fullness);
    addFloat(face.midface_projection);
    addFloat(face.eye_width_scale);
    addFloat(face.eye_height_scale);
    addFloat(face.eye_depth);
    addFloat(face.eye_tilt);
    addFloat(face.eye_roundness);
    addFloat(face.nose_projection_scale);
    addFloat(face.nose_bridge_scale);
    addFloat(face.nose_tip_width_scale);
    addFloat(face.nostril_width_scale);
    addFloat(face.ear_scale);
    addFloat(face.ear_angle);
    addFloat(face.eye_asymmetry);
    addFloat(face.brow_asymmetry);
    addFloat(face.mouth_asymmetry);
    addFloat(face.ear_asymmetry);
    return hash;
}

void setPoint(std::array<Vec3, kRigBoneCount>& points, BoneId id, Vec3 point) noexcept {
    points[boneIndex(id)] = point;
}

void buildFinger(std::array<Vec3, kRigBoneCount>& points,
                 BoneId first,
                 Vec3 hand,
                 Vec3 forearm,
                 float lateral_offset,
                 bool thumb,
                 float height,
                 float hand_scale,
                 float digit_length) noexcept {
    const Vec3 direction = normalized(subtract(hand, forearm), {0.0F, -1.0F, 0.0F});
    const Vec3 side = normalized(cross(direction, {0.0F, 0.0F, 1.0F}));
    Vec3 axis = add(direction, multiply({0.0F, 0.0F, 1.0F}, thumb ? .22F : .08F));
    if (thumb) {
        axis = add(axis, multiply(side, lateral_offset < 0.0F ? -0.75F : 0.75F));
    }
    axis = normalized(axis, direction);
    const Vec3 start = add(add(hand, multiply(direction, height * hand_scale * (thumb ? 0.024F : 0.052F))),
                           multiply(side, height * hand_scale * lateral_offset));
    const float segment_length = height * hand_scale * digit_length;
    const std::size_t first_index = boneIndex(first);
    setPoint(points, static_cast<BoneId>(first_index), start);
    setPoint(points, static_cast<BoneId>(first_index + 1U),
             add(start, multiply(axis, segment_length * 0.32F)));
    setPoint(points, static_cast<BoneId>(first_index + 2U),
             add(start, multiply(axis, segment_length * 0.65F)));
}

} // namespace

const BoneRecord* SkeletonData::find(BoneId id) const noexcept {
    const std::size_t index = boneIndex(id);
    if (index >= bones_.size() || bones_[index].id != id) {
        return nullptr;
    }
    return &bones_[index];
}

const AttachmentPoint* SkeletonData::findAttachment(AttachmentPointId id) const noexcept {
    for (const AttachmentPoint& attachment : attachments_) {
        if (attachment.id == id) {
            return &attachment;
        }
    }
    return nullptr;
}

bool SkeletonData::valid() const noexcept {
    if (schema_version_ != RigSchemaVersion || cache_key_ == 0 || bones_.size() != kRigBoneCount) {
        return false;
    }
    std::size_t body_count = 0U;
    std::size_t face_count = 0U;
    std::size_t finger_count = 0U;
    std::size_t roots = 0U;
    for (std::size_t index = 0U; index < bones_.size(); ++index) {
        const BoneRecord& bone = bones_[index];
        const BoneSchema& schema = kCanonicalRigSchema[index];
        if (bone.id != schema.id || bone.name != schema.name || bone.parent != schema.parent ||
            bone.category != schema.category) {
            return false;
        }
        if (bone.parent == kInvalidBoneIndex) {
            ++roots;
        } else if (bone.parent >= index || bone.parent >= bones_.size()) {
            return false;
        }
        const auto finiteTransform = [](const RigTransform& transform) {
            return finite(transform.translation) && finite(transform.scale) &&
                   std::isfinite(transform.rotation.x) && std::isfinite(transform.rotation.y) &&
                   std::isfinite(transform.rotation.z) && std::isfinite(transform.rotation.w) &&
                   transform.scale.x > 0.0F && transform.scale.y > 0.0F &&
                   transform.scale.z > 0.0F;
        };
        if (!finiteTransform(bone.local_bind) || !finiteTransform(bone.world_bind) ||
            !finiteTransform(bone.inverse_bind) ||
            (bone.parent != kInvalidBoneIndex && !(bone.reference_length > 1.0e-6F))) {
            return false;
        }
        switch (bone.category) {
        case BoneCategory::Body:
            ++body_count;
            break;
        case BoneCategory::FaceController:
            ++face_count;
            break;
        case BoneCategory::Finger:
            ++finger_count;
            break;
        }
    }
    if (roots != 1U || body_count != kBodyBoneCount || face_count != kFaceBoneCount ||
        finger_count != kFingerBoneCount) {
        return false;
    }
    for (const AttachmentPoint& attachment : attachments_) {
        if (find(attachment.bone) == nullptr || !finite(attachment.local) ||
            !finite(attachment.world)) {
            return false;
        }
    }
    return true;
}

foundation::Result<SkeletonData, foundation::Error> RigBuilder::build(
    const BodyPhenotype& body, const FacePhenotype& face) {
    if (!body.valid() || !face.valid()) {
        return foundation::Result<SkeletonData, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry rig phenotype"});
    }

    std::array<Vec3, kRigBoneCount> points{};
    const ReferenceFaceLayout layout(body, face);
    const float height=body.height, hip_y=body.hip_y*height;
    const float neck_y=static_cast<float>(layout.neck_joint_y*height);
    const float head_y=static_cast<float>(layout.head_pivot_y*height);
    const float chest_y=static_cast<float>(mix(body.hip_y,layout.neck_joint_y,.76)*height);
    setPoint(points, BoneId::Hips, {0,hip_y,0});
    setPoint(points, BoneId::SpineLower, {0,static_cast<float>(mix(body.hip_y,layout.neck_joint_y,.25)*height),0});
    setPoint(points, BoneId::SpineUpper, {0,static_cast<float>(mix(body.hip_y,layout.neck_joint_y,.56)*height),0});
    setPoint(points, BoneId::Chest, {0,chest_y,0});
    setPoint(points, BoneId::Neck, {0,neck_y,0});
    setPoint(points, BoneId::Head, {0,head_y,0});

    const float shoulder_half=.128F*body.shoulder_width_scale*height;
    const float clavicle_half=.050F*(.75F+.25F*body.shoulder_width_scale)*height;
    const float shoulder_y=static_cast<float>(mix(chest_y/height,layout.neck_joint_y,.34)*height);
    const float arm_angle=22.0F*3.14159265358979323846F/180.0F;
    const float upper_length=.18F*body.arm_length_scale*height;
    const float fore_length=.155F*body.arm_length_scale*height;
    const auto build_side=[&](float sign,BoneId clavicle,BoneId upper,BoneId fore,BoneId hand){
        const float ax=std::sin(arm_angle)*sign, ay=-std::cos(arm_angle);
        setPoint(points,clavicle,{clavicle_half*sign,shoulder_y-.010F*height,0});
        setPoint(points,upper,{shoulder_half*sign,shoulder_y,0});
        setPoint(points,fore,{shoulder_half*sign+ax*upper_length,shoulder_y+ay*upper_length,0});
        setPoint(points,hand,{shoulder_half*sign+ax*(upper_length+fore_length),shoulder_y+ay*(upper_length+fore_length),0});
    };
    build_side(1,BoneId::ClavicleL,BoneId::UpperArmL,BoneId::ForeArmL,BoneId::HandL);
    build_side(-1,BoneId::ClavicleR,BoneId::UpperArmR,BoneId::ForeArmR,BoneId::HandR);

    const float hip_half=.052F*body.hip_width_scale*height;
    const float thigh_y=(body.hip_y-.015F)*height, ankle_y=.045F*height;
    const Vec3 left_thigh{hip_half,thigh_y,0};
    const Vec3 right_thigh{-hip_half,thigh_y,0};
    setPoint(points, BoneId::ThighL, left_thigh);
    const Vec3 left_foot{hip_half,ankle_y,0}, right_foot{-hip_half,ankle_y,0};
    setPoint(points, BoneId::ShinL, mix(left_thigh, left_foot, 0.5F));
    setPoint(points, BoneId::FootL, left_foot);
    setPoint(points, BoneId::ToesL,
             {hip_half,.018F*height,.085F*body.foot_scale*height});
    setPoint(points, BoneId::ThighR, right_thigh);
    setPoint(points, BoneId::ShinR, mix(right_thigh, right_foot, 0.5F));
    setPoint(points, BoneId::FootR, right_foot);
    setPoint(points, BoneId::ToesR,
             {-hip_half,.018F*height,.085F*body.foot_scale*height});

    setPoint(points, BoneId::Jaw, {0,static_cast<float>(.915*height),static_cast<float>(.001*height)});
    setPoint(points, BoneId::MouthUpper,{0,static_cast<float>((layout.mouth_y+.0012)*height),static_cast<float>((layout.mouth_z+.0009)*height)});
    setPoint(points, BoneId::MouthLower,{0,static_cast<float>((layout.mouth_y-.0012)*height),static_cast<float>((layout.mouth_z+.0009)*height)});

    const auto setFaceSide = [&](bool left, BoneId eye, BoneId lid_upper, BoneId lid_lower,
                                 BoneId brow_inner, BoneId brow_outer, BoneId mouth_corner,
                                 BoneId cheek) {
        const std::size_t side=left?0U:1U; const double sign=left?1:-1; const auto& e=layout.eyes[side];
        setPoint(points,eye,{static_cast<float>(e.x*height),static_cast<float>(e.y*height),static_cast<float>(e.z*height)});
        setPoint(points,lid_upper,{static_cast<float>(e.x*height),static_cast<float>((e.y+e.height)*height),static_cast<float>(layout.globeFront(e,e.x,e.y+e.height)*height)});
        setPoint(points,lid_lower,{static_cast<float>(e.x*height),static_cast<float>((e.y-e.height)*height),static_cast<float>(layout.globeFront(e,e.x,e.y-e.height)*height)});
        const auto& br=layout.brows[side];
        setPoint(points,brow_inner,multiply(br.inner,height)); setPoint(points,brow_outer,multiply(br.outer,height));
        const double corner_x=sign*layout.mouth_half, corner_y=layout.mouth_y+sign*face.mouth_asymmetry*.5;
        setPoint(points,mouth_corner,{static_cast<float>(corner_x*height),static_cast<float>(corner_y*height),static_cast<float>((layout.frontZ(corner_x,layout.mouth_y)+.001)*height)});
        const double cheek_x=e.x+sign*.006,cheek_y=face.eye_y_ratio-.016;
        setPoint(points,cheek,{static_cast<float>(cheek_x*height),static_cast<float>(cheek_y*height),static_cast<float>(layout.frontZ(cheek_x,cheek_y)*height)});
    };
    setFaceSide(true, BoneId::EyeL, BoneId::LidUpperL, BoneId::LidLowerL,
                BoneId::BrowInnerL, BoneId::BrowOuterL, BoneId::MouthCornerL, BoneId::CheekL);
    setFaceSide(false, BoneId::EyeR, BoneId::LidUpperR, BoneId::LidLowerR,
                BoneId::BrowInnerR, BoneId::BrowOuterR, BoneId::MouthCornerR, BoneId::CheekR);

    const std::array<BoneId, 5U> left_first{
        BoneId::FingerLLittle0, BoneId::FingerLRing0, BoneId::FingerLMiddle0,
        BoneId::FingerLIndex0, BoneId::FingerLThumb0};
    const std::array<BoneId, 5U> right_first{
        BoneId::FingerRIndex0, BoneId::FingerRMiddle0, BoneId::FingerRRing0,
        BoneId::FingerRLittle0, BoneId::FingerRThumb0};
    const std::array<float, 4U> offsets{-1.5F, -0.5F, 0.5F, 1.5F};
    const std::array<float, 4U> left_lengths{.029F,.039F,.042F,.033F};
    const std::array<float, 4U> right_lengths{.033F,.042F,.039F,.029F};
    for (std::size_t index = 0U; index < 5U; ++index) {
        buildFinger(points, left_first[index], points[boneIndex(BoneId::HandL)], points[boneIndex(BoneId::ForeArmL)],
                    index == 4U ? .017F : offsets[index] * .0101F, index == 4U,
                    height, body.hand_scale, index == 4U ? .034F : left_lengths[index]);
        buildFinger(points, right_first[index], points[boneIndex(BoneId::HandR)],
                    points[boneIndex(BoneId::ForeArmR)], index == 4U ? -.017F : offsets[index] * .0101F,
                    index == 4U, height, body.hand_scale,
                    index == 4U ? .034F : right_lengths[index]);
    }

    SkeletonData result{};
    result.schema_version_ = RigSchemaVersion;
    result.cache_key_ = hashInput(body, face);
    result.bones_.reserve(kRigBoneCount);
    for (std::size_t index = 0U; index < kRigBoneCount; ++index) {
        const BoneSchema& schema = kCanonicalRigSchema[index];
        BoneRecord record{};
        record.id = schema.id;
        record.name = schema.name;
        record.parent = schema.parent;
        record.category = schema.category;
        record.local_bind.translation = schema.parent == kInvalidBoneIndex
                                            ? points[index]
                                            : subtract(points[index], points[schema.parent]);
        record.local_bind.rotation = RigQuaternion::identity();
        record.local_bind.scale = {1.0F, 1.0F, 1.0F};
        record.world_bind = schema.parent == kInvalidBoneIndex
                                ? record.local_bind
                                : compose(result.bones_[schema.parent].world_bind, record.local_bind);
        record.inverse_bind = inverse(record.world_bind);
        record.reference_length = schema.parent == kInvalidBoneIndex
                                      ? 0.0F
                                      : length(record.local_bind.translation);
        result.bones_.push_back(record);
    }

    const auto addAttachment = [&result, &points](AttachmentPointId id, BoneId bone) {
        result.attachments_.push_back({id, bone, {}, points[boneIndex(bone)]});
    };
    addAttachment(AttachmentPointId::Pelvis, BoneId::Hips);
    addAttachment(AttachmentPointId::Chest, BoneId::Chest);
    addAttachment(AttachmentPointId::Head, BoneId::Head);
    addAttachment(AttachmentPointId::Jaw, BoneId::Jaw);
    addAttachment(AttachmentPointId::LeftHand, BoneId::HandL);
    addAttachment(AttachmentPointId::RightHand, BoneId::HandR);
    addAttachment(AttachmentPointId::LeftFoot, BoneId::FootL);
    addAttachment(AttachmentPointId::RightFoot, BoneId::FootR);
    addAttachment(AttachmentPointId::LeftEye, BoneId::EyeL);
    addAttachment(AttachmentPointId::RightEye, BoneId::EyeR);
    addAttachment(AttachmentPointId::Mouth, BoneId::MouthUpper);

    return result.valid()
               ? foundation::Result<SkeletonData, foundation::Error>::success(std::move(result))
               : foundation::Result<SkeletonData, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "infantry rig constraints are infeasible"});
}

} // namespace genomes::infantry
