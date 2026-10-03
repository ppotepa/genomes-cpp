#include <genomes/infantry/GearSurfaceGenerator.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
[[nodiscard]] double jsSubtract(float left,float right) noexcept {
    volatile double value=static_cast<double>(left)-static_cast<double>(right);return value;
}
void addFloat32(float& accumulator,double value) noexcept {
    volatile double sum=static_cast<double>(accumulator)+value;
    accumulator=static_cast<float>(sum);
}
[[nodiscard]] double jsLength(Vec3 value) noexcept {
    volatile double xx=static_cast<double>(value.x)*value.x;
    volatile double yy=static_cast<double>(value.y)*value.y;
    volatile double zz=static_cast<double>(value.z)*value.z;
    volatile double xy=static_cast<double>(xx)+static_cast<double>(yy);
    return std::sqrt(static_cast<double>(xy)+static_cast<double>(zz));
}

struct DVec3 final { double x{},y{},z{}; };
struct DSection final { double y{},rx{},rz{},z{}; };
struct DQuaternion final { double x{},y{},z{},w{1.0}; };

[[nodiscard]] DVec3 rotate(DVec3 value,DQuaternion quaternion) noexcept {
    const double ix=quaternion.w*value.x+quaternion.y*value.z-quaternion.z*value.y;
    const double iy=quaternion.w*value.y+quaternion.z*value.x-quaternion.x*value.z;
    const double iz=quaternion.w*value.z+quaternion.x*value.y-quaternion.y*value.x;
    const double iw=-quaternion.x*value.x-quaternion.y*value.y-quaternion.z*value.z;
    return {ix*quaternion.w+iw*-quaternion.x+iy*-quaternion.z-iz*-quaternion.y,
        iy*quaternion.w+iw*-quaternion.y+iz*-quaternion.x-ix*-quaternion.z,
        iz*quaternion.w+iw*-quaternion.z+ix*-quaternion.y-iy*-quaternion.x};
}

[[nodiscard]] DQuaternion fromZ(DVec3 normal) noexcept {
    const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
    const double inverse_length=1.0/length;
    normal={normal.x*inverse_length,normal.y*inverse_length,normal.z*inverse_length};if(normal.z<-.999999)return {0.0,1.0,0.0,0.0};
    DQuaternion result{-normal.y,normal.x,0.0,1.0+normal.z};
    const double q_length=std::sqrt(result.x*result.x+result.y*result.y+result.z*result.z+result.w*result.w);
    const double inverse_q_length=1.0/q_length;
    result.x*=inverse_q_length;result.y*=inverse_q_length;result.z*=inverse_q_length;result.w*=inverse_q_length;return result;
}

[[nodiscard]] double referenceVariantSize(std::uint32_t seed) noexcept {
    std::uint32_t state=seed+0x6D2B79F5U;
    std::uint32_t value=state;
    value=(value^(value>>15U))*(value|1U);
    value^=value+(value^(value>>7U))*(value|61U);
    const double random=static_cast<double>(value^(value>>14U))/4294967296.0;
    return .965+random*.07;
}

[[nodiscard]] DVec3 referencePackDimensions(std::string_view identifier,
                                             DVec3 fallback) noexcept {
    if(identifier=="pack_small")return {.26,.32,.13};
    if(identifier=="pack_medium")return {.32,.43,.19};
    if(identifier=="pack_large")return {.37,.53,.22};
    if(identifier=="pack_medical")return {.35,.42,.19};
    if(identifier=="pack_radio")return {.31,.39,.18};
    if(identifier=="pack_engineer")return {.33,.43,.20};
    return fallback;
}

[[nodiscard]] double referenceArmorThickness(std::string_view identifier,
                                              double fallback) noexcept {
    if(identifier=="light_vest")return .010;
    if(identifier=="plate_carrier")return .020;
    if(identifier=="heavy_armor")return .029;
    return fallback;
}

[[nodiscard]] DVec3 referenceJacketProfile(const EquipmentFit& fit,double y) {
    for(std::size_t index=0U;index+1U<fit.jacket.size();++index)if(y<=fit.jacket[index+1U].reference_y){
        const auto& a=fit.jacket[index];const auto& b=fit.jacket[index+1U];
        const double amount=std::clamp((y-a.reference_y)/(b.reference_y-a.reference_y),0.0,1.0);
        return {a.reference_half_width+(b.reference_half_width-a.reference_half_width)*amount,
            a.reference_half_depth+(b.reference_half_depth-a.reference_half_depth)*amount,0.0};}
    return {fit.jacket.back().reference_half_width,fit.jacket.back().reference_half_depth,0.0};
}

class ReferenceHeadProfile final {
public:
    explicit ReferenceHeadProfile(const EquipmentFit& fit):face_(fit.face.reference) {
        const auto smooth=[](double value){value=std::clamp(value,0.0,1.0);return value*value*(3.0-2.0*value);};
        const auto bell=[](double value){return std::exp(-value*value);};
        const auto blend=[](double a,double b,double t){return a+(b-a)*t;};
        constexpr std::array<DSection,12U> base{{
            {.881,.034,.030,.013},{.890,.039,.037,.010},{.900,.041,.041,.008},
            {.913,.045,.044,.005},{.925,.049,.046,.003},{.938,.048,.046,.002},
            {.948,.047,.046,.001},{.959,.0465,.046,0},{.973,.043,.043,-.001},
            {.986,.032,.034,-.003},{.995,.015,.018,-.004},{.999,0,0,-.004}}};
        std::array<DSection,12U> shaped{};
        for(std::size_t index=0;index<base.size();++index){const double y0=base[index].y;
            const double low=smooth(std::clamp((.928-y0)/.058,0.0,1.0));
            const double temple=bell((y0-.951)/.020),forehead=smooth((y0-.948)/.038),chin=bell((y0-.887)/.015);
            double y=y0;if(y0<.925)y=.925+(y0-.925)*face_.jaw_length_scale+face_.chin_height*chin*.65;
            if(y0>.947)y=.999-(.999-y0)*face_.head_length_scale;
            double scale=blend(1.0,face_.jaw_width_scale,low)*blend(1.0,face_.jaw_angle,low*low*.40);
            scale=blend(scale,face_.temple_width_scale,temple*.55);
            scale=blend(scale,face_.forehead_width_scale,forehead*.65);
            shaped[index]={y,base[index].rx*scale*face_.head_width_scale*blend(1.0,fit.body.reference_head_scale,.4),
                base[index].rz*face_.head_depth_scale*blend(1.0,fit.body.reference_head_scale,.4),base[index].z};}
        shaped[0].y=std::clamp(shaped[0].y,.8755,.8855);
        for(std::size_t index=1;index<shaped.size();++index)shaped[index].y=std::max(shaped[index].y,shaped[index-1U].y+.003);
        const double neck_x=.0285*fit.body.reference_neck_scale,neck_z=.024*fit.body.reference_neck_scale;
        levels_.reserve(19U);levels_.push_back({.828,neck_x*1.14,neck_z*1.12,-.002});
        levels_.push_back({.841,neck_x*1.06,neck_z*1.04,-.002});levels_.push_back({.853,neck_x,neck_z,-.002});
        for(const double t:{.2,.4,.6,.8}){const double amount=smooth(t);levels_.push_back({blend(.853,shaped[0].y,t),
            blend(neck_x*.99,shaped[0].rx,amount),blend(neck_z*1.01,shaped[0].rz,amount),blend(.001,shaped[0].z,amount)});}
        levels_.insert(levels_.end(),shaped.begin(),shaped.end());chin_y_=levels_[7U].y;top_y=levels_.back().y;
        const double eye_w=.0083*face_.eye_width_scale,eye_h=.0034*face_.eye_height_scale;
        const double radius=std::max(eye_w*1.12,eye_h*1.65)*.82;
        const double minimum=radius*1.20+.003,maximum=std::max(minimum,section(face_.eye_y).rx*.66);
        const double spacing=std::clamp(face_.eye_spacing,minimum,maximum);
        hair_floor_=0.0;
        for(const double sign:{1.0,-1.0}){const double eye_y=face_.eye_y+sign*face_.eye_asymmetry*.5;
            const double brow_y=std::max(face_.brow_y+sign*face_.brow_asymmetry*.5,eye_y+eye_h+.006);
            hair_floor_=std::max({hair_floor_,brow_y,brow_y+face_.brow_tilt*.006});
            (void)spacing;}
        hair_floor_+=.006;
    }

    [[nodiscard]] DSection section(double y) const {
        std::size_t lower=0U;while(lower+1U<levels_.size()&&levels_[lower+1U].y<y)++lower;
        if(lower+1U>=levels_.size())return {y,0.0,0.0,levels_.back().z};
        const double span=levels_[lower+1U].y-levels_[lower].y;
        const double t=std::clamp((y-levels_[lower].y)/span,0.0,1.0),t2=t*t,t3=t2*t;
        const auto component=[](const DSection& value,std::size_t field){return field==0U?value.y:field==1U?value.rx:field==2U?value.rz:value.z;};
        const auto slope=[&](std::size_t at,std::size_t field){if(at==0U)return(component(levels_[1U],field)-component(levels_[0U],field))/(levels_[1U].y-levels_[0U].y);
            if(at+1U==levels_.size())return(component(levels_[at],field)-component(levels_[at-1U],field))/(levels_[at].y-levels_[at-1U].y);
            const double left=(component(levels_[at],field)-component(levels_[at-1U],field))/(levels_[at].y-levels_[at-1U].y);
            const double right=(component(levels_[at+1U],field)-component(levels_[at],field))/(levels_[at+1U].y-levels_[at].y);
            return left*right<=0.0?0.0:2.0*left*right/(left+right);};
        const auto value=[&](std::size_t field){const double a=component(levels_[lower],field),b=component(levels_[lower+1U],field);
            return(2*t3-3*t2+1)*a+(t3-2*t2+t)*slope(lower,field)*span+(-2*t3+3*t2)*b+(t3-t2)*slope(lower+1U,field)*span;};
        return {y,std::max(0.0,value(1U)),std::max(0.0,value(2U)),value(3U)};
    }

    [[nodiscard]] DVec3 point(double y,double theta) const {
        const auto profile=section(y);const double sx=std::sin(theta),cz=std::cos(theta);
        const auto smooth=[](double value){value=std::clamp(value,0.0,1.0);return value*value*(3.0-2.0*value);};
        const auto bell=[](double x,double yy=0.0){return std::exp(-(x*x+yy*yy));};
        const double chin=bell((y-(chin_y_+.014))/.020),side=smooth((std::abs(sx)-.10)/.72);
        const double x=profile.rx*sx*(1.0+(face_.chin_width_scale-1.0)*.30*chin*side);double z=profile.z+profile.rz*cz;
        if(cz>0.0&&y>.882){const double front=smooth((cz-.02)/.48);
            const double cheek=bell((std::abs(x)-.030*face_.cheekbone_scale)/.014,(y-(.925+face_.cheekbone_y))/.016);
            z+=(.0014+face_.cheek_fullness*.42+(face_.cheekbone_scale-1.0)*.0018)*cheek*front;
            z+=face_.chin_projection*.30*bell(x/.023,(y-(chin_y_+.014))/.020)*front;
            z+=face_.brow_ridge*bell((std::abs(x)-face_.eye_spacing)/.017,(y-(face_.eye_y+.010))/.009)*cz;
            z+=face_.midface_projection*.38*bell(x/.034,(y-.918)/.024)*cz;
            z+=face_.forehead_slope*smooth((y-.947)/.045)*cz;}
        return {x,y,z};
    }

    [[nodiscard]] double frontZ(double x,double y,const EquipmentFit& fit) const {
        const auto profile=section(y);const auto& face=fit.face.reference;
        const double sx=std::clamp(x/std::max(.00001,profile.rx),-.9999,.9999);
        const double cz=std::sqrt(std::max(0.0,1.0-sx*sx));
        const auto smooth=[](double value){value=std::clamp(value,0.0,1.0);return value*value*(3.0-2.0*value);};
        const auto bell=[](double a,double b=0.0){return std::exp(-a*a-b*b);};
        const double chin=bell((y-(chin_y_+.014))/.020);
        const double side=smooth((std::abs(sx)-.10)/.72);
        const double px=profile.rx*sx*(1.0+(face.chin_width_scale-1.0)*.30*chin*side);
        double z=profile.z+profile.rz*cz;
        if(cz>0.0&&y>.882){
            const double front=smooth((cz-.02)/.48);
            const double cheek=bell((std::abs(px)-.030*face.cheekbone_scale)/.014,
                                    (y-(.925+face.cheekbone_y))/.016);
            z+=(.0014+face.cheek_fullness*.42+(face.cheekbone_scale-1.0)*.0018)*cheek*front;
            z+=face.chin_projection*.30*bell(px/.023,(y-(chin_y_+.014))/.020)*front;
            z+=face.brow_ridge*bell((std::abs(px)-face.eye_spacing)/.017,
                                    (y-(face.eye_y+.010))/.009)*cz;
            z+=face.midface_projection*.38*bell(px/.034,(y-.918)/.024)*cz;
            z+=face.forehead_slope*smooth((y-.947)/.045)*cz;
        }
        return z;
    }

    [[nodiscard]] double noseBaseY() const noexcept {
        const double mouth_y=std::clamp(face_.mouth_y,chin_y_+.014,face_.eye_y-.031);
        return std::max(face_.eye_y-.031*face_.nose_length_scale,mouth_y+.010);
    }

    [[nodiscard]] double headBottom(double theta,const EquipmentFit& fit,bool helmet) const {
        const double front=std::max(0.0,std::cos(theta)),side=std::abs(std::sin(theta));
        if(helmet){const double side_amount=fit.headgear_visual.style=="light"?.014:.006;
            return std::max(.926+front*.031+side*side_amount,hair_floor_*front+.929*(1.0-front));}
        return std::max(.949-std::max(0.0,-std::cos(theta))*.009,hair_floor_+.001);
    }

    double top_y{};
private:
    const ReferenceFaceParameters& face_;
    std::vector<DSection> levels_{};
    double chin_y_{},hair_floor_{};
};
[[nodiscard]] std::uint32_t tubeSegments(std::uint32_t detail_level) noexcept {
    return detail_level == 3U ? 8U : detail_level == 1U ? 4U : 6U;
}

struct Quaternion final { float x{0.0F}, y{0.0F}, z{0.0F}, w{1.0F}; };

[[nodiscard]] Vec3 rotate(Vec3 value, Quaternion quaternion) noexcept {
    const Vec3 q{quaternion.x, quaternion.y, quaternion.z};
    const Vec3 twice_cross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {value.x + quaternion.w * twice_cross.x +
                (q.y * twice_cross.z - q.z * twice_cross.y),
            value.y + quaternion.w * twice_cross.y +
                (q.z * twice_cross.x - q.x * twice_cross.z),
            value.z + quaternion.w * twice_cross.z +
                (q.x * twice_cross.y - q.y * twice_cross.x)};
}

[[nodiscard]] Quaternion fromZ(Vec3 normal) noexcept {
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    normal = {normal.x / length, normal.y / length, normal.z / length};
    if (normal.z < -0.999999F) return {0.0F, 1.0F, 0.0F, 0.0F};
    Quaternion result{-normal.y, normal.x, 0.0F, 1.0F + normal.z};
    const float q_length = std::sqrt(result.x * result.x + result.y * result.y +
                                     result.z * result.z + result.w * result.w);
    result.x /= q_length; result.y /= q_length; result.z /= q_length; result.w /= q_length;
    return result;
}

