#include <genomes/infantry/FaceAnatomy.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace genomes::infantry {

namespace {
[[nodiscard]] double mix(double a,double b,double t) noexcept{return a+(b-a)*t;}
[[nodiscard]] double smooth(double value) noexcept{const double t=std::clamp(value,0.0,1.0);return t*t*(3-2*t);}
[[nodiscard]] double bell(double x) noexcept{return std::exp(-x*x);}
} // namespace

foundation::Result<ResolvedAnatomy, foundation::Error>
FaceAnatomyEvaluator::resolve(const PhenotypeArtifact& phenotype) {
    if (!phenotype.valid()) {
        return foundation::Result<ResolvedAnatomy, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid phenotype for anatomy resolution"});
    }
    const BodyPhenotype& body = phenotype.body;
    const FacePhenotype& face = phenotype.face;
    ResolvedAnatomy result{};
    result.reference_profile = true;
    result.height = body.height;
    const auto add_torso = [&result](float y, float width, float depth, float front) {
        if (result.torso_sections.empty() ||
            y > result.torso_sections.back().y + 1.0e-5F) {
            result.torso_sections.push_back({y, width, depth, front});
        }
    };
    add_torso(0.04F, body.hip_width * 0.30F, body.chest_depth * 0.36F, 0.0F);
    add_torso(body.pelvis.y - body.height * 0.04F, body.hip_width * 0.50F,
              body.chest_depth * 0.46F, 0.0F);
    add_torso(body.pelvis.y, body.hip_width * 0.50F, body.chest_depth * 0.50F, 0.0F);
    add_torso(body.chest.y - body.height * 0.07F, body.waist_width * 0.50F,
              body.chest_depth * 0.48F, 0.0F);
    add_torso(body.chest.y, body.shoulder_width * 0.50F, body.chest_depth * 0.50F, 0.0F);
    add_torso(body.head.y - body.height * 0.06F, body.shoulder_width * 0.38F,
              body.chest_depth * 0.42F, 0.0F);
    add_torso(body.head.y, body.shoulder_width * 0.28F, body.chest_depth * 0.32F, 0.0F);

    const auto add_head = [&result](float y, float width, float depth, float center_z) {
        if (result.head_sections.empty() ||
            y > result.head_sections.back().y + 1.0e-5F) {
            result.head_sections.push_back({y, width, depth, center_z});
        }
    };
    const double neck_x=.0285*body.neck_scale,neck_z=.024*body.neck_scale;
    constexpr std::array<std::array<double,4>,12> base{{
        {{.881,.034,.030,.013}},{{.890,.039,.037,.010}},{{.900,.041,.041,.008}},
        {{.913,.045,.044,.005}},{{.925,.049,.046,.003}},{{.938,.048,.046,.002}},
        {{.948,.047,.046,.001}},{{.959,.0465,.046,0}},{{.973,.043,.043,-.001}},
        {{.986,.032,.034,-.003}},{{.995,.015,.018,-.004}},{{.999,0,0,-.004}}}};
    std::array<std::array<double,4>,12> shaped{};
    for(std::size_t i=0;i<base.size();++i){const double y0=base[i][0],low=smooth(std::clamp((.928-y0)/.058,0.0,1.0));
        const double temple=bell((y0-.951)/.020),forehead=smooth((y0-.948)/.038),chin=bell((y0-.887)/.015);
        double y=y0;if(y0<.925)y=.925+(y0-.925)*face.jaw_length_scale+face.chin_height*chin*.65;
        if(y0>.947)y=.999-(.999-y0)*face.head_length_scale;
        double scale=mix(1,face.jaw_width_scale,low)*mix(1,face.jaw_angle,low*low*.40);
        scale=mix(scale,face.temple_width_scale,temple*.55);scale=mix(scale,face.forehead_width_scale,forehead*.65);
        shaped[i]={y,base[i][1]*scale*face.head_width_scale*mix(1,body.head_scale,.4),
                   base[i][2]*face.head_depth_scale*mix(1,body.head_scale,.4),base[i][3]};}
    shaped[0][0]=std::clamp(shaped[0][0],.8755,.8855);
    for(std::size_t i=1;i<shaped.size();++i)shaped[i][0]=std::max(shaped[i][0],shaped[i-1][0]+.003);
    const double chin_y=shaped[0][0];
    const auto add_normalized=[&](double y,double rx,double rz,double z){add_head(
        static_cast<float>(y*body.height),static_cast<float>(rx*body.height),
        static_cast<float>(rz*body.height),static_cast<float>(z*body.height));};
    add_normalized(.828,neck_x*1.14,neck_z*1.12,-.002);
    add_normalized(.841,neck_x*1.06,neck_z*1.04,-.002);
    add_normalized(.853,neck_x,neck_z,-.002);
    for(const double t:{.2,.4,.6,.8}){const double blend=smooth(t);add_normalized(
        mix(.853,chin_y,t),mix(neck_x*.99,shaped[0][1],blend),
        mix(neck_z*1.01,shaped[0][2],blend),mix(.001,shaped[0][3],blend));}
    for(const auto& level:shaped)add_normalized(level[0],level[1],level[2],level[3]);
    if (result.head_sections.size() < 2U) {
        return foundation::Result<ResolvedAnatomy, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "head anatomy has insufficient profile levels"});
    }

    const float lower_head_y = static_cast<float>(chin_y*body.height);
    const float eye_front = face.frontZ(face.eye_y);
    const float nose_front = face.frontZ(face.nose_y) + face.nose_length;
    const float mouth_front = face.frontZ(face.mouth_y);
    const float ear_x = face.section(face.eye_y).radius_x * 0.96F * face.ear_scale;
    result.face.left_eye = {-face.eye_spacing,
                            face.eye_y - face.eye_asymmetry * 0.5F,
                            eye_front + face.eye_depth};
    result.face.right_eye = {face.eye_spacing,
                             face.eye_y + face.eye_asymmetry * 0.5F,
                             eye_front + face.eye_depth};
    result.face.nose_bridge = {0.0F,
                               face.eye_y - face.eye_radius * 0.70F,
                               eye_front + face.nose_bridge_scale * 0.001F};
    result.face.nose_tip = {0.0F, face.nose_y, nose_front};
    result.face.left_mouth_corner = {-face.mouth_width * 0.5F,
                                     face.mouth_y - face.mouth_asymmetry * 0.5F,
                                     mouth_front};
    result.face.right_mouth_corner = {face.mouth_width * 0.5F,
                                      face.mouth_y + face.mouth_asymmetry * 0.5F,
                                      mouth_front};
    result.face.chin = {0.0F, lower_head_y, face.section(lower_head_y).center_z +
                                       face.section(lower_head_y).radius_z * 0.82F};
    result.face.left_ear = {-ear_x,
                            face.eye_y - face.eye_radius * 0.25F - face.ear_asymmetry * 0.5F,
                            face.section(face.eye_y).center_z};
    result.face.right_ear = {ear_x,
                             face.eye_y - face.eye_radius * 0.25F + face.ear_asymmetry * 0.5F,
                             face.section(face.eye_y).center_z};
    // Scalp is anatomy, not hairstyle. Hair volume is an outward/lift
    // property and must never move the skull crown itself.
    result.scalp = {lower_head_y, face.hairline_y,
                    result.head_sections.back().y,
                    face.temple_recession, face.widow_peak};
    return foundation::Result<ResolvedAnatomy, foundation::Error>::success(std::move(result));
}