[[nodiscard]] foundation::Color tint(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] float srgbToLinear(float value) noexcept {
    return value <= .04045F ? value / 12.92F
                            : std::pow((value + .055F) / 1.055F, 2.4F);
}

[[nodiscard]] foundation::Color rgb(std::uint32_t hex) noexcept {
    return {srgbToLinear(static_cast<float>((hex >> 16U) & 0xffU) / 255.0F),
            srgbToLinear(static_cast<float>((hex >> 8U) & 0xffU) / 255.0F),
            srgbToLinear(static_cast<float>(hex & 0xffU) / 255.0F), 1.0F};
}

[[nodiscard]] foundation::Color clothColor(const GearArtifact& artifact,
                                            float variant_shade,
                                            float factor = 1.0F) noexcept {
    foundation::Color result{
        std::min(1.0F, artifact.palette.r * variant_shade * factor),
        std::min(1.0F, artifact.palette.g * variant_shade * factor),
        std::min(1.0F, artifact.palette.b * variant_shade * factor), 1.0F};
    const float fade = .018F * artifact.wear;
    result.r += (1.0F - result.r) * fade;
    result.g += (1.0F - result.g) * fade;
    result.b += (1.0F - result.b) * fade;
    return result;
}

[[nodiscard]] std::array<SkinInfluence, 4U> boneWeight(const GearPiece& piece,
                                                         std::uint8_t& count) noexcept {
    count = 1U;
    return {SkinInfluence{static_cast<std::uint16_t>(boneIndex(piece.bone)), 1.0F},
            {}, {}, {}};
}

void appendBox(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
               Vec3 dimensions, foundation::Color color, std::uint32_t region,
               std::uint32_t detail_level, const Quaternion* rotation = nullptr,
               std::span<const SkinInfluence> override_weights = {},
               const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    constexpr std::array<std::array<int, 2U>, 6U> faces{{
        {{0, 1}}, {{0, -1}}, {{1, 1}}, {{1, -1}}, {{2, 1}}, {{2, -1}}}};
    const Vec3 half{dimensions.x * 0.5F, dimensions.y * 0.5F, dimensions.z * 0.5F};
    const float radius = std::min({half.x, half.y, half.z}) * .4F;
    const Vec3 core{half.x - radius, half.y - radius, half.z - radius};
    const std::uint32_t segments = detail_level == 3U ? 4U : detail_level == 1U ? 1U : 2U;
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights = override_weights.empty()
        ? std::span<const SkinInfluence>(influences.data(), count) : override_weights;
    const auto component = [](Vec3& value, int axis) -> float& {
        return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
    };
    const auto read = [](Vec3 value, int axis) {
        return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
    };
    for (const auto face : faces) {
        const int axis = face[0], sign = face[1];
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        std::vector<std::vector<std::uint32_t>> loops(segments + 1U,
                                                      std::vector<std::uint32_t>(segments + 1U));
        for (std::uint32_t row = 0U; row <= segments; ++row) {
            for (std::uint32_t column = 0U; column <= segments; ++column) {
                Vec3 raw{};
                component(raw, axis) = read(half, axis) * static_cast<float>(sign);
                component(raw, u) = (static_cast<float>(column) / static_cast<float>(segments) * 2.0F - 1.0F) * read(half, u);
                component(raw, v) = (static_cast<float>(row) / static_cast<float>(segments) * 2.0F - 1.0F) * read(half, v);
                Vec3 point{std::clamp(raw.x, -core.x, core.x),
                           std::clamp(raw.y, -core.y, core.y),
                           std::clamp(raw.z, -core.z, core.z)};
                Vec3 normal{raw.x - point.x, raw.y - point.y, raw.z - point.z};
                const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                normal = {normal.x / length, normal.y / length, normal.z / length};
                point = {point.x + normal.x * radius,
                         point.y + normal.y * radius,
                         point.z + normal.z * radius};
                if (rotation != nullptr) {
                    point = rotate(point, *rotation);
                    normal = rotate(normal, *rotation);
                }
                point = {point.x + center.x, point.y + center.y, point.z + center.z};
                NormalizedInfluences dynamic_weights{};
                std::span<const SkinInfluence> vertex_weights = weights;
                if (weight_function != nullptr) {
                    dynamic_weights = (*weight_function)(point);
                    vertex_weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                                    dynamic_weights.count);
                }
                loops[row][column] = builder.appendVertex({point, normal,
                    {static_cast<float>(column) / static_cast<float>(segments),
                     static_cast<float>(row) / static_cast<float>(segments)},
                    color, static_cast<std::uint16_t>(region), vertex_weights});
            }
        }
        for (std::uint32_t row = 0U; row < segments; ++row) {
            for (std::uint32_t column = 0U; column < segments; ++column) {
                builder.triangle(loops[row][column], loops[row + 1U][column],
                                 loops[row][column + 1U]);
                builder.triangle(loops[row][column + 1U], loops[row + 1U][column],
                                 loops[row + 1U][column + 1U]);
            }
        }
    }
}

void bridge(AppearanceMeshBuilder& builder, std::span<const std::uint32_t> first,
            std::span<const std::uint32_t> second);

void appendEllipsoid(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
                     Vec3 radii, foundation::Color color, std::uint32_t region,
                     std::uint32_t segments = 20U, std::uint32_t rows = 12U,
                     const Quaternion* rotation = nullptr) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    const auto vertex = [&builder, color, region, weights](Vec3 position, Vec3 normal,
                                                            Vec2 uv) {
        return builder.appendVertex({position, normal, uv,
                                     color, static_cast<std::uint16_t>(region), weights});
    };
    std::vector<std::vector<std::uint32_t>> rings;
    rings.reserve(rows + 1U);
    for (std::uint32_t row = 0U; row <= rows; ++row) {
        const float phi = -1.57079632679F +
            3.14159265359F * static_cast<float>(row) / static_cast<float>(rows);
        const float cp = std::cos(phi);
        const float sp = std::sin(phi);
        std::vector<std::uint32_t> current;
        current.reserve(segments);
        for (std::uint32_t segment = 0U; segment < segments; ++segment) {
            const float angle = 6.28318530718F * static_cast<float>(segment) /
                                static_cast<float>(segments);
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);
            Vec3 point{radii.x * cp * ca, radii.y * sp, radii.z * cp * sa};
            Vec3 normal{cp * ca / radii.x, sp / radii.y, cp * sa / radii.z};
            const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            normal = {normal.x / length, normal.y / length, normal.z / length};
            if (rotation != nullptr) { point = rotate(point, *rotation); normal = rotate(normal, *rotation); }
            current.push_back(vertex({center.x + point.x, center.y + point.y,
                                      center.z + point.z}, normal,
                                     {static_cast<float>(segment) / static_cast<float>(segments),
                                      static_cast<float>(row) / static_cast<float>(rows)}));
        }
        if (!rings.empty()) bridge(builder, rings.back(), current);
        rings.push_back(std::move(current));
    }
}

void appendReferenceEllipsoid(AppearanceMeshBuilder& builder,const GearPiece& piece,
                              DVec3 center,DVec3 radii,foundation::Color color,
                              std::uint32_t region,std::uint32_t segments,std::uint32_t rows,
                              std::unordered_map<std::uint32_t,DVec3>& reference_positions,
                              const DQuaternion* rotation=nullptr) {
    std::uint8_t count=0U;const auto influences=boneWeight(piece,count);
    const std::span<const SkinInfluence> weights(influences.data(),count);
    constexpr double pi=3.1415926535897932384626433832795;
    std::vector<std::vector<std::uint32_t>> rings;rings.reserve(rows+1U);
    for(std::uint32_t row=0U;row<=rows;++row){
        const double phi=-pi*.5+pi*static_cast<double>(row)/rows;
        const double cp=std::cos(phi),sp=std::sin(phi);
        std::vector<std::uint32_t> current;current.reserve(segments);
        for(std::uint32_t segment=0U;segment<segments;++segment){
            const double angle=2.0*pi*static_cast<double>(segment)/segments;
            const double ca=std::cos(angle),sa=std::sin(angle);
            DVec3 point{radii.x*cp*ca,radii.y*sp,radii.z*cp*sa};
            DVec3 normal{cp*ca/radii.x,sp/radii.y,cp*sa/radii.z};
            const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
            normal={normal.x/length,normal.y/length,normal.z/length};
            if(rotation!=nullptr){point=rotate(point,*rotation);normal=rotate(normal,*rotation);}
            point={center.x+point.x,center.y+point.y,center.z+point.z};
            const auto vertex=builder.appendVertex({
                {static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
                {static_cast<float>(static_cast<double>(segment)/segments),
                 static_cast<float>(static_cast<double>(row)/rows)},color,
                static_cast<std::uint16_t>(region),weights});
            reference_positions.emplace(vertex,point);current.push_back(vertex);
        }
        if(!rings.empty())bridge(builder,rings.back(),current);
        rings.push_back(std::move(current));
    }
}

void appendRibbon(AppearanceMeshBuilder& builder, const GearPiece& piece,
                  std::span<const Vec3> points, float width,
                  foundation::Color color, std::uint32_t region,
                  Vec3 axis = {1.0F, 0.0F, 0.0F},
                  const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    std::array<std::uint32_t, 2U> previous{};
    bool has_previous = false;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Vec3 before = points[index == 0U ? 0U : index - 1U];
        const Vec3 after = points[std::min(index + 1U, points.size() - 1U)];
        Vec3 tangent{after.x - before.x, after.y - before.y, after.z - before.z};
        const float tangent_length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tangent_length > 0.0F) tangent = {tangent.x / tangent_length, tangent.y / tangent_length, tangent.z / tangent_length};
        Vec3 normal{axis.y * tangent.z - axis.z * tangent.y,
                    axis.z * tangent.x - axis.x * tangent.z,
                    axis.x * tangent.y - axis.y * tangent.x};
        const float normal_length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (normal_length > 0.0F) normal = {normal.x / normal_length, normal.y / normal_length, normal.z / normal_length};
        NormalizedInfluences dynamic_weights{};
        std::span<const SkinInfluence> vertex_weights = weights;
        if (weight_function != nullptr) {
            dynamic_weights = (*weight_function)(points[index]);
            vertex_weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                             dynamic_weights.count);
        }
        const std::array<std::uint32_t, 2U> edge{
            builder.appendVertex({{points[index].x - axis.x * width * .5F,
                                   points[index].y - axis.y * width * .5F,
                                   points[index].z - axis.z * width * .5F},
                                  normal, {0.0F, static_cast<float>(index)}, color,
                                  static_cast<std::uint16_t>(region), vertex_weights}),
            builder.appendVertex({{points[index].x + axis.x * width * .5F,
                                   points[index].y + axis.y * width * .5F,
                                   points[index].z + axis.z * width * .5F},
                                  normal, {1.0F, static_cast<float>(index)}, color,
                                  static_cast<std::uint16_t>(region), vertex_weights})};
        if (has_previous) {
            builder.triangle(previous[0], edge[0], previous[1]);
            builder.triangle(previous[1], edge[0], edge[1]);
        }
        previous = edge;
        has_previous = true;
    }
}

void appendReferenceRibbon(AppearanceMeshBuilder& builder,const GearPiece& piece,
                           std::span<const DVec3> points,double width,
                           foundation::Color color,std::uint32_t region,
                           std::unordered_map<std::uint32_t,DVec3>& reference_positions) {
    std::uint8_t count=0U;const auto influences=boneWeight(piece,count);
    const std::span<const SkinInfluence> weights(influences.data(),count);
    constexpr DVec3 axis{1.0,0.0,0.0};
    std::array<std::uint32_t,2U> previous{};bool has_previous=false;
    for(std::size_t index=0U;index<points.size();++index){
        const auto& before=points[index==0U?0U:index-1U];
        const auto& after=points[std::min(index+1U,points.size()-1U)];
        DVec3 tangent{after.x-before.x,after.y-before.y,after.z-before.z};
        const double tangent_length=std::sqrt(tangent.x*tangent.x+tangent.y*tangent.y+tangent.z*tangent.z);
        if(tangent_length>0.0)tangent={tangent.x/tangent_length,tangent.y/tangent_length,tangent.z/tangent_length};
        DVec3 normal{axis.y*tangent.z-axis.z*tangent.y,axis.z*tangent.x-axis.x*tangent.z,
                     axis.x*tangent.y-axis.y*tangent.x};
        const double normal_length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
        if(normal_length>0.0)normal={normal.x/normal_length,normal.y/normal_length,normal.z/normal_length};
        const DVec3 offsets[2]{{-axis.x*width*.5,-axis.y*width*.5,-axis.z*width*.5},
                               {axis.x*width*.5,axis.y*width*.5,axis.z*width*.5}};
        std::array<std::uint32_t,2U> edge{};
        for(std::size_t side=0U;side<2U;++side){
            const DVec3 point{points[index].x+offsets[side].x,points[index].y+offsets[side].y,
                              points[index].z+offsets[side].z};
            edge[side]=builder.appendVertex({{static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
                {static_cast<float>(side),static_cast<float>(index)},color,
                static_cast<std::uint16_t>(region),weights});
            reference_positions.emplace(edge[side],point);
        }
        if(has_previous){builder.triangle(previous[0],edge[0],previous[1]);
            builder.triangle(previous[1],edge[0],edge[1]);}
        previous=edge;has_previous=true;
    }
}

[[nodiscard]] NormalizedInfluences neckWeights(const EquipmentFit& fit, float y);

[[nodiscard]] NormalizedInfluences torsoWeights(const EquipmentFit& fit, Vec3 point) {
    const float spine_lower = fit.bind_points[boneIndex(BoneId::SpineLower)].y;
    const float spine_upper = fit.bind_points[boneIndex(BoneId::SpineUpper)].y;
    const float chest = fit.bind_points[boneIndex(BoneId::Chest)].y;
    std::array<SkinInfluence, 2U> values{};
    if (point.y < spine_lower) {
        const float t = std::clamp((point.y - fit.hip_y) /
            std::max(.001F, spine_lower - fit.hip_y), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::Hips)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)), eased}}};
    } else if (point.y < spine_upper) {
        const float t = std::clamp((point.y - spine_lower) /
            std::max(.001F, spine_upper - spine_lower), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)), eased}}};
    } else if (point.y < chest) {
        const float t = std::clamp((point.y - spine_upper) /
            std::max(.001F, chest - spine_upper), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::Chest)), eased}}};
    } else {
        if(point.y>.832F)return neckWeights(fit,point.y);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)), 1.0F}, {}}};
    }
    return normalizeTopFour(values);
}

[[nodiscard]] NormalizedInfluences neckWeights(const EquipmentFit& fit, float y) {
    const float head_pivot=fit.bind_points[boneIndex(BoneId::Head)].y;
    const float neck_joint=fit.bind_points[boneIndex(BoneId::Neck)].y;
    const auto eased=[](float value){value=std::clamp(value,0.0F,1.0F);return value*value*(3.0F-2.0F*value);};
    const float head=eased((y-(head_pivot-.022F))/.030F);
    const float chest=(1.0F-eased((y-(neck_joint-.010F))/.030F))*(1.0F-head);
    const std::array<SkinInfluence,3U> values{{
        {static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),chest},
        {static_cast<std::uint16_t>(boneIndex(BoneId::Neck)),1.0F-head-chest},
        {static_cast<std::uint16_t>(boneIndex(BoneId::Head)),head}}};
    return normalizeTopFour(values);
}

[[nodiscard]] std::vector<std::uint32_t> appendRing(
    AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
    Vec3 u, Vec3 v, float radius_x, float radius_z, std::uint32_t segments,
    foundation::Color color, std::uint32_t region, float uv_y = 0.0F,
    std::span<const SkinInfluence> override_weights = {}) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights = override_weights.empty()
        ? std::span<const SkinInfluence>(influences.data(), count) : override_weights;
    std::vector<std::uint32_t> ring;
    ring.reserve(segments);
    for (std::uint32_t index = 0U; index < segments; ++index) {
        const float angle = 6.28318530717958647692F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const float xx = std::cos(angle), zz = std::sin(angle);
        const Vec3 position{center.x + u.x * radius_x * xx + v.x * radius_z * zz,
                            center.y + u.y * radius_x * xx + v.y * radius_z * zz,
                            center.z + u.z * radius_x * xx + v.z * radius_z * zz};
        Vec3 normal{u.x * xx / std::max(radius_x, 1.0e-5F) +
                        v.x * zz / std::max(radius_z, 1.0e-5F),
                    u.y * xx / std::max(radius_x, 1.0e-5F) +
                        v.y * zz / std::max(radius_z, 1.0e-5F),
                    u.z * xx / std::max(radius_x, 1.0e-5F) +
                        v.z * zz / std::max(radius_z, 1.0e-5F)};
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        normal = {normal.x / length, normal.y / length, normal.z / length};
        ring.push_back(builder.appendVertex({position, normal,
            {static_cast<float>(index) / static_cast<float>(segments) * 3.0F, uv_y},
            color, static_cast<std::uint16_t>(region), weights}));
    }
    return ring;
}

[[nodiscard]] std::vector<std::uint32_t> appendReferenceRing(
    AppearanceMeshBuilder& builder,const GearPiece& piece,DVec3 center,DVec3 u,DVec3 v,
    double radius_x,double radius_z,std::uint32_t segments,foundation::Color color,
    std::uint32_t region,std::unordered_map<std::uint32_t,DVec3>& reference_positions,
    float uv_y=0.0F) {
    std::uint8_t count=0U;const auto influences=boneWeight(piece,count);
    const std::span<const SkinInfluence> weights(influences.data(),count);
    std::vector<std::uint32_t> ring;ring.reserve(segments);
    for(std::uint32_t index=0U;index<segments;++index){
        const double angle=6.283185307179586476925286766559*static_cast<double>(index)/segments;
        const double xx=std::cos(angle),zz=std::sin(angle);
        const DVec3 point{center.x+u.x*radius_x*xx+v.x*radius_z*zz,
            center.y+u.y*radius_x*xx+v.y*radius_z*zz,
            center.z+u.z*radius_x*xx+v.z*radius_z*zz};
        DVec3 normal{u.x*xx/std::max(radius_x,1.0e-5)+v.x*zz/std::max(radius_z,1.0e-5),
            u.y*xx/std::max(radius_x,1.0e-5)+v.y*zz/std::max(radius_z,1.0e-5),
            u.z*xx/std::max(radius_x,1.0e-5)+v.z*zz/std::max(radius_z,1.0e-5)};
        const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
        normal={normal.x/length,normal.y/length,normal.z/length};
        const auto vertex=builder.appendVertex({{static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
            {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
            {static_cast<float>(static_cast<double>(index)/segments*3.0),uv_y},color,
            static_cast<std::uint16_t>(region),weights});
        reference_positions.emplace(vertex,point);ring.push_back(vertex);
    }
    return ring;
}

void appendReferenceCylinder(AppearanceMeshBuilder& builder,const GearPiece& piece,
                             DVec3 first_point,DVec3 second_point,double radius,
                             foundation::Color color,std::uint32_t region,
                             std::uint32_t segments,
                             std::unordered_map<std::uint32_t,DVec3>& reference_positions) {
    DVec3 direction{second_point.x-first_point.x,second_point.y-first_point.y,
                     second_point.z-first_point.z};
    const double direction_length=std::sqrt(direction.x*direction.x+direction.y*direction.y+
                                             direction.z*direction.z);
    direction={direction.x/direction_length,direction.y/direction_length,direction.z/direction_length};
    const DVec3 guide=std::abs(direction.z)<.9?DVec3{0.0,0.0,1.0}:DVec3{0.0,1.0,0.0};
    DVec3 u{direction.y*guide.z-direction.z*guide.y,
            direction.z*guide.x-direction.x*guide.z,
            direction.x*guide.y-direction.y*guide.x};
    const double u_length=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
    u={u.x/u_length,u.y/u_length,u.z/u_length};
    const DVec3 v{u.y*direction.z-u.z*direction.y,
                  u.z*direction.x-u.x*direction.z,
                  u.x*direction.y-u.y*direction.x};
    const auto first=appendReferenceRing(builder,piece,first_point,u,v,radius,radius,segments,
                                         color,region,reference_positions);
    const auto second=appendReferenceRing(builder,piece,second_point,u,v,radius,radius,segments,
                                          color,region,reference_positions);
    bridge(builder,first,second);
    std::uint8_t weight_count=0U;const auto influence_array=boneWeight(piece,weight_count);
    const std::span<const SkinInfluence> weights(influence_array.data(),weight_count);
    const auto cap=[&](const std::vector<std::uint32_t>& loop,DVec3 center,DVec3 normal){
        const auto vertex=builder.appendVertex({{static_cast<float>(center.x),static_cast<float>(center.y),static_cast<float>(center.z)},
            {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
            {},color,static_cast<std::uint16_t>(region),weights});
        reference_positions.emplace(vertex,center);
        for(std::size_t index=0U;index<loop.size();++index)
            builder.triangle(vertex,loop[index],loop[(index+1U)%loop.size()]);};
    cap(first,first_point,{-direction.x,-direction.y,-direction.z});
    cap(second,second_point,direction);
}

void appendTubePath(AppearanceMeshBuilder& builder, const GearPiece& piece,
                    std::span<const Vec3> points, float radius,
                    foundation::Color color, std::uint32_t region,
                    std::uint32_t segments,
                    const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    std::vector<std::uint32_t> previous;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Vec3 delta = index + 1U < points.size()
            ? Vec3{points[index + 1U].x - points[index].x,
                   points[index + 1U].y - points[index].y,
                   points[index + 1U].z - points[index].z}
            : Vec3{points[index].x - points[index - 1U].x,
                   points[index].y - points[index - 1U].y,
                   points[index].z - points[index - 1U].z};
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const Vec3 direction{delta.x / length, delta.y / length, delta.z / length};
        const Vec3 guide = std::abs(direction.z) < .9F ? Vec3{0.0F,0.0F,1.0F}
                                                        : Vec3{0.0F,1.0F,0.0F};
        Vec3 u{direction.y * guide.z - direction.z * guide.y,
               direction.z * guide.x - direction.x * guide.z,
               direction.x * guide.y - direction.y * guide.x};
        const float u_length = std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
        u = {u.x/u_length,u.y/u_length,u.z/u_length};
        Vec3 v{u.y * direction.z - u.z * direction.y,
               u.z * direction.x - u.x * direction.z,
               u.x * direction.y - u.y * direction.x};
        NormalizedInfluences dynamic_weights{};
        std::span<const SkinInfluence> weights{};
        if (weight_function != nullptr) {
            dynamic_weights = (*weight_function)(points[index]);
            weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                      dynamic_weights.count);
        }
        auto current = appendRing(builder,piece,points[index],u,v,radius,radius,
                                  segments,color,region,
                                  static_cast<float>(index)/static_cast<float>(points.size()),
                                  weights);
        if (!previous.empty()) bridge(builder, previous, current);
        previous = std::move(current);
    }
}

void appendReferenceTubePath(AppearanceMeshBuilder& builder,const GearPiece& piece,
                             std::span<const DVec3> points,double radius,
                             foundation::Color color,std::uint32_t region,
                             std::uint32_t segments,
                             std::unordered_map<std::uint32_t,DVec3>& reference_positions,
                             const std::function<NormalizedInfluences(Vec3)>* weight_function=nullptr) {
    std::uint8_t weight_count=0U;const auto weight_array=boneWeight(piece,weight_count);
    const std::span<const SkinInfluence> fixed_weights(weight_array.data(),weight_count);
    std::vector<std::uint32_t> previous;
    for(std::size_t at=0U;at<points.size();++at){const auto& a=points[at];DVec3 direction{};
        if(at+1U<points.size()){const auto& b=points[at+1U];direction={b.x-a.x,b.y-a.y,b.z-a.z};}
        else {const auto& b=points[at-1U];direction={a.x-b.x,a.y-b.y,a.z-b.z};}
        const double direction_length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
        direction={direction.x/direction_length,direction.y/direction_length,direction.z/direction_length};
        const DVec3 guide=std::abs(direction.z)<.9?DVec3{0.0,0.0,1.0}:DVec3{0.0,1.0,0.0};
        DVec3 u{direction.y*guide.z-direction.z*guide.y,direction.z*guide.x-direction.x*guide.z,direction.x*guide.y-direction.y*guide.x};
        const double u_length=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);u={u.x/u_length,u.y/u_length,u.z/u_length};
        DVec3 v{u.y*direction.z-u.z*direction.y,u.z*direction.x-u.x*direction.z,u.x*direction.y-u.y*direction.x};
        NormalizedInfluences dynamic_weights{};std::span<const SkinInfluence> weights=fixed_weights;
        if(weight_function!=nullptr){dynamic_weights=(*weight_function)({static_cast<float>(a.x),static_cast<float>(a.y),static_cast<float>(a.z)});
            weights=std::span<const SkinInfluence>(dynamic_weights.values.data(),dynamic_weights.count);}
        std::vector<std::uint32_t> ring;ring.reserve(segments);
        for(std::uint32_t index=0U;index<segments;++index){const double angle=2.0*3.14159265358979323846*index/segments;
            const double xx=std::cos(angle),zz=std::sin(angle);DVec3 point=a;
            point.x+=u.x*radius*xx;point.y+=u.y*radius*xx;point.z+=u.z*radius*xx;
            point.x+=v.x*radius*zz;point.y+=v.y*radius*zz;point.z+=v.z*radius*zz;
            DVec3 normal{u.x*xx+v.x*zz,u.y*xx+v.y*zz,u.z*xx+v.z*zz};
            const double normal_length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
            normal={normal.x/normal_length,normal.y/normal_length,normal.z/normal_length};
            const auto vertex=builder.appendVertex({{static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
                {static_cast<float>(index)/static_cast<float>(segments)*3.0F,static_cast<float>(at)/static_cast<float>(points.size())},
                color,static_cast<std::uint16_t>(region),weights});reference_positions.emplace(vertex,point);ring.push_back(vertex);}
        if(!previous.empty())bridge(builder,previous,ring);previous=std::move(ring);}
}

void appendReferenceBox(AppearanceMeshBuilder& builder,const GearPiece& piece,DVec3 center,
                        DVec3 dimensions,foundation::Color color,std::uint32_t region,
                        std::uint32_t detail_level,
                        std::unordered_map<std::uint32_t,DVec3>& reference_positions,
                        const std::function<NormalizedInfluences(Vec3)>* weight_function=nullptr,
                        const DQuaternion* rotation=nullptr) {
    constexpr std::array<std::array<int,2U>,6U> faces{{{{0,1}},{{0,-1}},{{1,1}},{{1,-1}},{{2,1}},{{2,-1}}}};
    const DVec3 half{dimensions.x*.5,dimensions.y*.5,dimensions.z*.5};
    const double radius=std::min({half.x,half.y,half.z})*.20*2.0;
    const DVec3 core{half.x-radius,half.y-radius,half.z-radius};
    const std::uint32_t segments=detail_level==3U?4U:detail_level==1U?1U:2U;
    std::uint8_t weight_count=0U;const auto weight_array=boneWeight(piece,weight_count);
    const std::span<const SkinInfluence> weights(weight_array.data(),weight_count);
    const auto read=[](DVec3 value,int axis){return axis==0?value.x:axis==1?value.y:value.z;};
    const auto write=[](DVec3& value,int axis,double component){if(axis==0)value.x=component;else if(axis==1)value.y=component;else value.z=component;};
    for(const auto face:faces){const int axis=face[0],sign=face[1],u=(axis+1)%3,v=(axis+2)%3;
        std::vector<std::vector<std::uint32_t>> loops(segments+1U,std::vector<std::uint32_t>(segments+1U));
        for(std::uint32_t row=0U;row<=segments;++row)for(std::uint32_t column=0U;column<=segments;++column){DVec3 raw{};
            write(raw,axis,read(half,axis)*sign);write(raw,u,(static_cast<double>(column)/segments*2.0-1.0)*read(half,u));
            write(raw,v,(static_cast<double>(row)/segments*2.0-1.0)*read(half,v));
            DVec3 point{std::clamp(raw.x,-core.x,core.x),std::clamp(raw.y,-core.y,core.y),std::clamp(raw.z,-core.z,core.z)};
            DVec3 normal{raw.x-point.x,raw.y-point.y,raw.z-point.z};const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
            const double inverse_length=1.0/length;
            normal={normal.x*inverse_length,normal.y*inverse_length,normal.z*inverse_length};point.x+=normal.x*radius;point.y+=normal.y*radius;point.z+=normal.z*radius;
            if(rotation!=nullptr){point=rotate(point,*rotation);normal=rotate(normal,*rotation);}
            point.x+=center.x;point.y+=center.y;point.z+=center.z;
            const Vec3 float_point{static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)};
            NormalizedInfluences dynamic_weights{};std::span<const SkinInfluence> vertex_weights=weights;
            if(weight_function!=nullptr){dynamic_weights=(*weight_function)(float_point);vertex_weights={dynamic_weights.values.data(),dynamic_weights.count};}
            const auto vertex=builder.appendVertex({float_point,
                {static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},
                {static_cast<float>(column)/segments,static_cast<float>(row)/segments},color,static_cast<std::uint16_t>(region),vertex_weights});
            reference_positions.emplace(vertex,point);loops[row][column]=vertex;}
        for(std::uint32_t row=0U;row<segments;++row)for(std::uint32_t column=0U;column<segments;++column){
            builder.triangle(loops[row][column],loops[row+1U][column],loops[row][column+1U]);
            builder.triangle(loops[row][column+1U],loops[row+1U][column],loops[row+1U][column+1U]);}}
}

void bridge(AppearanceMeshBuilder& builder, std::span<const std::uint32_t> first,
            std::span<const std::uint32_t> second) {
    for (std::size_t index = 0U; index < first.size(); ++index) {
        const std::size_t next = (index + 1U) % first.size();
        builder.triangle(first[index], second[index], first[next]);
        builder.triangle(first[next], second[index], second[next]);
    }
}