HeadCrossSection FaceAnatomyEvaluator::sectionAt(const ResolvedAnatomy& anatomy,
                                                 float y) noexcept {
    if (anatomy.head_sections.empty()) {
        return {};
    }
    const auto& levels=anatomy.head_sections;
    if(!anatomy.reference_profile){
        if(y<=levels.front().y)return levels.front();
        for(std::size_t index=1;index<levels.size();++index)if(y<=levels[index].y){
            const auto& lower=levels[index-1U];const auto& upper=levels[index];
            const float t=std::clamp((y-lower.y)/std::max(1.0e-6F,upper.y-lower.y),0.0F,1.0F);
            const float s=t*t*(3-2*t);return {y,lower.half_width+(upper.half_width-lower.half_width)*s,
                lower.half_depth+(upper.half_depth-lower.half_depth)*s,
                lower.center_z+(upper.center_z-lower.center_z)*s};}
        return levels.back();
    }
    std::size_t lower=0;
    while(lower+1U<levels.size()&&levels[lower+1U].y<y)++lower;
    if(lower+1U>=levels.size())return {y,0,0,levels.back().center_z};
    const float span=levels[lower+1U].y-levels[lower].y;
    const float t=std::clamp((y-levels[lower].y)/span,0.0F,1.0F),t2=t*t,t3=t2*t;
    const auto component=[](const HeadCrossSection& section,std::size_t index){return
        index==0?section.y:(index==1?section.half_width:(index==2?section.half_depth:section.center_z));};
    const auto slope=[&](std::size_t at,std::size_t field){
        if(at==0)return (component(levels[1],field)-component(levels[0],field))/(levels[1].y-levels[0].y);
        if(at+1U==levels.size())return (component(levels[at],field)-component(levels[at-1U],field))/(levels[at].y-levels[at-1U].y);
        const float dl=(component(levels[at],field)-component(levels[at-1U],field))/(levels[at].y-levels[at-1U].y);
        const float dr=(component(levels[at+1U],field)-component(levels[at],field))/(levels[at+1U].y-levels[at].y);
        return dl*dr<=0?0.0F:2*dl*dr/(dl+dr);};
    const auto hermite=[&](std::size_t field){const float a=component(levels[lower],field),b=component(levels[lower+1U],field);
        return (2*t3-3*t2+1)*a+(t3-2*t2+t)*slope(lower,field)*span+
               (-2*t3+3*t2)*b+(t3-t2)*slope(lower+1U,field)*span;};
    return {y,std::max(0.0F,hermite(1)),std::max(0.0F,hermite(2)),hermite(3)};
}