void appendCylinder(AppearanceMeshBuilder& builder, const GearPiece& piece,
                    Vec3 first_point, Vec3 second_point, float radius,
                    foundation::Color color, std::uint32_t region,
                    std::uint32_t segments) {
    Vec3 direction{second_point.x-first_point.x,second_point.y-first_point.y,
                   second_point.z-first_point.z};
    const float direction_length=std::sqrt(direction.x*direction.x+direction.y*direction.y+
                                            direction.z*direction.z);
    direction={direction.x/direction_length,direction.y/direction_length,direction.z/direction_length};
    const Vec3 guide=std::abs(direction.z)<.9F?Vec3{0.0F,0.0F,1.0F}:Vec3{0.0F,1.0F,0.0F};
    Vec3 u{direction.y*guide.z-direction.z*guide.y,
           direction.z*guide.x-direction.x*guide.z,
           direction.x*guide.y-direction.y*guide.x};
    const float u_length=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
    u={u.x/u_length,u.y/u_length,u.z/u_length};
    const Vec3 v{u.y*direction.z-u.z*direction.y,
                 u.z*direction.x-u.x*direction.z,
                 u.x*direction.y-u.y*direction.x};
    const auto first=appendRing(builder,piece,first_point,u,v,radius,radius,segments,color,region);
    const auto second=appendRing(builder,piece,second_point,u,v,radius,radius,segments,color,region);
    bridge(builder,first,second);
    std::uint8_t count=0U;const auto influence_array=boneWeight(piece,count);
    const std::span<const SkinInfluence> weights(influence_array.data(),count);
    const auto cap=[&](const std::vector<std::uint32_t>& loop,Vec3 center,Vec3 normal){
        const auto middle=builder.appendVertex({center,normal,{},color,
            static_cast<std::uint16_t>(region),weights});
        for(std::size_t index=0U;index<loop.size();++index)
            builder.triangle(middle,loop[index],loop[(index+1U)%loop.size()]);};
    cap(first,first_point,{-direction.x,-direction.y,-direction.z});
    cap(second,second_point,direction);
}

} // namespace