float FaceAnatomyEvaluator::hairlineY(const ResolvedAnatomy& anatomy,
                                          float azimuth) noexcept {
    // HairGenerator uses +Z as front, i.e. sin(azimuth) is frontal. Recession
    // raises the lower boundary at the temples; a widow peak lowers its centre.
    const float front = std::max(0.0F, std::sin(azimuth));
    const float side = std::abs(std::cos(azimuth));
    const float temple = front * std::pow(side, 1.8F);
    const float center_front = front * std::pow(std::max(0.0F, 1.0F - side), 1.5F);
    const float resolved = anatomy.scalp.hairline_y +
        anatomy.scalp.temple_recession * temple -
        anatomy.scalp.widow_peak * center_front;
    return std::clamp(resolved, anatomy.scalp.lower_y,
                      std::max(anatomy.scalp.lower_y, anatomy.scalp.crown_y - 1.0e-4F));
}

foundation::Vec3 FaceAnatomyEvaluator::scalpPoint(const ResolvedAnatomy& anatomy,
                                                  float normalized_height,
                                                  float azimuth) noexcept {
    const float t = std::clamp(normalized_height, 0.0F, 1.0F);
    const float hairline = hairlineY(anatomy, azimuth);
    const float y = hairline +
                    (anatomy.scalp.crown_y - hairline) * t;
    const HeadCrossSection section = sectionAt(anatomy, y);
    const float c = std::cos(azimuth);
    const float s = std::sin(azimuth);
    return {section.half_width * c, y,
            section.center_z + section.half_depth * s};
}

} // namespace genomes::infantry