foundation::Result<AppearanceMesh, foundation::Error> GearSurfaceGenerator::build(
    const GearArtifact& artifact) {
    AppearanceMeshBuilder builder;
    std::unordered_map<std::uint32_t,DVec3> reference_positions;
    std::vector<AppearanceVertexTag> tags;
    std::unordered_map<std::uint32_t,std::uint32_t> duplicate_sources;
    for (const GearPiece& piece : artifact.pieces) {
        const std::uint32_t first_vertex = static_cast<std::uint32_t>(builder.mesh().vertices.size());
        const auto* definition = EquipmentCatalog::findItem(piece.definition_id);
        const std::string_view style = definition == nullptr ? std::string_view{} : definition->style;
        const Vec3 center = piece.center;
        const EquipmentItem* item = artifact.equipment.item(piece.slot);
        const float variant_shade = item == nullptr ? 1.0F : item->variant.shade;
        const foundation::Color variant_color{
            std::clamp(artifact.palette.r * variant_shade, 0.0F, 1.0F),
            std::clamp(artifact.palette.g * variant_shade, 0.0F, 1.0F),
            std::clamp(artifact.palette.b * variant_shade, 0.0F, 1.0F), 1.0F};
        const foundation::Color dark = tint(variant_color, 0.58F);
        const foundation::Color edge = tint(variant_color, 0.73F);
        switch (piece.slot) {
        case EquipmentSlot::Head:
            {
                const std::uint32_t segments = artifact.detail_level == 3U ? 48U :
                    artifact.detail_level == 1U ? 16U : 32U;
                const std::uint32_t rows = artifact.detail_level == 3U ? 14U :
                    artifact.detail_level == 1U ? 6U : 10U;
                const bool helmet=definition!=nullptr&&definition->kind==EquipmentKind::Helmet;
                const ReferenceHeadProfile reference_profile(artifact.fit);
                const double padding=(helmet?.008:.0035)/artifact.fit.reference_height;
                const double extra=(helmet?.017:(style=="beanie"?.018:.014))/artifact.fit.reference_height;
                const std::uint32_t shell_material=helmet?1U:0U;
                const double top_y=reference_profile.top_y;
                const auto web = tint(variant_color, .58F);
                const auto rubber = tint(rgb(0x292b28U), variant_shade);
                std::uint8_t weight_count = 0U;
                const auto weight_array = boneWeight(piece, weight_count);
                const std::span<const SkinInfluence> weights(weight_array.data(), weight_count);
                std::vector<std::uint32_t> first, previous, last;
                std::vector<DVec3> first_reference;
                std::vector<std::uint32_t> deferred_inner;
                first.reserve(segments); previous.reserve(segments); last.reserve(segments);
                for (std::uint32_t row = 0U; row < rows; ++row) {
                    const double t=static_cast<double>(row)/rows;
                    std::vector<std::uint32_t> loop;
                    loop.reserve(segments);
                    for (std::uint32_t column = 0U; column < segments; ++column) {
                        const double theta=-3.14159265358979323846+
                            6.283185307179586476925286766559*static_cast<double>(column)/segments;
                        const double bottom=reference_profile.headBottom(theta,artifact.fit,helmet);
                        DVec3 reference_point=reference_profile.point(bottom+(top_y-.0001-bottom)*t,theta);
                        const double round=std::cos(t*1.57079632679489661923);
                        reference_point.x+=std::sin(theta)*(padding+(helmet?.004:.0015))*round;
                        reference_point.z+=std::cos(theta)*(padding+(helmet?.004:.0015))*round;
                        reference_point.y+=extra*std::sin(t*1.57079632679489661923);
                        if(style=="beret"){
                            reference_point.x+=.013*std::sin(t*1.57079632679489661923);
                            reference_point.y+=.005*std::sin(theta)*round;
                        }
                        if(style=="patrol"&&t>.45)reference_point.y-=.006*std::sin((t-.45)/.55*3.14159265358979323846);
                        const Vec3 point{static_cast<float>(reference_point.x),static_cast<float>(reference_point.y),static_cast<float>(reference_point.z)};
                        const float color_factor=static_cast<float>(1.0+.025*std::sin(theta*10.0+t*8.0));
                        const auto color = style == "cover" || !helmet
                            ? clothColor(artifact, variant_shade, color_factor)
                            : tint(edge, color_factor);
                        const auto shell_vertex=builder.appendVertex({point,
                            {static_cast<float>(std::sin(theta)*round),static_cast<float>(t),static_cast<float>(std::cos(theta)*round)},
                            {static_cast<float>(column) / static_cast<float>(segments) * 3.0F,
                             static_cast<float>(t*4.0)}, color, static_cast<std::uint16_t>(shell_material), weights});
                        reference_positions.emplace(shell_vertex,reference_point);loop.push_back(shell_vertex);
                        if(row==0U)first_reference.push_back(reference_point);
                    }
                    if (!previous.empty()) bridge(builder, previous, loop); else first = loop;
                    previous = loop; last = std::move(loop);
                }
                auto crown_reference=reference_profile.point(top_y,0.0);crown_reference.y+=extra;
                if(style=="beret")crown_reference.x+=.013;
                const Vec3 crown{static_cast<float>(crown_reference.x),static_cast<float>(crown_reference.y),static_cast<float>(crown_reference.z)};
                const auto crown_color=helmet?edge:clothColor(artifact,variant_shade);
                const auto tip = builder.appendVertex({crown, {0.0F, 1.0F, 0.0F}, {},
                    crown_color, static_cast<std::uint16_t>(shell_material), weights});
                reference_positions.emplace(tip,crown_reference);
                for (std::uint32_t column = 0U; column < segments; ++column)
                    builder.triangle(last[column], tip, last[(column + 1U) % segments]);
                first_reference.push_back(first_reference.front());
                appendReferenceTubePath(builder,piece,first_reference,.0018,helmet?rubber:web,
                                        shell_material,tubeSegments(artifact.detail_level),reference_positions);
                std::vector<std::uint32_t> inner;
                inner.reserve(segments);
                for (std::size_t index=0U;index<first.size();++index) {
                    const auto& source=first_reference[index];
                    const double angle=std::atan2(source.x,source.z);
                    const DVec3 point{source.x-std::sin(angle)*padding*.7,
                        source.y+.0007,source.z-std::cos(angle)*padding*.7};
                    const auto vertex=builder.appendVertex({
                        {static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                        {0.0F,1.0F,0.0F},{},web,1U,weights});
                    reference_positions.emplace(vertex,point);inner.push_back(vertex);
                }
                if(helmet)bridge(builder, first, inner);else deferred_inner=std::move(inner);
                if(style=="cap"||style=="patrol"){
                    const double y=reference_profile.headBottom(0.0,artifact.fit,false)+.0015;
                    const double radius_x=reference_profile.section(y).rx;
                    const double z=reference_profile.point(y,0.0).z;
                    std::array<std::array<std::uint32_t,17U>,5U> grid{};
                    std::array<DVec3,17U> edge_points{};
                    for(std::uint32_t row=0U;row<=4U;++row){const double t=static_cast<double>(row)/4.0;
                        for(std::uint32_t column=0U;column<=16U;++column){
                            const double x=(static_cast<double>(column)/16.0*2.0-1.0)*radius_x*.90;
                            const DVec3 point{x,y-.007*t*t,z+.037*t*(.73+.27*std::cos(x/radius_x*1.57079632679489661923))-.009*(x/radius_x)*(x/radius_x)};
                            const auto vertex=builder.appendVertex({
                                {static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                                {0.0F,1.0F,0.0F},{static_cast<float>(static_cast<double>(column)/16.0),static_cast<float>(t)},
                                clothColor(artifact,variant_shade,.90F),0U,weights});
                            grid[row][column]=vertex;reference_positions.emplace(vertex,point);
                            if(row==4U)edge_points[column]=point;
                        }}
                    for(std::uint32_t row=0U;row<4U;++row)for(std::uint32_t column=0U;column<16U;++column){
                        builder.triangle(grid[row][column],grid[row+1U][column],grid[row][column+1U]);
                        builder.triangle(grid[row][column+1U],grid[row+1U][column],grid[row+1U][column+1U]);}
                    appendReferenceTubePath(builder,piece,edge_points,.0012,web,0U,
                        tubeSegments(artifact.detail_level),reference_positions);
                }
                if(style=="boonie"){
                    std::vector<std::uint32_t> prior;
                    std::vector<DVec3> outer;
                    for(std::uint32_t row=0U;row<3U;++row){std::vector<std::uint32_t> loop;loop.reserve(segments);
                        for(std::uint32_t column=0U;column<segments;++column){const double angle=
                                6.283185307179586476925286766559*static_cast<double>(column)/segments;
                            DVec3 point=reference_profile.point(reference_profile.headBottom(angle,artifact.fit,false),angle);
                            const double radius=.004+static_cast<double>(row)*.012;
                            point.x+=std::sin(angle)*radius;
                            point.y+=-.002+static_cast<double>(row)*.001*std::sin(angle*2.0);
                            point.z+=std::cos(angle)*radius;
                            const auto vertex=builder.appendVertex({
                                {static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},
                                {0.0F,1.0F,0.0F},{},clothColor(artifact,variant_shade),0U,weights});
                            reference_positions.emplace(vertex,point);loop.push_back(vertex);
                            if(row==2U)outer.push_back(point);}
                        if(!prior.empty())bridge(builder,prior,loop);prior=std::move(loop);}
                    outer.push_back(outer.front());appendReferenceTubePath(builder,piece,outer,.0012,web,0U,
                        tubeSegments(artifact.detail_level),reference_positions);
                }
                const float jaw_y = artifact.fit.face_anatomy.face.chin.y / artifact.fit.height + .006F;
                if(helmet)for (const float sign : {-1.0F, 1.0F}) {
                    const double angle=static_cast<double>(sign)*1.57079632679489661923;
                    const auto reference_point=reference_profile.point(
                        reference_profile.headBottom(angle,artifact.fit,true),angle);
                    const Vec3 point{static_cast<float>(reference_point.x),static_cast<float>(reference_point.y),static_cast<float>(reference_point.z)};
                    const auto jaw_section = FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy, jaw_y * artifact.fit.height);
                    const auto temple_section = FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy, .904F * artifact.fit.height);
                    const std::array<Vec3, 3U> strap{{
                        {point.x + sign * .005F, point.y, point.z},
                        {sign * temple_section.half_width / artifact.fit.height * 1.03F, .908F, .022F},
                        {sign * jaw_section.half_width / artifact.fit.height * .45F, jaw_y - .004F,
                         artifact.fit.faceFrontZ(sign * .015F, jaw_y) + .002F}}};
                    appendRibbon(builder, piece, strap, .0045F, web, 0U);
                    appendReferenceBox(builder,piece,
                        {reference_point.x+static_cast<double>(sign)*.006,reference_point.y+.005,reference_point.z},
                        {.006,.014,.022},rubber,1U,artifact.detail_level,reference_positions);
                }
                if (helmet&&style == "cover") for (const float sign : {-1.0F, 1.0F}) {
                    std::array<DVec3,9U> seam{};
                    for (std::uint32_t index = 0U; index < seam.size(); ++index) {
                        const double theta=static_cast<double>(sign)*(.18+static_cast<double>(index)*.12);
                        seam[index]=reference_profile.point(.976,theta);
                        seam[index].x+=std::sin(theta)*.009;
                        seam[index].y+=.009;
                        seam[index].z+=std::cos(theta)*.009;
                    }
                    appendReferenceTubePath(builder,piece,seam,.0008,web,0U,
                                            tubeSegments(artifact.detail_level),reference_positions);
                }
                if(!deferred_inner.empty()){
                    std::vector<std::uint32_t> duplicated_first;duplicated_first.reserve(first.size());
                    for(const auto source:first){const auto& vertex=builder.mesh().vertices[source];
                        const auto duplicate=builder.appendVertex({vertex.position,vertex.normal,
                            vertex.uv,vertex.color,1U,std::span<const SkinInfluence>(
                                vertex.influences.data(),vertex.influence_count)});
                        const auto reference=reference_positions.find(source);
                        if(reference!=reference_positions.end())reference_positions.emplace(duplicate,reference->second);
                        duplicated_first.push_back(duplicate);duplicate_sources.emplace(duplicate,source);}
                    for(std::size_t index=0U;index<duplicated_first.size();++index){
                        const auto next=(index+1U)%duplicated_first.size();
                        builder.appendTriangle(duplicated_first[index],deferred_inner[index],
                                               duplicated_first[next]);
                        builder.appendTriangle(duplicated_first[next],deferred_inner[index],
                                               deferred_inner[next]);
                    }
                }
            }
            break;
        case EquipmentSlot::Face:
            {
                if(definition==nullptr||style=="balaclava")break;
                const auto rubber=tint(rgb(0x292b28U),variant_shade);
                const auto metal=tint(rgb(0x545951U),variant_shade);
                if(definition->kind==EquipmentKind::Eyewear){
                    const bool goggles=style=="goggles";
                    const auto& face=artifact.fit.face.reference;
                    const ReferenceHeadProfile reference_profile(artifact.fit);
                    const double eye_width=.0083*face.eye_width_scale,eye_height=.0034*face.eye_height_scale;
                    const double radius=std::max(eye_width*1.12,eye_height*1.65)*.82;
                    const double minimum=radius*1.20+.003;
                    const double maximum=std::max(minimum,reference_profile.section(face.eye_y).rx*.66);
                    const double spacing=std::clamp(face.eye_spacing,minimum,maximum);
                    const double rx=eye_width*(goggles?1.5:1.25),ry=eye_height*(goggles?1.75:1.4);
                    std::array<DVec3,2U> ends{};std::size_t end_index=0U;
                    for(const double sign:{1.0,-1.0}){
                        const double eye_x=sign*spacing,eye_y=face.eye_y+sign*face.eye_asymmetry*.5;
                        const double eye_z=reference_profile.frontZ(eye_x,eye_y,artifact.fit)-radius*.80+
                                           face.eye_depth*.16;
                        const DVec3 center{eye_x,eye_y,eye_z+radius+.0025};
                        std::array<DVec3,25U> loop{};
                        for(std::uint32_t index=0U;index<=24U;++index){const double angle=
                            static_cast<double>(index)/24.0*6.283185307179586476925286766559;
                            const double sine=std::sin(angle);loop[index]={center.x+rx*std::cos(angle),
                                center.y+ry*sine,center.z-.001*sine*sine};}
                        appendReferenceTubePath(builder,piece,loop,goggles?.0018:.0008,rubber,1U,
                            tubeSegments(artifact.detail_level),reference_positions);
                        ends[end_index++]={center.x-sign*rx,center.y,center.z};
                        const DVec3 outer{center.x+sign*rx,center.y,center.z};
                        DVec3 side=reference_profile.point(eye_y,sign*1.57079632679489661923);
                        side.x+=sign*.003;
                        const std::array<DVec3,3U> arm{{outer,
                            {outer.x+sign*.004,outer.y,outer.z-.010},side}};
                        appendReferenceTubePath(builder,piece,arm,goggles?.0017:.0008,dark,1U,
                            tubeSegments(artifact.detail_level),reference_positions);
                    }
                    appendReferenceTubePath(builder,piece,ends,.0009,metal,1U,
                        tubeSegments(artifact.detail_level),reference_positions);
                }else if(style=="respirator"){
                    const ReferenceHeadProfile reference_profile(artifact.fit);
                    const double nose_base=reference_profile.noseBaseY();
                    const double y=nose_base-.006;
                    const double z=reference_profile.frontZ(0.0,y,artifact.fit)+.009;
                    appendReferenceBox(builder,piece,{0.0,y,z},{.038,.033,.021},rubber,1U,
                                       artifact.detail_level,reference_positions);
                    for(const double sign:{-1.0,1.0}){
                        const DVec3 point{sign*.024,y-.004,z};
                        const DVec3 end{point.x,point.y,point.z+.012};
                        appendReferenceCylinder(builder,piece,point,end,.010,edge,1U,12U,
                                                reference_positions);
                        for(int line=-2;line<=2;++line){const double yy=static_cast<double>(line)*.002;
                            const std::array<DVec3,2U> grille{{{end.x-.006,end.y+yy,end.z},
                                                               {end.x+.006,end.y+yy,end.z}}};
                            appendReferenceTubePath(builder,piece,grille,.0005,rubber,1U,
                                tubeSegments(artifact.detail_level),reference_positions);}
                    }
                }
            }
            break;
        case EquipmentSlot::Neck:
            {
                constexpr std::uint32_t count=32U;
                std::vector<std::uint32_t> previous;
                for(std::uint32_t row=0U;row<=5U;++row){
                    const float y=.847F+static_cast<float>(row)*.0045F;
                    const auto section=FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy,y*artifact.fit.height);
                    const float radius=.003F+std::sin(static_cast<float>(row)/5.0F*
                        3.14159265358979323846F)*.002F;
                    std::vector<std::uint32_t> ring;ring.reserve(count);
                    for(std::uint32_t index=0U;index<count;++index){const float angle=
                        6.28318530717958647692F*static_cast<float>(index)/static_cast<float>(count);
                        const Vec3 point{(section.half_width/artifact.fit.height+radius)*std::cos(angle),
                            y,section.center_z/artifact.fit.height+
                              (section.half_depth/artifact.fit.height+radius)*std::sin(angle)};
                        const auto dynamic=neckWeights(artifact.fit,y);
                        ring.push_back(builder.appendVertex({point,{std::cos(angle),0.0F,std::sin(angle)},
                            {static_cast<float>(index)/static_cast<float>(count)*3.0F,0.0F},
                            clothColor(artifact,variant_shade,row%2U?.87F:1.0F),0U,
                            std::span<const SkinInfluence>(dynamic.values.data(),dynamic.count)}));}
                    if(!previous.empty())bridge(builder,previous,ring);previous=std::move(ring);
                }
                if(style=="scarf"){
                    constexpr double x=.018,y=.807;
                    const auto profile=referenceJacketProfile(artifact.fit,y);
                    const DVec3 point{x,y,profile.y*std::sqrt(std::max(.04,1.0-
                        x*x/(profile.x*profile.x)))+.008};
                    const std::function<NormalizedInfluences(Vec3)> weights=
                        [&artifact](Vec3 p){return torsoWeights(artifact.fit,p);};
                    appendReferenceBox(builder,piece,point,{.023,.072,.007},
                        clothColor(artifact,variant_shade),0U,artifact.detail_level,
                        reference_positions,&weights);
                }
            }
            break;
        case EquipmentSlot::TorsoArmor:
            {
                const auto* armor_item=artifact.equipment.item(EquipmentSlot::TorsoArmor);
                const auto* armor_definition=armor_item==nullptr?nullptr:
                    EquipmentCatalog::findItem(armor_item->definition_id);
                const double exact_armor_thickness=referenceArmorThickness(
                    armor_definition==nullptr?std::string_view{}:armor_definition->identifier,
                    static_cast<double>(artifact.fit.armor_thickness)*artifact.fit.reference_height);
                const double gap=exact_armor_thickness/artifact.fit.reference_height+.003;
                const double y0=artifact.fit.mapTorsoYExact(style=="heavy"?.603:.635);
                const double y1=artifact.fit.mapTorsoYExact(.793);
                const double width_factor=style=="heavy"?.94:.79;
                const auto panel_color = clothColor(artifact, variant_shade, .87F);
                const std::function<NormalizedInfluences(Vec3)> weights =
                    [&artifact](Vec3 point) { return torsoWeights(artifact.fit, point); };
                const auto panel = [&](bool back) {
                    constexpr std::uint32_t rows = 8U, columns = 12U;
                    const double sign=back?-1.0:1.0;
                    std::array<std::array<std::uint32_t, columns + 1U>, rows + 1U> grid{};
                    std::array<std::array<DVec3,columns+1U>,rows+1U> reference_grid{};
                    for (std::uint32_t row = 0U; row <= rows; ++row) {
                        const double t=static_cast<double>(row)/rows;
                        const double y=y0+(y1-y0)*t;
                        const auto profile=referenceJacketProfile(artifact.fit,y);const double radius_x=profile.x;
                        const double taper=1.0-.16*std::max(0.0,(t-.68)/.32);
                        for (std::uint32_t column = 0U; column <= columns; ++column) {
                            const double x=(static_cast<double>(column)/columns*2.0-1.0)*
                                radius_x * width_factor * taper;
                            DVec3 reference_position{x,y,profile.y*std::sqrt(std::max(.04,1.0-x*x/(profile.x*profile.x)))+gap};
                            reference_position.z*=sign;
                            const Vec3 position{static_cast<float>(reference_position.x),static_cast<float>(reference_position.y),static_cast<float>(reference_position.z)};
                            Vec3 normal{static_cast<float>(x),0.0F,static_cast<float>(sign*.2)};
                            const float length = std::sqrt(normal.x*normal.x+normal.z*normal.z);
                            normal = {normal.x/length,0.0F,normal.z/length};
                            const auto vertex_weights = weights(position);
                            grid[row][column] = builder.appendVertex({position,normal,
                                {static_cast<float>(column)/static_cast<float>(columns)*3.0F,
                                 static_cast<float>(t*5.0)},panel_color,0U,
                                std::span<const SkinInfluence>(vertex_weights.values.data(),
                                                               vertex_weights.count)});
                            reference_positions.emplace(grid[row][column],reference_position);
                            reference_grid[row][column]=reference_position;
                        }
                    }
                    for (std::uint32_t row=0U;row<rows;++row)for(std::uint32_t column=0U;column<columns;++column){
                        builder.triangle(grid[row][column],grid[row+1U][column],grid[row][column+1U]);
                        builder.triangle(grid[row][column+1U],grid[row+1U][column],grid[row+1U][column+1U]);
                    }
                    std::array<DVec3,columns+1U> boundary{};
                    for(const std::uint32_t row:{0U,rows}){
                        for(std::uint32_t column=0U;column<=columns;++column)boundary[column]=reference_grid[row][column];
                        appendReferenceTubePath(builder,piece,boundary,.0015,dark,0U,
                            tubeSegments(artifact.detail_level),reference_positions,&weights);
                    }
                    std::array<DVec3,rows+1U> side_boundary{};
                    for(const std::uint32_t column:{0U,columns}){
                        for(std::uint32_t row=0U;row<=rows;++row)side_boundary[row]=reference_grid[row][column];
                        appendReferenceTubePath(builder,piece,side_boundary,.0015,dark,0U,
                            tubeSegments(artifact.detail_level),reference_positions,&weights);
                    }
                };
                panel(false); panel(true);
                const float strap_y0=artifact.fit.mapTorsoY(.665F);
                const float strap_y1=artifact.fit.mapTorsoY(.790F);
                const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*artifact.fit.profile(strap_y1).x*.65F;
                    std::array<Vec3,16U> points{};std::size_t at=0U;
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(strap_y0,strap_y1,static_cast<float>(i)/6.0F);points[at++]=artifact.fit.front(x*.86F,y,gap+.001F);}
                    points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(strap_y1,strap_y0,static_cast<float>(i)/6.0F);auto p=artifact.fit.front(x*.86F,y,gap+.001F);p.z=-p.z;points[at++]=p;}
                    appendRibbon(builder,piece,points,.022F,dark,0U,{1.0F,0.0F,0.0F},&weights);
                }
                const double side_y=artifact.fit.mapTorsoYExact(.673);
                const auto side_profile=referenceJacketProfile(artifact.fit,side_y);
                for(const float sign:{-1.0F,1.0F})
                    appendReferenceBox(builder,piece,{sign*(side_profile.x+.004),side_y,0.0},
                        {.009,.037,side_profile.y*1.7},dark,0U,artifact.detail_level,reference_positions,&weights);
                if(style=="heavy"){
                    const double lower_y=artifact.fit.mapTorsoYExact(.603);
                    const auto profile=referenceJacketProfile(artifact.fit,lower_y);
                    appendReferenceBox(builder,piece,{0.0,lower_y,profile.y+gap+.004},
                        {profile.x*1.25,.040,.009},edge,0U,artifact.detail_level,
                        reference_positions,&weights);
                }
            }
            break;
        case EquipmentSlot::ChestRig:
            {
                const double y=artifact.fit.mapTorsoYExact(.711);
                const double gap=static_cast<double>(artifact.fit.armor_thickness)+.010;
                if(artifact.fit.armor_thickness==0.0F){
                    const std::function<NormalizedInfluences(Vec3)> strap_weights=
                        [&artifact](Vec3 point){return torsoWeights(artifact.fit,point);};
                    const float y0=artifact.fit.mapTorsoY(.665F),y1=artifact.fit.mapTorsoY(.790F);
                    const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                    for(const float sign:{-1.0F,1.0F}){const float x=sign*artifact.fit.profile(y1).x*.65F;
                        std::array<Vec3,16U> points{};std::size_t at=0U;
                        for(std::uint32_t index=0U;index<7U;++index){const float yy=mix(y0,y1,static_cast<float>(index)/6.0F);points[at++]=artifact.fit.front(x*.86F,yy,gap);}
                        points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                        for(std::uint32_t index=0U;index<7U;++index){const float yy=mix(y1,y0,static_cast<float>(index)/6.0F);auto p=artifact.fit.front(x*.86F,yy,gap);p.z=-p.z;points[at++]=p;}
                        appendRibbon(builder,piece,points,.013F,dark,0U,{1.0F,0.0F,0.0F},&strap_weights);
                    }
                }
                const std::uint32_t pouch_count = definition->visual.count;
                const double width=referenceJacketProfile(artifact.fit,y).x*1.40/pouch_count;
                const double height=style=="ammo"?.066:.056;
                for (std::uint32_t index = 0U; index < pouch_count; ++index) {
                    const double x=(static_cast<double>(index)-(static_cast<double>(pouch_count)-1.0)*.5)*(width+.003);
                    const auto profile=referenceJacketProfile(artifact.fit,y);
                    const DVec3 pouch_center{x,y,profile.y*std::sqrt(std::max(.04,1.0-x*x/(profile.x*profile.x)))+gap+.010};
                    DVec3 normal{x*.8,0.0,1.0};const double normal_length=std::sqrt(normal.x*normal.x+normal.z*normal.z);
                    normal={normal.x/normal_length,0.0,normal.z/normal_length};const DQuaternion rotation=fromZ(normal);
                    const NormalizedInfluences pouch_weights=torsoWeights(artifact.fit,
                        {static_cast<float>(pouch_center.x),static_cast<float>(pouch_center.y),static_cast<float>(pouch_center.z)});
                    const std::function<NormalizedInfluences(Vec3)> constant_weights=[pouch_weights](Vec3){return pouch_weights;};
                    const auto offset=[&](double ox,double oy,double oz){const auto transformed=rotate(DVec3{ox,oy,oz},rotation);
                        return DVec3{pouch_center.x+transformed.x,pouch_center.y+transformed.y,pouch_center.z+transformed.z};
                    };
                    const DVec3 size{width,height,.024};
                    appendReferenceBox(builder,piece,pouch_center,size,piece.color,0U,artifact.detail_level,reference_positions,&constant_weights,&rotation);
                    appendReferenceBox(builder,piece,offset(0.0,size.y*.30,size.z*.48),
                        {size.x*1.025,size.y*.24,.003},edge,0U,artifact.detail_level,reference_positions,&constant_weights,&rotation);
                    const double front=size.z*.54;
                    appendReferenceBox(builder,piece,offset(0.0,0.0,front),{.008,size.y*.50,.003},dark,0U,
                        artifact.detail_level,reference_positions,&constant_weights,&rotation);
                    appendReferenceBox(builder,piece,offset(0.0,-size.y*.12,front+.001),{.014,.010,.004},
                        tint(rgb(0x292b28U),variant_shade),1U,artifact.detail_level,reference_positions,&constant_weights,&rotation);
                    if (style == "medical" || style == "tools") {
                        const auto light = tint(rgb(0xb9b097U), variant_shade * .6F);
                        for (const float sign : {-1.0F, 1.0F})
                            appendReferenceBox(builder,piece,offset(sign*size.x*.22,-size.y*.06,front+.001),
                                {.004,size.y*.55,.002},light,0U,artifact.detail_level,reference_positions,&constant_weights,&rotation);
                    }
                }
                const auto profile = artifact.fit.profile(y);
                const std::array<Vec3,3U> line{{
                    artifact.fit.front(-profile.x * .78F, y - .035F, gap),
                    artifact.fit.front(0.0F, y - .035F, gap),
                    artifact.fit.front(profile.x * .78F, y - .035F, gap)}};
                const std::function<NormalizedInfluences(Vec3)> weights =
                    [&artifact](Vec3 point) { return torsoWeights(artifact.fit, point); };
                appendRibbon(builder, piece, line, .010F, dark, 0U,
                             {0.0F, 1.0F, 0.0F}, &weights);
            }
            break;
        case EquipmentSlot::Legs:
            if (definition == nullptr || !definition->visual.pads) break;
            for (const bool left : {true, false}) {
                GearPiece leg_piece = piece;
                leg_piece.bone = left ? BoneId::ShinL : BoneId::ShinR;
                const auto& shin=artifact.fit.reference_bind_points[boneIndex(leg_piece.bone)];
                const DVec3 knee{shin[0],shin[1]+.009,
                                 shin[2]+.038*artifact.fit.body.reference_leg_thickness_scale};
                appendReferenceBox(builder,leg_piece,knee,
                    {.047*artifact.fit.body.reference_leg_thickness_scale,.058,.009},
                    tint(rgb(0x292b28U),variant_shade),1U,artifact.detail_level,reference_positions);
                for (int ridge = -1; ridge <= 1; ++ridge) {
                    appendReferenceBox(builder,leg_piece,
                        {knee.x,knee.y+ridge*.014,knee.z+.006},
                        {.036,.005,.003},edge,1U,artifact.detail_level,reference_positions);
                }
            }
            break;
        case EquipmentSlot::Belt:
            {
                const double y=artifact.fit.reference_hip_y+.009;
                const auto profile=referenceJacketProfile(artifact.fit,y);
                const double radius_x=profile.x+.004,radius_z=profile.y+.004;
                const double height=definition!=nullptr&&definition->visual.style=="utility"?.014:.009;
                const auto lower=appendReferenceRing(builder,piece,{0.0,y-height*.5,0.0},
                    {1.0,0.0,0.0},{0.0,0.0,1.0},radius_x,radius_z,48U,dark,0U,reference_positions);
                const auto upper=appendReferenceRing(builder,piece,{0.0,y+height*.5,0.0},
                    {1.0,0.0,0.0},{0.0,0.0,1.0},radius_x,radius_z,48U,dark,0U,reference_positions);
                bridge(builder, lower, upper);
                appendReferenceBox(builder,piece,{0.0,y,radius_z+.002},
                    {.024,height*.85,.006},tint(rgb(0x292b28U),variant_shade),1U,
                    artifact.detail_level,reference_positions);
            }
            break;
        case EquipmentSlot::Back:
            {
                const Vec3 d=artifact.fit.pack_dimensions;
                const auto& socket=artifact.fit.socket(EquipmentSocketId::BackCenter);
                const auto* armor_item=artifact.equipment.item(EquipmentSlot::TorsoArmor);
                const auto* armor_definition=armor_item==nullptr?nullptr:
                    EquipmentCatalog::findItem(armor_item->definition_id);
                const double exact_armor_gap=referenceArmorThickness(
                    armor_definition==nullptr?std::string_view{}:armor_definition->identifier,
                    static_cast<double>(artifact.fit.armor_thickness)*artifact.fit.reference_height)/
                    artifact.fit.reference_height;
                const double exact_torso_y=artifact.fit.mapTorsoYExact(.725);
                const auto exact_torso_profile=referenceJacketProfile(artifact.fit,exact_torso_y);
                const DVec3 exact_socket{0.0,exact_torso_y,
                    -exact_torso_profile.y-.008-exact_armor_gap};
                const DVec3 authoring_pack=referencePackDimensions(
                    definition==nullptr?std::string_view{}:definition->identifier,{.26,.32,.13});
                const double pack_scale=std::clamp(.97+.10*(artifact.fit.body.reference_shoulder_width_scale-1.0),.90,1.08)*
                    (item==nullptr?1.0:referenceVariantSize(item->seed));
                const DVec3 exact_pack_dimensions{
                    authoring_pack.x/artifact.fit.reference_height*pack_scale,
                    authoring_pack.y/artifact.fit.reference_height*pack_scale,
                    authoring_pack.z/artifact.fit.reference_height};
                const DVec3 exact_pack_center{exact_socket.x,exact_socket.y,
                    exact_socket.z-exact_pack_dimensions.z*.5};
                const Vec3 pack_center{socket.position.x,socket.position.y,socket.position.z-d.z*.5F};
                appendBox(builder,piece,pack_center,d,piece.color,0U,artifact.detail_level);
                appendBox(builder,piece,{pack_center.x,pack_center.y+d.y*.39F,pack_center.z-.002F},
                          {d.x*1.03F,d.y*.20F,d.z*1.04F},edge,0U,artifact.detail_level);
                const Vec3 back{pack_center.x,pack_center.y,pack_center.z-d.z*.5F-.004F};
                const auto pouch=[&](Vec3 pouch_center,Vec3 size,Vec3 normal,std::string_view pouch_style){
                    const bool front_pouch=size.z<.03F;
                    const DVec3 exact_direction{normal.x>0.0F?1.0:-1.0,0.0,-.3};
                    const double exact_direction_length=std::sqrt(exact_direction.x*exact_direction.x+exact_direction.z*exact_direction.z);
                    const auto exact_direction_quaternion=fromZ(DVec3{exact_direction.x/exact_direction_length,0.0,exact_direction.z/exact_direction_length});
                    const Quaternion rotation=front_pouch?
                        fromZ(normal):Quaternion{static_cast<float>(exact_direction_quaternion.x),static_cast<float>(exact_direction_quaternion.y),
                                                 static_cast<float>(exact_direction_quaternion.z),static_cast<float>(exact_direction_quaternion.w)};
                    const DQuaternion exact_rotation=front_pouch?
                        DQuaternion{static_cast<double>(rotation.x),static_cast<double>(rotation.y),
                                    static_cast<double>(rotation.z),static_cast<double>(rotation.w)}:
                        exact_direction_quaternion;
                    const double authoring_width=authoring_pack.x;
                    const double authoring_height=authoring_pack.y;
                    const double authoring_depth=authoring_pack.z;
                    const double exact_pouch_y=exact_torso_y-
                        authoring_height/artifact.fit.reference_height*pack_scale*.12;
                    const double exact_pack_width=authoring_width/artifact.fit.reference_height*pack_scale;
                    const double exact_pack_height=authoring_height/artifact.fit.reference_height*pack_scale;
                    const double exact_pack_depth=authoring_depth/artifact.fit.reference_height;
                    const DVec3 exact_size=front_pouch?
                        DVec3{exact_pack_width*.72/(style=="medical"?2.0:1.0),exact_pack_height*.44,.021}:
                        DVec3{exact_pack_width*.28,exact_pack_height*.50,exact_pack_depth*.68};
                    DVec3 center{pouch_center.x,exact_pouch_y,pouch_center.z};
                    if(front_pouch){
                        const double front_spacing=style=="medical"?exact_pack_width*.45*.5:0.0;
                        center.x=(pouch_center.x<0.0F?-front_spacing:front_spacing);
                    }else center.x=(pouch_center.x<0.0F?-1.0:1.0)*exact_pack_width*.53;
                    if(!front_pouch)center.z=exact_socket.z-exact_pack_depth*.5-.006;
                    else center.z=exact_socket.z-exact_pack_depth-.012;
                    const auto exact_offset=[&](double x,double y,double z){const auto p=rotate(DVec3{x,y,z},exact_rotation);return DVec3{center.x+p.x,center.y+p.y,center.z+p.z};};
                    const double exact_front=exact_size.z*.54;
                    appendReferenceBox(builder,piece,center,exact_size,piece.color,0U,artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,exact_size.y*.30,exact_size.z*.48),
                              {exact_size.x*1.025,exact_size.y*.24,.003},edge,0U,artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,0.0,exact_front),{.008,exact_size.y*.50,.003},dark,0U,artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,-exact_size.y*.12,exact_front+.001),{.014,.010,.004},
                              tint(rgb(0x292b28U),variant_shade),1U,artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    if(pouch_style=="medical"||pouch_style=="tools")
                        for(const double sign:{-1.0,1.0})
                            appendReferenceBox(builder,piece,
                                exact_offset(sign*exact_size.x*.22,-exact_size.y*.06,exact_front+.001),
                                {.004,exact_size.y*.55,.002},
                                tint(rgb(0xb9b097U),variant_shade*.6F),0U,
                                artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                };
                for(const float sign:{-1.0F,1.0F}){
                    Vec3 normal{sign,0.0F,-.3F};const float length=std::sqrt(1.09F);normal={normal.x/length,0.0F,normal.z/length};
                    pouch({pack_center.x+sign*d.x*.53F,pack_center.y-d.y*.12F,pack_center.z-.006F},
                          {d.x*.28F,d.y*.50F,d.z*.68F},normal,
                          style=="medical"?std::string_view{"medical"}:std::string_view{"utility"});
                }
                const std::uint32_t front_pouches=style=="medical"?2U:1U;
                for(std::uint32_t index=0U;index<front_pouches;++index)
                    pouch({back.x+(static_cast<float>(index)-(static_cast<float>(front_pouches)-1.0F)*.5F)*d.x*.45F,
                           back.y-d.y*.12F,back.z-.008F},
                          {d.x*.72F/static_cast<float>(front_pouches),d.y*.44F,.021F},
                          {0.0F,0.0F,-1.0F},style);
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*d.x*.29F;
                    const std::array<Vec3,3U> strap{{
                        {pack_center.x+x,pack_center.y+d.y*.42F,pack_center.z-d.z*.54F},
                        {pack_center.x+x,pack_center.y,pack_center.z-d.z*.54F},
                        {pack_center.x+x,pack_center.y-d.y*.42F,pack_center.z-d.z*.54F}}};
                    appendRibbon(builder,piece,strap,.009F,dark,0U);
                    appendReferenceBox(builder,piece,
                              {exact_pack_center.x+static_cast<double>(sign)*exact_pack_dimensions.x*.29,
                               exact_pack_center.y-exact_pack_dimensions.y*.16,
                               exact_pack_center.z-exact_pack_dimensions.z*.55},
                              {.016,.013,.006},tint(rgb(0x292b28U),variant_shade),1U,
                              artifact.detail_level,reference_positions);
                }
                const std::array<Vec3,4U> handle{{
                    {pack_center.x-.019F,pack_center.y+d.y*.5F,pack_center.z},
                    {pack_center.x-.016F,pack_center.y+d.y*.55F,pack_center.z},
                    {pack_center.x+.016F,pack_center.y+d.y*.55F,pack_center.z},
                    {pack_center.x+.019F,pack_center.y+d.y*.5F,pack_center.z}}};
                appendTubePath(builder,piece,handle,.0022F,dark,0U,
                               tubeSegments(artifact.detail_level));
                const std::function<NormalizedInfluences(Vec3)> weights=
                    [&artifact](Vec3 point){return torsoWeights(artifact.fit,point);};
                const float y0=artifact.fit.mapTorsoY(.665F),y1=artifact.fit.mapTorsoY(.790F);
                const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*artifact.fit.profile(y1).x*.65F;std::array<Vec3,16U> points{};std::size_t at=0U;
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(y0,y1,static_cast<float>(i)/6.0F);points[at++]=artifact.fit.front(x*.86F,y,artifact.fit.armor_thickness+.011F);}
                    points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(y1,y0,static_cast<float>(i)/6.0F);auto p=artifact.fit.front(x*.86F,y,artifact.fit.armor_thickness+.011F);p.z=-p.z;points[at++]=p;}
                    appendRibbon(builder,piece,points,.015F,dark,0U,{1.0F,0.0F,0.0F},&weights);
                }
                if(definition!=nullptr&&definition->visual.roll){
                    const float y=pack_center.y-d.y*.49F-.028F;
                    appendCylinder(builder,piece,{-d.x*.6F,y,pack_center.z},
                                   {d.x*.6F,y,pack_center.z},.026F,
                                   clothColor(artifact,variant_shade,.80F),0U,16U);
                    for(const float sign:{-1.0F,1.0F})
                        appendCylinder(builder,piece,{sign*d.x*.34F-.003F,y,pack_center.z},
                                       {sign*d.x*.34F+.003F,y,pack_center.z},.027F,
                                       dark,0U,16U);
                }
                if(style=="radio"){
                    const auto rubber=tint(rgb(0x292b28U),variant_shade);
                    const auto metal=tint(rgb(0x545951U),variant_shade);
                    appendReferenceBox(builder,piece,
                        {exact_pack_center.x+.012,exact_pack_center.y+exact_pack_dimensions.y*.5+.008,
                         exact_pack_center.z},
                        {exact_pack_dimensions.x*.72,.020,exact_pack_dimensions.z*.80},rubber,1U,
                        artifact.detail_level,reference_positions);
                    const DVec3 point{exact_pack_center.x-exact_pack_dimensions.x*.30,
                                      exact_pack_center.y+exact_pack_dimensions.y*.52,
                                      exact_pack_center.z-exact_pack_dimensions.z*.12};
                    appendReferenceCylinder(builder,piece,point,
                        {point.x+.016,point.y+.23,point.z-.012},.0014,metal,2U,10U,
                        reference_positions);
                    appendReferenceCylinder(builder,piece,point,
                        {point.x+.002,point.y+.033,point.z-.002},.0035,rubber,1U,10U,
                        reference_positions);
                    const double cable_y=artifact.fit.mapTorsoYExact(.77);
                    const auto cable_profile=referenceJacketProfile(artifact.fit,cable_y);
                    const double cable_gap=.016+exact_armor_gap;
                    const double cable_x=cable_profile.x*.7;
                    const DVec3 cable_front{cable_x,cable_y,
                        cable_profile.y*std::sqrt(std::max(.04,1.0-
                            cable_x*cable_x/(cable_profile.x*cable_profile.x)))+cable_gap};
                    const std::array<DVec3,3U> cable{{
                        {point.x+.033,point.y,point.z},
                        {exact_pack_center.x+exact_pack_dimensions.x*.55,
                         exact_pack_center.y+exact_pack_dimensions.y*.4,
                         exact_pack_center.z+.025},
                        cable_front}};
                    appendReferenceTubePath(builder,piece,cable,.0015,rubber,1U,
                        tubeSegments(artifact.detail_level),reference_positions,&weights);
                }
                if(style=="engineer"){
                    const DVec3 exact_back{exact_pack_center.x,exact_pack_center.y,
                        exact_pack_center.z-exact_pack_dimensions.z*.5-.004};
                    const DVec3 point{exact_back.x-exact_pack_dimensions.x*.30,
                                      exact_back.y+.005,exact_back.z-.03};
                    appendReferenceCylinder(builder,piece,
                        {point.x,point.y-.05,point.z},{point.x,point.y+.065,point.z},
                        .0038,edge,1U,10U,reference_positions);
                    appendReferenceBox(builder,piece,{point.x,point.y-.071,point.z},
                        {.035,.045,.004},tint(rgb(0x545951U),variant_shade),2U,
                        artifact.detail_level,reference_positions);
                    appendReferenceCylinder(builder,piece,
                        {point.x-.012,point.y+.079,point.z},
                        {point.x+.012,point.y+.079,point.z},.0035,
                        tint(rgb(0x292b28U),variant_shade),1U,10U,reference_positions);
                }
            }
            break;
        case EquipmentSlot::LeftHip:
        case EquipmentSlot::RightHip:
        case EquipmentSlot::LeftThigh:
        case EquipmentSlot::RightThigh:
        case EquipmentSlot::Utility1:
        case EquipmentSlot::Utility2:
        case EquipmentSlot::Utility3:
            {
                const auto socket_id = piece.slot == EquipmentSlot::LeftHip ? EquipmentSocketId::HipL :
                    piece.slot == EquipmentSlot::RightHip ? EquipmentSocketId::HipR :
                    piece.slot == EquipmentSlot::LeftThigh ? EquipmentSocketId::ThighL :
                    piece.slot == EquipmentSlot::RightThigh ? EquipmentSocketId::ThighR :
                    piece.slot == EquipmentSlot::Utility1 ? EquipmentSocketId::WaistFront :
                    piece.slot == EquipmentSlot::Utility2 ? EquipmentSocketId::WaistBack :
                    EquipmentSocketId::ChestLeft;
                const Vec3 socket_normal = artifact.fit.socket(socket_id).normal;
                const Quaternion rotation = fromZ(socket_normal);
                const auto offset = [&center, &rotation](float x, float y, float z) {
                    const Vec3 transformed = rotate({x, y, z}, rotation);
                    return Vec3{center.x + transformed.x, center.y + transformed.y,
                                center.z + transformed.z};
                };
                const foundation::Color rubber = tint(rgb(0x292b28U), variant_shade);
                if(style=="binoculars"){
                    if(piece.slot==EquipmentSlot::Utility3){
                        const double torso_y=artifact.fit.mapTorsoYExact(.725);
                        const double socket_y=artifact.fit.mapTorsoYExact(.78);
                        const auto anchor_profile=referenceJacketProfile(artifact.fit,torso_y);
                        const auto face_profile=referenceJacketProfile(artifact.fit,socket_y);
                        const double socket_x=anchor_profile.x*.48;
                        const DVec3 exact_center{socket_x,socket_y,face_profile.y*std::sqrt(std::max(
                            .04,1.0-socket_x*socket_x/(face_profile.x*face_profile.x)))+.004+
                            artifact.fit.armor_thickness};
                        const DQuaternion exact_rotation=fromZ(DVec3{0.0,0.0,1.0});
                        const auto exact_offset=[&](double x,double y,double z){
                            const auto transformed=rotate(DVec3{x,y,z},exact_rotation);
                            return DVec3{exact_center.x+transformed.x,exact_center.y+transformed.y,
                                         exact_center.z+transformed.z};};
                        for(const double sign:{-1.0,1.0}){
                            const DVec3 first=exact_offset(sign*.012,0.0,0.0);
                            const DVec3 delta=rotate(DVec3{0.0,-.030,.003},exact_rotation);
                            appendReferenceCylinder(builder,piece,first,
                                {first.x+delta.x,first.y+delta.y,first.z+delta.z},.009,
                                rubber,1U,12U,reference_positions);
                        }
                        appendReferenceBox(builder,piece,exact_center,{.028,.010,.012},
                            tint(rgb(0x545951U),variant_shade),1U,artifact.detail_level,
                            reference_positions,nullptr,&exact_rotation);
                    }else{
                        for(const float sign:{-1.0F,1.0F}){
                            const Vec3 first=offset(sign*.012F,0.0F,0.0F);
                            const Vec3 delta=rotate({0.0F,-.030F,.003F},rotation);
                            appendCylinder(builder,piece,first,
                                {first.x+delta.x,first.y+delta.y,first.z+delta.z},.009F,
                                rubber,1U,12U);
                        }
                        appendBox(builder,piece,center,{.028F,.010F,.012F},
                                  tint(rgb(0x545951U),variant_shade),1U,
                                  artifact.detail_level,&rotation);
                    }
                    break;
                }
                if(style=="radio"){
                    if(piece.slot==EquipmentSlot::Utility3){
                        const double torso_y=artifact.fit.mapTorsoYExact(.725);
                        const double socket_y=artifact.fit.mapTorsoYExact(.78);
                        const auto anchor_profile=referenceJacketProfile(artifact.fit,torso_y);
                        const auto face_profile=referenceJacketProfile(artifact.fit,socket_y);
                        const double socket_x=anchor_profile.x*.48;
                        const DVec3 exact_center{socket_x,socket_y,face_profile.y*std::sqrt(std::max(
                            .04,1.0-socket_x*socket_x/(face_profile.x*face_profile.x)))+.004+
                            artifact.fit.armor_thickness};
                        const DQuaternion exact_rotation=fromZ(DVec3{0.0,0.0,1.0});
                        const auto exact_offset=[&](double x,double y,double z){
                            const auto transformed=rotate(DVec3{x,y,z},exact_rotation);
                            return DVec3{exact_center.x+transformed.x,exact_center.y+transformed.y,
                                         exact_center.z+transformed.z};};
                        appendReferenceBox(builder,piece,exact_center,{.020,.037,.016},rubber,1U,
                            artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                        const DVec3 top=exact_offset(-.007,.018,0.0);
                        const auto antenna_delta=rotate(DVec3{0.0,.045,0.0},exact_rotation);
                        appendReferenceCylinder(builder,piece,top,
                            {top.x+antenna_delta.x,top.y+antenna_delta.y,top.z+antenna_delta.z},
                            .0012,tint(rgb(0x545951U),variant_shade),2U,10U,reference_positions);
                        for(std::uint32_t index=0U;index<4U;++index)
                            appendReferenceBox(builder,piece,
                                exact_offset(0.0,.007-static_cast<double>(index)*.004,.009),
                                {.014,.0015,.001},edge,1U,artifact.detail_level,
                                reference_positions,nullptr,&exact_rotation);
                    }else{
                        appendBox(builder,piece,center,{.020F,.037F,.016F},rubber,1U,
                                  artifact.detail_level,&rotation);
                        const Vec3 top=offset(-.007F,.018F,0.0F);
                        const Vec3 antenna_delta=rotate({0.0F,.045F,0.0F},rotation);
                        appendCylinder(builder,piece,top,
                            {top.x+antenna_delta.x,top.y+antenna_delta.y,top.z+antenna_delta.z},
                            .0012F,tint(rgb(0x545951U),variant_shade),2U,10U);
                        for(std::uint32_t index=0U;index<4U;++index)
                            appendBox(builder,piece,offset(0.0F,.007F-static_cast<float>(index)*.004F,.009F),
                                      {.014F,.0015F,.001F},edge,1U,artifact.detail_level,&rotation);
                    }
                    break;
                }
                if (style == "canteen") {
                    if(piece.slot==EquipmentSlot::LeftHip||piece.slot==EquipmentSlot::RightHip){
                        const double sign=piece.slot==EquipmentSlot::LeftHip?1.0:-1.0;
                        const double hip_y=artifact.fit.reference_hip_y;
                        const auto profile=referenceJacketProfile(artifact.fit,hip_y+.012);
                        const DVec3 exact_center{sign*(profile.x+.014),hip_y-.018,0.0};
                        const DQuaternion exact_rotation=fromZ(DVec3{sign,0.0,0.0});
                        const DVec3 exact_radii{.025,.037,.018};
                        appendReferenceEllipsoid(builder,piece,exact_center,exact_radii,edge,0U,
                            12U,8U,reference_positions,&exact_rotation);
                        const auto exact_offset=[&](double x,double y,double z){const auto p=rotate(DVec3{x,y,z},exact_rotation);
                            return DVec3{exact_center.x+p.x,exact_center.y+p.y,exact_center.z+p.z};};
                        appendReferenceBox(builder,piece,exact_offset(0.0,.037,0.0),
                            {.016,.013,.015},rubber,1U,artifact.detail_level,reference_positions,
                            nullptr,&exact_rotation);
                        const std::array<DVec3,2U> line{{exact_offset(-.020,.020,.017),
                                                        exact_offset(.020,.020,.017)}};
                        appendReferenceRibbon(builder,piece,line,.006,dark,0U,reference_positions);
                    }else{
                        appendEllipsoid(builder,piece,center,{.025F,.037F,.018F},edge,0U,12U,8U,&rotation);
                        appendBox(builder,piece,offset(0.0F,.037F,0.0F),{.016F,.013F,.015F},rubber,1U,
                                  artifact.detail_level,&rotation);
                        const std::array<Vec3,2U> line{{offset(-.020F,.020F,.017F),offset(.020F,.020F,.017F)}};
                        appendRibbon(builder,piece,line,.006F,dark,0U);
                    }
                    break;
                }
                if(piece.slot==EquipmentSlot::LeftHip||piece.slot==EquipmentSlot::RightHip||
                   piece.slot==EquipmentSlot::LeftThigh||piece.slot==EquipmentSlot::RightThigh){
                    const bool left=piece.slot==EquipmentSlot::LeftHip||piece.slot==EquipmentSlot::LeftThigh;
                    const bool thigh=piece.slot==EquipmentSlot::LeftThigh||piece.slot==EquipmentSlot::RightThigh;
                    const double sign=left?1.0:-1.0;
                    DVec3 exact_center{};
                    if(thigh){
                        const BoneId thigh_bone=left?BoneId::ThighL:BoneId::ThighR;
                        const auto& bind=artifact.fit.reference_bind_points[boneIndex(thigh_bone)];
                        exact_center={bind[0]+sign*(.054*artifact.fit.body.reference_leg_thickness_scale+.013),
                                      bind[1]-.062,bind[2]};
                    }else{
                        const double exact_hip_y=artifact.fit.reference_hip_y;
                        const auto exact_profile=referenceJacketProfile(artifact.fit,exact_hip_y+.012);
                        exact_center={sign*(exact_profile.x+.014),exact_hip_y-.018,0.0};
                    }
                    const DVec3 exact_normal{sign,0.0,0.0};
                    const auto exact_rotation=fromZ(exact_normal);
                    const double exact_variant=item==nullptr?1.0:referenceVariantSize(item->seed);
                    const DVec3 exact_size=style=="map"?DVec3{.050,.057,.012}:DVec3{.043,.057,.025};
                    const DVec3 pouch_size{exact_size.x*exact_variant,exact_size.y*exact_variant,
                                           exact_size.z*exact_variant};
                    const auto exact_offset=[&](double x,double y,double z){const auto p=rotate(DVec3{x,y,z},exact_rotation);
                        return DVec3{exact_center.x+p.x,exact_center.y+p.y,exact_center.z+p.z};};
                    appendReferenceBox(builder,piece,exact_center,pouch_size,piece.color,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,pouch_size.y*.30,pouch_size.z*.48),
                        {pouch_size.x*1.025,pouch_size.y*.24,.003},edge,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    const double front=pouch_size.z*.54;
                    appendReferenceBox(builder,piece,exact_offset(0.0,0.0,front),
                        {.008,pouch_size.y*.50,.003},dark,0U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,-pouch_size.y*.12,front+.001),
                        {.014,.010,.004},rubber,1U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    if(style=="medical"||style=="tools"){
                        const foundation::Color light=tint(rgb(0xb9b097U),variant_shade*.6F);
                        for(const double pouch_sign:{-1.0,1.0})
                            appendReferenceBox(builder,piece,exact_offset(pouch_sign*pouch_size.x*.22,
                                -pouch_size.y*.06,front+.001),{.004,pouch_size.y*.55,.002},light,0U,
                                artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    }
                    if(thigh){
                        const BoneId thigh_bone=left?BoneId::ThighL:BoneId::ThighR;
                        GearPiece strap_piece=piece;strap_piece.bone=thigh_bone;
                        const auto& bind=artifact.fit.reference_bind_points[boneIndex(thigh_bone)];
                        const double radius=.055*artifact.fit.body.reference_leg_thickness_scale+.003;
                        const auto one=appendReferenceRing(builder,strap_piece,
                            {bind[0],exact_center.y-.004,bind[2]},{1.0,0.0,0.0},{0.0,0.0,1.0},
                            radius,radius,32U,dark,0U,reference_positions);
                        const auto two=appendReferenceRing(builder,strap_piece,
                            {bind[0],exact_center.y+.004,bind[2]},{1.0,0.0,0.0},{0.0,0.0,1.0},
                            radius,radius,32U,dark,0U,reference_positions);
                        bridge(builder,one,two);
                    }
                }else{
                    Vec3 pouch_size = style == "map" ? Vec3{.050F, .057F, .012F}
                                                     : Vec3{.043F, .057F, .025F};
                    pouch_size = {pouch_size.x * item->variant.size,
                                  pouch_size.y * item->variant.size,
                                  pouch_size.z * item->variant.size};
                    appendBox(builder, piece, center, pouch_size, piece.color, 0U,
                              artifact.detail_level, &rotation);
                    appendBox(builder, piece,
                              offset(0.0F, pouch_size.y * .30F, pouch_size.z * .48F),
                              {pouch_size.x * 1.025F, pouch_size.y * .24F, .003F},
                              edge, 0U, artifact.detail_level, &rotation);
                    const float front = pouch_size.z * .54F;
                    appendBox(builder, piece, offset(0.0F, 0.0F, front),
                              {.008F, pouch_size.y * .50F, .003F}, dark, 0U,
                              artifact.detail_level, &rotation);
                    appendBox(builder, piece, offset(0.0F, -pouch_size.y * .12F, front + .001F),
                              {.014F, .010F, .004F}, rubber, 1U,
                              artifact.detail_level, &rotation);
                    if (style == "medical" || style == "tools") {
                        const foundation::Color light = tint(rgb(0xb9b097U), variant_shade * .6F);
                        for (const float pouch_sign : {-1.0F, 1.0F})
                            appendBox(builder, piece,
                                      offset(pouch_sign * pouch_size.x * .22F,
                                             -pouch_size.y * .06F, front + .001F),
                                      {.004F, pouch_size.y * .55F, .002F}, light, 0U,
                                      artifact.detail_level, &rotation);
                    }
                }
                if ((piece.slot == EquipmentSlot::LeftThigh ||
                    piece.slot == EquipmentSlot::RightThigh) && style!="map" &&
                    style!="utility" && style!="ammo" && style!="medical" && style!="tools") {
                    const bool left = piece.slot == EquipmentSlot::LeftThigh;
                    const BoneId thigh = left ? BoneId::ThighL : BoneId::ThighR;
                    GearPiece strap_piece = piece; strap_piece.bone = thigh;
                    Vec3 strap_center = artifact.fit.bind_points[boneIndex(thigh)];
                    strap_center.y = center.y;
                    const float radius = .055F * artifact.fit.body.leg_thickness_scale + .003F;
                    const auto one = appendRing(builder, strap_piece,
                        {strap_center.x, strap_center.y - .004F, strap_center.z},
                        {1.0F,0.0F,0.0F},{0.0F,0.0F,1.0F},radius,radius,32U,dark,0U);
                    const auto two = appendRing(builder, strap_piece,
                        {strap_center.x, strap_center.y + .004F, strap_center.z},
                        {1.0F,0.0F,0.0F},{0.0F,0.0F,1.0F},radius,radius,32U,dark,0U);
                    bridge(builder, one, two);
                }
            }
            break;
        case EquipmentSlot::PrimaryWeapon:
            {
                const std::array<Vec3, 2U> points{{
                    {center.x - .016F, center.y + .028F, center.z + .003F},
                    {center.x + .016F, center.y + .028F, center.z + .003F}}};
                appendRibbon(builder, piece, points, .009F, dark, 0U);
            }
            break;
        case EquipmentSlot::SecondaryWeapon:
        case EquipmentSlot::MeleeWeapon:
        case EquipmentSlot::Throwable:
            {
                const foundation::Color rubber=tint(rgb(0x292b28U),variant_shade);
                if(style=="knife"){
                    if(piece.slot==EquipmentSlot::MeleeWeapon){
                        const double hip_y=artifact.fit.reference_hip_y;
                        const auto profile=referenceJacketProfile(artifact.fit,hip_y+.012);
                        const DVec3 exact_center{-(profile.x+.014),hip_y-.018,0.0};
                        appendReferenceBox(builder,piece,exact_center,{.020,.052,.012},rubber,1U,
                            artifact.detail_level,reference_positions);
                    }else{
                        appendBox(builder,piece,center,{.020F,.052F,.012F},rubber,1U,
                                  artifact.detail_level);
                    }
                    break;
                }
                if(style=="grenade"&&piece.slot==EquipmentSlot::Throwable){
                    const double hip_y=artifact.fit.reference_hip_y;
                    const auto waist_profile=referenceJacketProfile(artifact.fit,hip_y+.012);
                    const DVec3 exact_center{0.0,hip_y-.007,waist_profile.y+.012};
                    constexpr DVec3 exact_size{.033,.029,.017};
                    const DQuaternion exact_rotation{};
                    const auto exact_offset=[&](double x,double y,double z){
                        const auto transformed=rotate(DVec3{x,y,z},exact_rotation);
                        return DVec3{exact_center.x+transformed.x,exact_center.y+transformed.y,
                                     exact_center.z+transformed.z};};
                    appendReferenceBox(builder,piece,exact_center,exact_size,piece.color,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,
                        exact_offset(0.0,exact_size.y*.30,exact_size.z*.48),
                        {exact_size.x*1.025,exact_size.y*.24,.003},edge,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    const double front=exact_size.z*.54;
                    appendReferenceBox(builder,piece,exact_offset(0.0,0.0,front),
                        {.008,exact_size.y*.50,.003},dark,0U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,exact_offset(0.0,-exact_size.y*.12,front+.001),
                        {.014,.010,.004},rubber,1U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    break;
                }
                if(style=="sidearm"&&piece.slot==EquipmentSlot::SecondaryWeapon){
                    const auto& body=artifact.fit.body;
                    const double hip_half=.052*body.reference_hip_width_scale;
                    const DVec3 exact_center{
                        -hip_half-.048*body.reference_leg_thickness_scale,
                        artifact.fit.reference_hip_y-.092,.040};
                    const DQuaternion exact_rotation=fromZ(DVec3{-.7,0.0,.7});
                    constexpr DVec3 exact_size{.030,.057,.018};
                    const auto exact_offset=[&](double x,double y,double z){
                        const auto transformed=rotate(DVec3{x,y,z},exact_rotation);
                        return DVec3{exact_center.x+transformed.x,
                                     exact_center.y+transformed.y,
                                     exact_center.z+transformed.z};};
                    appendReferenceBox(builder,piece,exact_center,exact_size,piece.color,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,
                        exact_offset(0.0,exact_size.y*.30,exact_size.z*.48),
                        {exact_size.x*1.025,exact_size.y*.24,.003},edge,0U,
                        artifact.detail_level,reference_positions,nullptr,&exact_rotation);
                    const double front=exact_size.z*.54;
                    appendReferenceBox(builder,piece,exact_offset(0.0,0.0,front),
                        {.008,exact_size.y*.50,.003},dark,0U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    appendReferenceBox(builder,piece,
                        exact_offset(0.0,-exact_size.y*.12,front+.001),
                        {.014,.010,.004},rubber,1U,artifact.detail_level,
                        reference_positions,nullptr,&exact_rotation);
                    break;
                }
                const auto socket_id=style=="sidearm"?EquipmentSocketId::WeaponHip:
                                     EquipmentSocketId::WaistFront;
                const Quaternion rotation=fromZ(artifact.fit.socket(socket_id).normal);
                const Vec3 size=style=="sidearm"?Vec3{.030F,.057F,.018F}:
                                                    Vec3{.033F,.029F,.017F};
                const auto offset=[&](float x,float y,float z){const auto p=rotate({x,y,z},rotation);
                    return Vec3{center.x+p.x,center.y+p.y,center.z+p.z};};
                appendBox(builder,piece,center,size,piece.color,0U,artifact.detail_level,&rotation);
                appendBox(builder,piece,offset(0.0F,size.y*.30F,size.z*.48F),
                          {size.x*1.025F,size.y*.24F,.003F},edge,0U,
                          artifact.detail_level,&rotation);
                const float front=size.z*.54F;
                appendBox(builder,piece,offset(0.0F,0.0F,front),
                          {.008F,size.y*.50F,.003F},dark,0U,artifact.detail_level,&rotation);
                appendBox(builder,piece,offset(0.0F,-size.y*.12F,front+.001F),
                          {.014F,.010F,.004F},rubber,1U,artifact.detail_level,&rotation);
            }
            break;
        default:
            break;
        }
        const std::uint32_t end_vertex = static_cast<std::uint32_t>(builder.mesh().vertices.size());
        if (end_vertex > first_vertex) {
            AppearanceVertexTag tag{};
            tag.name = "gear." + std::string(
                EquipmentCatalog::slots()[equipmentSlotIndex(piece.slot)].identifier);
            tag.vertices.reserve(end_vertex - first_vertex);
            for (std::uint32_t vertex = first_vertex; vertex < end_vertex; ++vertex)
                if(!duplicate_sources.contains(vertex))tag.vertices.push_back(vertex);
            tags.push_back(std::move(tag));
        }
    }
    for (std::uint32_t index=0U;index<builder.mesh().vertices.size();++index) {
        auto& vertex=builder.mesh().vertices[index];const auto reference=reference_positions.find(index);
        if(reference!=reference_positions.end()){
            vertex.position.x=static_cast<float>(reference->second.x*artifact.fit.reference_height);
            vertex.position.y=static_cast<float>(reference->second.y*artifact.fit.reference_height);
            vertex.position.z=static_cast<float>(reference->second.z*artifact.fit.reference_height);
        }else{
            vertex.position.x *= artifact.fit.height;
            vertex.position.y *= artifact.fit.height;
            vertex.position.z *= artifact.fit.height;
        }
    }
    AppearanceMesh mesh = std::move(builder).finalize();
    if(mesh.vertices.empty()){
        mesh.minimum={};mesh.maximum={};mesh.sphere_center={};mesh.sphere_radius=0.0F;
        mesh.tags=std::move(tags);
        return foundation::Result<AppearanceMesh,foundation::Error>::success(std::move(mesh));
    }
    mesh.minimum = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity()};
    mesh.maximum = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity()};
    for (const auto& vertex : mesh.vertices) {
        mesh.minimum.x = std::min(mesh.minimum.x, vertex.position.x);
        mesh.minimum.y = std::min(mesh.minimum.y, vertex.position.y);
        mesh.minimum.z = std::min(mesh.minimum.z, vertex.position.z);
        mesh.maximum.x = std::max(mesh.maximum.x, vertex.position.x);
        mesh.maximum.y = std::max(mesh.maximum.y, vertex.position.y);
        mesh.maximum.z = std::max(mesh.maximum.z, vertex.position.z);
    }
    mesh.sphere_center = {(mesh.minimum.x + mesh.maximum.x) * .5F,
                          (mesh.minimum.y + mesh.maximum.y) * .5F,
                          (mesh.minimum.z + mesh.maximum.z) * .5F};
    float radius_squared = 0.0F;
    for (const auto& vertex : mesh.vertices) {
        const float x=vertex.position.x-mesh.sphere_center.x;
        const float y=vertex.position.y-mesh.sphere_center.y;
        const float z=vertex.position.z-mesh.sphere_center.z;
        radius_squared=std::max(radius_squared,x*x+y*y+z*z);
    }
    mesh.sphere_radius=std::sqrt(radius_squared);
    std::array<std::vector<std::uint32_t>, 3U> material_indices{};
    for (std::size_t triangle = 0U; triangle < mesh.indices.size() / 3U; ++triangle) {
        const std::size_t offset = triangle * 3U;
        const auto material = std::min<std::size_t>(2U,
            mesh.vertices[mesh.indices[offset]].material_region);
        material_indices[material].insert(material_indices[material].end(),
            {mesh.indices[offset], mesh.indices[offset + 1U], mesh.indices[offset + 2U]});
    }
    mesh.indices.clear();
    mesh.groups.clear();
    for (std::uint16_t material = 0U; material < material_indices.size(); ++material) {
        const auto& source = material_indices[material];
        if (source.empty()) continue;
        mesh.groups.push_back({static_cast<std::uint32_t>(mesh.indices.size()),
                               static_cast<std::uint32_t>(source.size()), material});
        mesh.indices.insert(mesh.indices.end(), source.begin(), source.end());
    }
    const std::vector<AppearanceVertex> hinted_vertices = mesh.vertices;
    struct EdgeEntry final { std::uint32_t triangle; bool direction; };
    const std::size_t triangle_count = mesh.indices.size() / 3U;
    std::unordered_map<std::uint64_t, EdgeEntry> edges;
    std::vector<std::vector<std::pair<std::uint32_t, std::uint8_t>>> neighbours(triangle_count);
    for (std::uint32_t triangle = 0U; triangle < triangle_count; ++triangle) {
        for (std::uint32_t edge = 0U; edge < 3U; ++edge) {
            const auto actual_a = mesh.indices[triangle * 3U + edge];
            const auto actual_b = mesh.indices[triangle * 3U + (edge + 1U) % 3U];
            const auto found_a=duplicate_sources.find(actual_a),found_b=duplicate_sources.find(actual_b);
            const auto a=found_a==duplicate_sources.end()?actual_a:found_a->second;
            const auto b=found_b==duplicate_sources.end()?actual_b:found_b->second;
            const auto low = std::min(a, b), high = std::max(a, b);
            const std::uint64_t key = static_cast<std::uint64_t>(low) * mesh.vertices.size() + high;
            const EdgeEntry entry{triangle, a < b};
            const auto found = edges.find(key);
            if (found == edges.end()) edges.emplace(key, entry);
            else {
                const std::uint8_t different = found->second.direction == entry.direction ? 1U : 0U;
                neighbours[triangle].push_back({found->second.triangle, different});
                neighbours[found->second.triangle].push_back({triangle, different});
            }
        }
    }
    std::vector<std::int8_t> flips(triangle_count, -1);
    for (std::uint32_t start = 0U; start < triangle_count; ++start) {
        if (flips[start] != -1) continue;
        flips[start] = 0;
        std::vector<std::uint32_t> queue{start};
        double score = 0.0;
        for (std::size_t at = 0U; at < queue.size(); ++at) {
            const auto triangle = queue[at];
            for (const auto [next, delta] : neighbours[triangle]) if (flips[next] == -1) {
                flips[next] = static_cast<std::int8_t>(flips[triangle] ^ delta);
                queue.push_back(next);
            }
            const auto a = mesh.indices[triangle * 3U];
            const auto b = mesh.indices[triangle * 3U + 1U];
            const auto c = mesh.indices[triangle * 3U + 2U];
            const auto& pa = mesh.vertices[a].position;
            const auto& pb0 = mesh.vertices[b].position;
            const auto& pc0 = mesh.vertices[c].position;
            const Vec3 pb{pb0.x-pa.x,pb0.y-pa.y,pb0.z-pa.z};
            const Vec3 pc{pc0.x-pa.x,pc0.y-pa.y,pc0.z-pa.z};
            const Vec3 normal{pb.y*pc.z-pb.z*pc.y,pb.z*pc.x-pb.x*pc.z,pb.x*pc.y-pb.y*pc.x};
            const Vec3 hint{hinted_vertices[a].normal.x+hinted_vertices[b].normal.x+hinted_vertices[c].normal.x,
                            hinted_vertices[a].normal.y+hinted_vertices[b].normal.y+hinted_vertices[c].normal.y,
                            hinted_vertices[a].normal.z+hinted_vertices[b].normal.z+hinted_vertices[c].normal.z};
            const double dot = static_cast<double>(normal.x)*hint.x+
                static_cast<double>(normal.y)*hint.y+static_cast<double>(normal.z)*hint.z;
            score += flips[triangle] ? -dot : dot;
        }
        const std::uint8_t invert = score < 0.0 ? 1U : 0U;
        for (const auto triangle : queue) if ((flips[triangle] ^ invert) != 0)
            std::swap(mesh.indices[triangle * 3U + 1U], mesh.indices[triangle * 3U + 2U]);
    }
    for (auto& vertex : mesh.vertices) vertex.normal = {};
    for (std::size_t offset = 0U; offset < mesh.indices.size(); offset += 3U) {
        const std::uint32_t a = mesh.indices[offset];
        const std::uint32_t b = mesh.indices[offset + 1U];
        const std::uint32_t c = mesh.indices[offset + 2U];
        const auto& pa = mesh.vertices[a].position;
        const auto& pb = mesh.vertices[b].position;
        const auto& pc = mesh.vertices[c].position;
        const double cbx=jsSubtract(pc.x,pb.x),cby=jsSubtract(pc.y,pb.y),cbz=jsSubtract(pc.z,pb.z);
        const double abx=jsSubtract(pa.x,pb.x),aby=jsSubtract(pa.y,pb.y),abz=jsSubtract(pa.z,pb.z);
        const double nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
        for (const std::uint32_t index : {a, b, c}) {
            const auto duplicate=duplicate_sources.find(index);
            const auto target=duplicate==duplicate_sources.end()?index:duplicate->second;
            addFloat32(mesh.vertices[target].normal.x,nx);
            addFloat32(mesh.vertices[target].normal.y,ny);
            addFloat32(mesh.vertices[target].normal.z,nz);
        }
    }
    for(const auto& [duplicate,source]:duplicate_sources)
        mesh.vertices[duplicate].normal=mesh.vertices[source].normal;
    for (auto& vertex : mesh.vertices) {
        const double length=jsLength(vertex.normal);
        if (length > 0.0F) {
            vertex.normal.x=static_cast<float>(vertex.normal.x/length);
            vertex.normal.y=static_cast<float>(vertex.normal.y/length);
            vertex.normal.z=static_cast<float>(vertex.normal.z/length);
        }
    }
    for (auto& vertex : mesh.vertices) {
        vertex.material_region = static_cast<std::uint16_t>(
            vertex.material_region == 0U ? AppearanceMaterialRegion::EquipmentCloth :
            vertex.material_region == 1U ? AppearanceMaterialRegion::EquipmentPaint :
                                           AppearanceMaterialRegion::EquipmentMetal);
    }
    mesh.tags = std::move(tags);
    return foundation::Result<AppearanceMesh, foundation::Error>::success(std::move(mesh));
}

} // namespace genomes::infantry
