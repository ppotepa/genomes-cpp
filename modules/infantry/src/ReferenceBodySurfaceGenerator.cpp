#include <genomes/infantry/ReferenceBodySurfaceGenerator.hpp>
#include <genomes/infantry/ReferenceFaceSurfaceGenerator.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace genomes::infantry {

namespace {

[[nodiscard]] double smooth(double value) noexcept {
    value=std::clamp(value,0.0,1.0);return value*value*(3.0-2.0*value);
}

[[nodiscard]] foundation::Color referenceRgb(std::uint32_t hex) {
    const auto channel=[](std::uint32_t value){const double c=static_cast<double>(value)/255.0;
        return static_cast<float>(c<.04045?c*.0773993808:std::pow(c*.9478672986+.0521327014,2.4));};
    return {channel((hex>>16U)&255U),channel((hex>>8U)&255U),channel(hex&255U),1.0F};
}

[[nodiscard]] std::vector<ReferenceInfluence> torsoWeights(
    const EquipmentFit& fit,ReferenceVec3 point) {
    const double lower=fit.bind_points[boneIndex(BoneId::SpineLower)].y;
    const double upper=fit.bind_points[boneIndex(BoneId::SpineUpper)].y;
    const double chest_y=fit.bind_points[boneIndex(BoneId::Chest)].y;
    if(point.y<lower){const double t=smooth((point.y-fit.hip_y)/std::max(.001,lower-fit.hip_y));
        return {{static_cast<std::uint16_t>(boneIndex(BoneId::Hips)),1.0-t},
                {static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)),t}};}
    if(point.y<upper){const double t=smooth((point.y-lower)/std::max(.001,upper-lower));
        return {{static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)),1.0-t},
                {static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)),t}};}
    if(point.y<chest_y){const double t=smooth((point.y-upper)/std::max(.001,chest_y-upper));
        return {{static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)),1.0-t},
                {static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),t}};}
    if(point.y>.832){const double head_pivot=fit.bind_points[boneIndex(BoneId::Head)].y;
        const double neck_joint=fit.bind_points[boneIndex(BoneId::Neck)].y;
        const double head=smooth((point.y-(head_pivot-.022))/.030);
        const double chest=(1.0-smooth((point.y-(neck_joint-.010))/.030))*(1.0-head);
        return {{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),chest},
                {static_cast<std::uint16_t>(boneIndex(BoneId::Neck)),1.0-head-chest},
                {static_cast<std::uint16_t>(boneIndex(BoneId::Head)),head}};}
    return {{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),1.0}};
}

} // namespace

foundation::Result<ReferenceAvatarSurface,foundation::Error> ReferenceBodySurfaceGenerator::build(
    const EquipmentFit& fit,const SkeletonData& skeleton,foundation::Color uniform,
    std::uint32_t detail_level) {
    if(detail_level<1U||detail_level>3U||!fit.valid(skeleton))return foundation::Result<ReferenceAvatarSurface,foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument,"invalid reference avatar surface request"});
    ReferenceSurfaceBuilder builder(fit.body.reference_height,kRigBoneCount);builder.setOmitFacialMorphs(detail_level==1U);
    ReferenceJacketTopology topology{};(void)appendJacket(builder,fit,skeleton,uniform,detail_level,&topology);
    const auto skin=referenceRgb(fit.body.skin_color_hex),glove=referenceRgb(0x3d4136U),boot=referenceRgb(0x302d29U);
    const auto palm=fit.gloves_present?glove:skin,digit=fit.gloves_present&&!fit.gloves_fingerless?glove:skin;
    (void)appendSleeve(builder,topology,fit,uniform,true);(void)appendHand(builder,fit,true,detail_level,palm,digit);
    (void)appendSleeve(builder,topology,fit,uniform,false);(void)appendHand(builder,fit,false,detail_level,palm,digit);
    (void)appendPants(builder,fit,uniform,detail_level);(void)appendBoot(builder,fit,true,detail_level,boot);
    (void)appendBoot(builder,fit,false,detail_level,boot);(void)appendClothDetails(builder,fit,uniform,detail_level);
    (void)ReferenceFaceSurfaceGenerator::appendShell(builder,fit,skin,detail_level);
    constexpr std::array<std::string_view,4U> names{"eyelidsClose","eyelidsArc","neckFlex","handsRelax"};
    ReferenceAvatarSurface result{};for(std::size_t index=0;index<names.size();++index){result.morphs[index].name=names[index];
        result.morphs[index].position_deltas=builder.morphPositions(names[index]);}
    result.mesh=builder.finalize();result.mesh.materials={
        {"body",uniform,.95F,0.0F,1.0F,false},
        {"skin",fit.body.skin_color,.95F,0.0F,1.0F,false},
        {"hair",{.035F,.025F,.018F,1.0F},.9F,0.0F,1.0F,false}};
    for(std::size_t index=0;index<names.size();++index){result.morphs[index].normal_deltas=
        builder.morphNormals(names[index],result.mesh);}
    return foundation::Result<ReferenceAvatarSurface,foundation::Error>::success(std::move(result));
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendJacket(
    ReferenceSurfaceBuilder& builder,const EquipmentFit& fit,const SkeletonData&,
    foundation::Color uniform,std::uint32_t detail_level,ReferenceJacketTopology* topology) {
    const std::size_t first_vertex=builder.vertexCount();
    const std::uint32_t segments=detail_level==3U?32U:detail_level==1U?16U:24U;
    const std::uint32_t column_span=detail_level==3U?4U:detail_level==1U?2U:3U;
    const double shoulder_y=fit.bind_points[boneIndex(BoneId::UpperArmL)].y;
    const double neck_y=fit.bind_points[boneIndex(BoneId::Neck)].y;
    const double shoulder_half=std::abs(fit.bind_points[boneIndex(BoneId::UpperArmL)].x);
    const ReferenceColor base{uniform.r,uniform.g,uniform.b};
    std::vector<ReferenceSurfaceBuilder::Ring> rings;
    rings.reserve(fit.jacket.size());builder.setTag("jacket");
    for(std::size_t row=0U;row<fit.jacket.size();++row){const auto& section=fit.jacket[row];
        ReferenceSurfaceBuilder::Ring ring;ring.reserve(segments);
        for(std::uint32_t column=0U;column<segments;++column){const double angle=
            6.283185307179586476925286766559*static_cast<double>(column)/static_cast<double>(segments);
            ReferenceVec3 point{section.half_width*std::cos(angle),section.y,
                                section.half_depth*std::sin(angle)};
            auto weights=torsoWeights(fit,point);
            if(point.y>shoulder_y-.055&&point.y<neck_y-.005&&
               std::abs(point.x)>shoulder_half*.65){const bool left=point.x>0.0;
                const double t=std::clamp((std::abs(point.x)-shoulder_half*.65)/
                    std::max(.02,shoulder_half*.48),0.0,.55);
                for(auto& weight:weights)weight.weight*=1.0-t;
                weights.push_back({static_cast<std::uint16_t>(boneIndex(left?BoneId::ClavicleL:BoneId::ClavicleR)),
                                   t*.46});
                weights.push_back({static_cast<std::uint16_t>(boneIndex(left?BoneId::UpperArmL:BoneId::UpperArmR)),
                                   t*.54});}
            const double factor=1.0+.026*std::sin(point.y*310.0)+.012*std::cos(point.x*213.0);
            ring.push_back(builder.vertex(point,weights,{std::min(1.0,base.r*factor),
                std::min(1.0,base.g*factor),std::min(1.0,base.b*factor)},
                {std::cos(angle),0.0,std::sin(angle)},
                {static_cast<double>(column)/segments*3.0,point.y*22.0},0U));}
        rings.push_back(std::move(ring));}
    const auto in_port=[&](std::uint32_t row,std::uint32_t column){if(row<7U||row>=10U)return false;
        const auto near=[&](std::int32_t value){const auto wrapped=(value+static_cast<std::int32_t>(segments))%
            static_cast<std::int32_t>(segments);return wrapped<static_cast<std::int32_t>(column_span)||
            wrapped>=static_cast<std::int32_t>(segments-column_span);};
        return near(static_cast<std::int32_t>(column))||near(static_cast<std::int32_t>(column)-
            static_cast<std::int32_t>(segments/2U));};
    std::size_t triangles=0U;
    for(std::uint32_t row=0U;row+1U<rings.size();++row)for(std::uint32_t column=0U;column<segments;++column){
        if(in_port(row,column))continue;const auto next=(column+1U)%segments;
        builder.triangle(rings[row][column],rings[row+1U][column],rings[row][next]);
        builder.triangle(rings[row][next],rings[row+1U][column],rings[row+1U][next]);triangles+=2U;}
    const std::array<ReferenceInfluence,1U> hips{{{static_cast<std::uint16_t>(boneIndex(BoneId::Hips)),1.0}}};
    builder.cap(rings.front(),hips,base,{0.0,-1.0,0.0});triangles+=segments;
    if(topology!=nullptr){topology->rings=std::move(rings);topology->segments=segments;
        topology->port_span=column_span;}
    return {builder.vertexCount()-first_vertex,triangles};
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendSleeve(
    ReferenceSurfaceBuilder& builder,const ReferenceJacketTopology& topology,
    const EquipmentFit& fit,foundation::Color uniform,bool left) {
    const std::size_t first_vertex=builder.vertexCount();
    const auto segments=topology.segments,span=topology.port_span;
    const std::int32_t center=left?0:static_cast<std::int32_t>(segments/2U);
    const auto ring_index=[&](std::size_t row,std::int32_t column){const auto wrapped=
        (column+static_cast<std::int32_t>(segments))%static_cast<std::int32_t>(segments);
        return topology.rings[row][static_cast<std::size_t>(wrapped)];};
    ReferenceSurfaceBuilder::Ring port;
    for(std::int32_t column=center-static_cast<std::int32_t>(span);
        column<=center+static_cast<std::int32_t>(span);++column)port.push_back(ring_index(7U,column));
    for(std::size_t row=8U;row<=10U;++row)port.push_back(ring_index(row,center+static_cast<std::int32_t>(span)));
    for(std::int32_t column=center+static_cast<std::int32_t>(span)-1;
        column>=center-static_cast<std::int32_t>(span);--column)port.push_back(ring_index(10U,column));
    for(std::size_t row=9U;row>7U;--row)port.push_back(ring_index(row,center-static_cast<std::int32_t>(span)));

    const BoneId shoulder_id=left?BoneId::UpperArmL:BoneId::UpperArmR;
    const BoneId elbow_id=left?BoneId::ForeArmL:BoneId::ForeArmR;
    const BoneId wrist_id=left?BoneId::HandL:BoneId::HandR;
    const BoneId clavicle_id=left?BoneId::ClavicleL:BoneId::ClavicleR;
    const auto to_reference=[&](BoneId id){const auto point=fit.bind_points[boneIndex(id)];
        return ReferenceVec3{point.x,point.y,point.z};};
    const auto shoulder=to_reference(shoulder_id),elbow=to_reference(elbow_id),wrist=to_reference(wrist_id);
    const auto difference=[](ReferenceVec3 a,ReferenceVec3 b){return ReferenceVec3{a.x-b.x,a.y-b.y,a.z-b.z};};
    const auto length=[](ReferenceVec3 value){return std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);};
    auto direction=difference(elbow,shoulder);const double direction_length=length(direction);
    direction={direction.x/direction_length,direction.y/direction_length,direction.z/direction_length};
    ReferenceVec3 u{direction.y,-direction.x,0.0};const double u_length=length(u);
    u={u.x/u_length,u.y/u_length,0.0};const ReferenceVec3 v{0.0,0.0,1.0};
    ReferenceVec3 port_center{};for(const auto vertex:port){const auto point=builder.point(vertex);
        port_center.x+=point.x;port_center.y+=point.y;port_center.z+=point.z;}
    const double inverse=1.0/static_cast<double>(port.size());port_center.x*=inverse;
    port_center.y*=inverse;port_center.z*=inverse;
    std::vector<std::array<double,2U>> projections;projections.reserve(port.size());
    for(const auto vertex:port){const auto point=difference(builder.point(vertex),port_center);
        projections.push_back({point.x*u.x+point.y*u.y+point.z*u.z,
                               point.x*v.x+point.y*v.y+point.z*v.z});}
    double area=0.0;for(std::size_t index=0;index<projections.size();++index){const auto& a=projections[index];
        const auto& b=projections[(index+1U)%projections.size()];area+=a[0]*b[1]-a[1]*b[0];}
    const double start_angle=std::atan2(projections[0][1],projections[0][0]);
    std::vector<double> angles;angles.reserve(port.size());const double orientation=area<0.0?-1.0:1.0;
    for(std::size_t index=0;index<port.size();++index)angles.push_back(start_angle+orientation*
        6.283185307179586476925286766559*static_cast<double>(index)/static_cast<double>(port.size()));
    const double total=length(difference(wrist,shoulder)),elbow_distance=length(difference(elbow,shoulder));
    const double thickness=fit.body.arm_thickness_scale;
    constexpr std::array<std::array<double,3U>,15U> profile{{
        {{-.012,.023,.027}},{{.008,.038,.039}},{{.028,.044,.044}},{{.057,.043,.041}},
        {{.102,.039,.039}},{{.141,.035,.036}},{{.164,.035,.034}},{{.176,.034,.033}},
        {{.188,.033,.032}},{{.201,.033,.031}},{{.226,.033,.031}},{{.262,.030,.029}},
        {{.296,.027,.021}},{{.322,.025,.016}},{{.337,.0255,.013}}}};
    const ReferenceColor base{uniform.r,uniform.g,uniform.b};builder.setTag(left?"sleeve.L":"sleeve.R");
    auto previous=port;std::size_t triangles=0U;
    const auto append_ring=[&](double distance,double radius_x,double radius_z,
                               std::span<const ReferenceInfluence> weights,ReferenceColor color,
                               double uv_y,bool varying=false){ReferenceSurfaceBuilder::Ring ring;ring.reserve(previous.size());
        for(std::size_t index=0;index<previous.size();++index){const double angle=angles[index],xx=std::cos(angle),zz=std::sin(angle);
            const ReferenceVec3 center{shoulder.x+direction.x*distance,shoulder.y+direction.y*distance,
                                       shoulder.z+direction.z*distance};
            const ReferenceVec3 point{center.x+u.x*radius_x*xx+v.x*radius_z*zz,
                center.y+u.y*radius_x*xx+v.y*radius_z*zz,center.z+u.z*radius_x*xx+v.z*radius_z*zz};
            ReferenceVec3 normal{u.x*xx/std::max(radius_x,1e-5)+v.x*zz/std::max(radius_z,1e-5),
                u.y*xx/std::max(radius_x,1e-5)+v.y*zz/std::max(radius_z,1e-5),
                u.z*xx/std::max(radius_x,1e-5)+v.z*zz/std::max(radius_z,1e-5)};
            const double normal_length=length(normal);normal={normal.x/normal_length,normal.y/normal_length,normal.z/normal_length};
            ReferenceColor vertex_color=color;if(varying){const double factor=1.0+.026*std::sin(point.y*310.0)+.012*std::cos(point.x*213.0);
                vertex_color={std::min(1.0,base.r*factor),std::min(1.0,base.g*factor),std::min(1.0,base.b*factor)};}
            ring.push_back(builder.vertex(point,weights,vertex_color,normal,
                {static_cast<double>(index)/previous.size()*3.0,uv_y},0U));}return ring;};
    for(const auto& row:profile){const double distance=row[0]<0.0?row[0]:row[0]/.337*total;
        std::vector<ReferenceInfluence> weights;const double blend=smooth((distance-(elbow_distance-.018))/.052);
        weights={{static_cast<std::uint16_t>(boneIndex(shoulder_id)),1.0-blend},
                 {static_cast<std::uint16_t>(boneIndex(elbow_id)),blend}};
        if(distance<.04){const double arm_weight=.45+(.72-.45)*smooth((distance+.014)/.055);
            const double upper=smooth((distance+.012)/.044);weights={{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),1.0-arm_weight},
                {static_cast<std::uint16_t>(boneIndex(clavicle_id)),arm_weight*(1.0-upper)},
                {static_cast<std::uint16_t>(boneIndex(shoulder_id)),arm_weight*upper}};}
        auto ring=append_ring(distance,row[1]*thickness,row[2]*thickness,weights,base,distance*19.0,true);
        builder.bridge(previous,ring);triangles+=previous.size()*2U;previous=std::move(ring);}
    const std::array<ReferenceInfluence,1U> forearm{{{static_cast<std::uint16_t>(boneIndex(elbow_id)),1.0}}};
    builder.cap(previous,forearm,{base.r*.82,base.g*.82,base.b*.82},direction);triangles+=previous.size();
    const auto cuff_a=append_ring(total-.012,.0262*thickness,.014*thickness,forearm,
        {base.r*.72,base.g*.72,base.b*.72},0.0);
    const auto cuff_b=append_ring(total,.0262*thickness,.014*thickness,forearm,
        {base.r*.80,base.g*.80,base.b*.80},1.0);
    builder.bridge(cuff_a,cuff_b);triangles+=cuff_a.size()*2U;
    return {builder.vertexCount()-first_vertex,triangles};
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendHand(
    ReferenceSurfaceBuilder& builder,const EquipmentFit& fit,bool left,
    std::uint32_t detail_level,foundation::Color hand_color,
    foundation::Color digit_color_value) {
    const std::size_t first_vertex=builder.vertexCount();
    const BoneId wrist_id=left?BoneId::HandL:BoneId::HandR;
    const BoneId elbow_id=left?BoneId::ForeArmL:BoneId::ForeArmR;
    const auto wrist_value=fit.bind_points[boneIndex(wrist_id)];
    const auto elbow_value=fit.bind_points[boneIndex(elbow_id)];
    const ReferenceVec3 wrist{wrist_value.x,wrist_value.y,wrist_value.z};
    ReferenceVec3 direction{wrist_value.x-elbow_value.x,wrist_value.y-elbow_value.y,
                            wrist_value.z-elbow_value.z};
    const auto length=[](ReferenceVec3 value){return std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);};
    const double direction_length=length(direction);direction={direction.x/direction_length,
        direction.y/direction_length,direction.z/direction_length};
    ReferenceVec3 u{direction.y,-direction.x,0.0};const double u_length=length(u);
    u={u.x/u_length,u.y/u_length,0.0};const ReferenceVec3 z_axis{0.0,0.0,1.0};
    const double hand_scale=fit.body.hand_scale;const std::uint32_t palm_segments=
        detail_level==3U?16U:detail_level==1U?6U:10U;
    const std::uint32_t digit_segments=detail_level==3U?10U:detail_level==1U?4U:6U;
    const ReferenceColor color{hand_color.r,hand_color.g,hand_color.b};
    const ReferenceColor digit_color{digit_color_value.r,digit_color_value.g,digit_color_value.b};
    const std::array<ReferenceInfluence,1U> hand_weight{{{
        static_cast<std::uint16_t>(boneIndex(wrist_id)),1.0}}};
    builder.setTag(left?"hand.L":"hand.R");
    constexpr std::array<std::array<double,3U>,4U> palm_profile{{
        {{0.0,.013,.011}},{{.017,.022,.011}},{{.042,.023,.009}},{{.058,.021,.008}}}};
    const auto append_ring=[&](ReferenceVec3 center,ReferenceVec3 ring_u,ReferenceVec3 ring_v,
                               double radius_x,double radius_z,std::uint32_t segments,
                               std::span<const ReferenceInfluence> weights,ReferenceColor ring_color,
                               double uv_y){
        ReferenceSurfaceBuilder::Ring ring;ring.reserve(segments);
        for(std::uint32_t index=0U;index<segments;++index){const double angle=
            6.283185307179586476925286766559*static_cast<double>(index)/segments;
            const double xx=std::cos(angle),zz=std::sin(angle);
            const ReferenceVec3 point{center.x+ring_u.x*radius_x*xx+ring_v.x*radius_z*zz,
                center.y+ring_u.y*radius_x*xx+ring_v.y*radius_z*zz,
                center.z+ring_u.z*radius_x*xx+ring_v.z*radius_z*zz};
            ReferenceVec3 normal{ring_u.x*xx/std::max(radius_x,1e-5)+ring_v.x*zz/std::max(radius_z,1e-5),
                ring_u.y*xx/std::max(radius_x,1e-5)+ring_v.y*zz/std::max(radius_z,1e-5),
                ring_u.z*xx/std::max(radius_x,1e-5)+ring_v.z*zz/std::max(radius_z,1e-5)};
            const double normal_length=length(normal);normal={normal.x/normal_length,normal.y/normal_length,normal.z/normal_length};
            ring.push_back(builder.vertex(point,weights,ring_color,normal,
                {static_cast<double>(index)/segments*3.0,uv_y},0U));}return ring;};
    ReferenceSurfaceBuilder::Ring previous;std::size_t triangles=0U;
    for(const auto& row:palm_profile){const double distance=row[0]*hand_scale;
        const ReferenceVec3 center{wrist.x+direction.x*distance,wrist.y+direction.y*distance,
                                   wrist.z+direction.z*distance};
        auto ring=append_ring(center,u,z_axis,row[1]*hand_scale,row[2]*hand_scale,
                              palm_segments,hand_weight,color,distance*10.0);
        if(!previous.empty()){builder.bridge(previous,ring,1U);triangles+=previous.size()*2U;}
        previous=std::move(ring);}
    builder.cap(previous,hand_weight,color,direction,1U);triangles+=previous.size();

    const std::array<double,4U> lengths=left?std::array<double,4U>{.029,.039,.042,.033}:
                                                std::array<double,4U>{.033,.042,.039,.029};
    const std::array<BoneId,5U> roots=left?
        std::array<BoneId,5U>{BoneId::FingerLLittle0,BoneId::FingerLRing0,
            BoneId::FingerLMiddle0,BoneId::FingerLIndex0,BoneId::FingerLThumb0}:
        std::array<BoneId,5U>{BoneId::FingerRIndex0,BoneId::FingerRMiddle0,
            BoneId::FingerRRing0,BoneId::FingerRLittle0,BoneId::FingerRThumb0};
    const auto normalized=[&](ReferenceVec3 value){const double magnitude=length(value);
        return ReferenceVec3{value.x/magnitude,value.y/magnitude,value.z/magnitude};};
    constexpr std::array<std::array<double,2U>,5U> digit_profile{{
        {{0.0,1.0}},{{.32,1.04}},{{.58,.96}},{{.84,.88}},{{1.0,.28}}}};
    for(std::size_t digit=0U;digit<5U;++digit){const bool thumb=digit==4U;
        const std::size_t first_digit=builder.vertexCount();
        const double side_sign=left?1.0:-1.0;
        const double offset=thumb?side_sign*.017:(static_cast<double>(digit)-1.5)*.0101;
        const double start_distance=(thumb?.024:.052)*hand_scale;
        const ReferenceVec3 start{wrist.x+direction.x*start_distance+u.x*offset*hand_scale,
            wrist.y+direction.y*start_distance+u.y*offset*hand_scale,
            wrist.z+direction.z*start_distance+u.z*offset*hand_scale};
        ReferenceVec3 axis{direction.x+z_axis.x*(thumb?.22:.08),
                           direction.y+z_axis.y*(thumb?.22:.08),
                           direction.z+z_axis.z*(thumb?.22:.08)};
        if(thumb){axis.x+=u.x*side_sign*.75;axis.y+=u.y*side_sign*.75;axis.z+=u.z*side_sign*.75;}
        axis=normalized(axis);ReferenceVec3 digit_u=normalized({axis.y,-axis.x,0.0});
        const ReferenceVec3 digit_v{digit_u.y*axis.z-digit_u.z*axis.y,
            digit_u.z*axis.x-digit_u.x*axis.z,digit_u.x*axis.y-digit_u.y*axis.x};
        const double digit_length=(thumb?.034:lengths[digit])*hand_scale;
        const double radius=(thumb?.007:(digit==3U?.0045:.0049))*hand_scale;
        ReferenceSurfaceBuilder::Ring digit_previous;
        for(const auto& row:digit_profile){const double t=row[0];std::vector<ReferenceInfluence> weights;
            const auto bone0=static_cast<std::uint16_t>(boneIndex(roots[digit]));
            const auto bone1=static_cast<std::uint16_t>(boneIndex(roots[digit])+1U);
            const auto bone2=static_cast<std::uint16_t>(boneIndex(roots[digit])+2U);
            if(t<.26)weights={{bone0,1.0}};else if(t<.38){const double blend=(t-.26)/.12;
                weights={{bone0,1.0-blend},{bone1,blend}};}else if(t<.59)weights={{bone1,1.0}};
            else if(t<.71){const double blend=(t-.59)/.12;weights={{bone1,1.0-blend},{bone2,blend}};}
            else weights={{bone2,1.0}};
            const ReferenceVec3 center{start.x+axis.x*digit_length*t,start.y+axis.y*digit_length*t,
                                       start.z+axis.z*digit_length*t};
            auto ring=append_ring(center,digit_u,digit_v,radius*row[1],radius*.80*row[1],
                                  digit_segments,weights,digit_color,t);
            if(!digit_previous.empty()){builder.bridge(digit_previous,ring,1U);triangles+=digit_previous.size()*2U;}
            digit_previous=std::move(ring);}
        const std::array<ReferenceInfluence,1U> tip{{{static_cast<std::uint16_t>(
            boneIndex(roots[digit])+2U),1.0}}};
        builder.cap(digit_previous,tip,digit_color,axis,1U);triangles+=digit_previous.size();
        const double curl=thumb?.9:2.25;for(std::size_t index=first_digit;index<builder.vertexCount();++index){const auto point=builder.point(static_cast<std::uint32_t>(index));
            const ReferenceVec3 offset{point.x-start.x,point.y-start.y,point.z-start.z};const double projection=offset.x*axis.x+offset.y*axis.y+offset.z*axis.z,
                t=std::clamp(projection/digit_length,0.0,1.0),angle=curl*t;const ReferenceVec3 center{
                    start.x+axis.x*digit_length*std::sin(angle)/curl+digit_v.x*digit_length*(1.0-std::cos(angle))/curl,
                    start.y+axis.y*digit_length*std::sin(angle)/curl+digit_v.y*digit_length*(1.0-std::cos(angle))/curl,
                    start.z+axis.z*digit_length*std::sin(angle)/curl+digit_v.z*digit_length*(1.0-std::cos(angle))/curl},
                normal{digit_v.x*std::cos(angle)-axis.x*std::sin(angle),digit_v.y*std::cos(angle)-axis.y*std::sin(angle),
                    digit_v.z*std::cos(angle)-axis.z*std::sin(angle)};const double across=offset.x*digit_u.x+offset.y*digit_u.y+offset.z*digit_u.z,
                depth=offset.x*digit_v.x+offset.y*digit_v.y+offset.z*digit_v.z;const ReferenceVec3 target{center.x+digit_u.x*across+normal.x*depth,
                    center.y+digit_u.y*across+normal.y*depth,center.z+digit_u.z*across+normal.z*depth};builder.morph("handsRelax",static_cast<std::uint32_t>(index),
                    {target.x-point.x,target.y-point.y,target.z-point.z});}
    }
    return {builder.vertexCount()-first_vertex,triangles};
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendPants(
    ReferenceSurfaceBuilder& builder,const EquipmentFit& fit,foundation::Color uniform,
    std::uint32_t detail_level) {
    const std::size_t first_vertex=builder.vertexCount();std::size_t triangles=0U;
    const std::uint32_t segments=detail_level==3U?32U:detail_level==1U?16U:24U;
    const auto map_leg_y=[&](double y){return .045+(y-.045)*(fit.body.hip_y-.045)/(.54-.045);};
    const auto mix=[](double a,double b,double t){return a+(b-a)*t;};
    const double leg_thickness=fit.body.leg_thickness_scale*fit.pants_ease;
    const ReferenceColor base{uniform.r,uniform.g,uniform.b};
    const auto shade=[&](double factor){return ReferenceColor{std::min(1.0,base.r*factor),
        std::min(1.0,base.g*factor),std::min(1.0,base.b*factor)};};
    const auto ring=[&](ReferenceVec3 center,double rx,double rz,std::size_t count,
                        const auto& weights,ReferenceColor color,double uv_y,
                        const std::vector<double>* angles=nullptr){ReferenceSurfaceBuilder::Ring out;
        out.reserve(count);for(std::size_t index=0U;index<count;++index){const double angle=angles?
            (*angles)[index]:6.283185307179586476925286766559*static_cast<double>(index)/count;
            const double xx=std::cos(angle),zz=std::sin(angle);ReferenceVec3 point{
                center.x+rx*xx,center.y,center.z+rz*zz};auto influences=weights(point);
            out.push_back(builder.vertex(point,influences,color,{xx,0.0,zz},
                {static_cast<double>(index)/count*3.0,uv_y},0U));}return out;};
    builder.setTag("trousers");ReferenceSurfaceBuilder::Ring previous,last;
    constexpr std::array<std::array<double,3U>,5U> profiles{{{{.565,.099,.061}},
        {{.543,.106,.064}},{{.520,.109,.067}},{{.491,.108,.066}},{{.468,.103,.059}}}};
    for(const auto& row:profiles){const double hip_t=smooth((row[0]-.468)/(.565-.468));
        const double y=map_leg_y(row[0]);const auto weights=[&](ReferenceVec3 point){
            const double lower=map_leg_y(.468),t=smooth((fit.body.hip_y-point.y)/
                std::max(.001,static_cast<double>(fit.body.hip_y)-lower))*.65;
            const BoneId thigh=point.x>=0.0?BoneId::ThighL:BoneId::ThighR;
            return std::vector<ReferenceInfluence>{{static_cast<std::uint16_t>(boneIndex(BoneId::Hips)),1.0-t},
                {static_cast<std::uint16_t>(boneIndex(thigh)),t}};};
        auto loop=ring({0.0,y,0.0},row[1]*mix(leg_thickness,
            fit.body.hip_width_scale,hip_t),row[2]*mix(leg_thickness,
            (fit.body.hip_width_scale+fit.body.waist_depth_scale)*.5,hip_t),segments,
            weights,shade(.91),y*18.0);
        if(!previous.empty()){builder.bridge(previous,loop);triangles+=previous.size()*2U;}
        previous=loop;last=std::move(loop);}
    const std::size_t front=segments/4U,back=3U*segments/4U;
    ReferenceSurfaceBuilder::Ring seam{last[front]};const std::size_t seam_count=
        detail_level==3U?8U:detail_level==1U?4U:6U;
    const std::array<ReferenceInfluence,3U> seam_weights{{
        {static_cast<std::uint16_t>(boneIndex(BoneId::Hips)),.45},
        {static_cast<std::uint16_t>(boneIndex(BoneId::ThighL)),.275},
        {static_cast<std::uint16_t>(boneIndex(BoneId::ThighR)),.275}}};
    for(std::size_t index=1U;index<seam_count;++index){const double t=static_cast<double>(index)/seam_count;
        seam.push_back(builder.vertex({0.0,map_leg_y(.468-.025*std::sin(t*3.14159265358979323846)),
            mix(.059,-.059,t)*leg_thickness},seam_weights,shade(.84),{0.0,-1.0,0.0},{0.0,t}));}
    seam.push_back(last[back]);const double hip_x=std::abs(fit.bind_points[boneIndex(BoneId::ThighL)].x);
    constexpr std::array<std::array<double,3U>,13U> legs{{{{.438,.052,.055}},{{.407,.050,.052}},
        {{.370,.045,.047}},{{.330,.038,.042}},{{.305,.035,.037}},{{.293,.034,.035}},
        {{.282,.034,.034}},{{.270,.034,.034}},{{.249,.035,.036}},{{.219,.037,.037}},
        {{.184,.034,.034}},{{.151,.0305,.032}},{{.134,.0305,.032}}}};
    for(const bool left:{true,false}){const double sign=left?1.0:-1.0;builder.setTag(left?"leg.L":"leg.R");
        ReferenceSurfaceBuilder::Ring loop;if(left){for(std::size_t k=back;k<=segments+front;++k)loop.push_back(last[k%segments]);
            loop.insert(loop.end(),seam.begin()+1,seam.end()-1);}else{for(std::size_t k=front;k<=back;++k)loop.push_back(last[k]);
            loop.insert(loop.end(),std::make_reverse_iterator(seam.end()-1),std::make_reverse_iterator(seam.begin()+1));}
        const ReferenceVec3 center{sign*hip_x,map_leg_y(.455),0.0};std::vector<std::array<double,2U>> projections;
        for(const auto vertex:loop){const auto point=builder.point(vertex);projections.push_back({point.x-center.x,point.z});}
        double area=0.0;for(std::size_t index=0;index<projections.size();++index){const auto& a=projections[index];
            const auto& b=projections[(index+1U)%projections.size()];area+=a[0]*b[1]-a[1]*b[0];}
        const double start=std::atan2(projections[0][1],projections[0][0]);std::vector<double> angles;
        for(std::size_t index=0;index<loop.size();++index)angles.push_back(start+(area<0.0?-1.0:1.0)*
            6.283185307179586476925286766559*static_cast<double>(index)/loop.size());
        auto last_loop=loop;const BoneId thigh=left?BoneId::ThighL:BoneId::ThighR;
        const BoneId shin=left?BoneId::ShinL:BoneId::ShinR;const double knee_y=fit.bind_points[boneIndex(shin)].y;
        for(const auto& row:legs){const double y=map_leg_y(row[0]);const double knee=smooth((knee_y-y)/
                std::max(.018,.058*(static_cast<double>(fit.body.hip_y)/.54)));
            const auto weights=[&](ReferenceVec3){return std::vector<ReferenceInfluence>{
                {static_cast<std::uint16_t>(boneIndex(thigh)),1.0-knee},
                {static_cast<std::uint16_t>(boneIndex(shin)),knee}};};
            const double calf=1.0+.08*(fit.body.muscle-1.0),taper=mix(1.0,calf,smooth((.28-row[0])/.10));
            const double factor=.90+.025*std::sin(y*370.0);auto next=ring({sign*hip_x,y,0.0},
                row[1]*leg_thickness*taper,row[2]*leg_thickness*taper,
                last_loop.size(),weights,shade(factor),y*24.0,&angles);
            builder.bridge(last_loop,next);triangles+=last_loop.size()*2U;last_loop=std::move(next);}
        const std::array<ReferenceInfluence,1U> shin_weight{{{static_cast<std::uint16_t>(boneIndex(shin)),1.0}}};
        builder.cap(last_loop,shin_weight,shade(.7),{0.0,-1.0,0.0});triangles+=last_loop.size();}
    return {builder.vertexCount()-first_vertex,triangles};
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendBoot(
    ReferenceSurfaceBuilder& builder,const EquipmentFit& fit,bool left,
    std::uint32_t detail_level,foundation::Color boot_color) {
    const std::size_t first_vertex=builder.vertexCount();std::size_t triangles=0U;
    const std::size_t segments=detail_level==3U?28U:detail_level==1U?8U:16U;
    const double sign=left?1.0:-1.0,foot_x=std::abs(fit.bind_points[boneIndex(
        left?BoneId::FootL:BoneId::FootR)].x),foot_scale=fit.body.foot_scale;
    const BoneId foot=left?BoneId::FootL:BoneId::FootR,toes=left?BoneId::ToesL:BoneId::ToesR,
        shin=left?BoneId::ShinL:BoneId::ShinR;
    const auto map_leg_y=[&](double y){return .045+(y-.045)*(fit.body.hip_y-.045)/(.54-.045);};
    const ReferenceColor base{boot_color.r,boot_color.g,boot_color.b};const auto shade=[&](double value){
        return ReferenceColor{std::min(1.0,base.r*value),std::min(1.0,base.g*value),
            std::min(1.0,base.b*value)};};
    constexpr std::array<std::array<double,4U>,10U> levels{{{{0,.034,.087,.041}},
        {{.010,.034,.087,.041}},{{.016,.033,.086,.041}},{{.023,.032,.083,.040}},
        {{.032,.031,.078,.038}},{{.045,.030,.065,.024}},{{.064,.029,.039,.006}},
        {{.088,.029,.032,0}},{{.119,.031,.033,0}},{{.151,.0335,.034,0}}}};
    ReferenceSurfaceBuilder::Ring previous;
    for(std::size_t level=0U;level<levels.size();++level){const auto& row=levels[level];
        const double y=row[0]<=.045?row[0]:(row[0]>.10?map_leg_y(.10)+
            (map_leg_y(row[0])-map_leg_y(.10))*fit.boot_shaft:map_leg_y(row[0]));
        builder.setTag(level==0U?(left?"sole.L":"sole.R"):(left?"boot.L":"boot.R"));
        ReferenceSurfaceBuilder::Ring loop;loop.reserve(segments);const double sx=foot_scale*
            (.95+.05*fit.body.leg_thickness_scale*fit.pants_ease)*fit.boot_width;
        for(std::size_t index=0U;index<segments;++index){const double angle=
            6.283185307179586476925286766559*static_cast<double>(index)/segments;
            double xx=std::cos(angle),zz=std::sin(angle);xx=std::copysign(std::pow(std::abs(xx),.88),xx)-
                sign*.055*std::pow(std::max(0.0,zz),2.0);zz=std::copysign(std::pow(std::abs(zz),.88),zz);
            const ReferenceVec3 point{sign*foot_x+row[1]*sx*xx,y,
                row[3]*foot_scale+row[2]*foot_scale*fit.boot_width*zz};
            std::vector<ReferenceInfluence> weights;if(point.y>map_leg_y(.070)){const double t=smooth(
                (point.y-map_leg_y(.07))/std::max(.01,map_leg_y(.135)-map_leg_y(.07)));
                weights={{static_cast<std::uint16_t>(boneIndex(foot)),1.0-t},
                    {static_cast<std::uint16_t>(boneIndex(shin)),t}};}else{const double t=smooth(
                (point.z/foot_scale-.062)/.054);weights={
                    {static_cast<std::uint16_t>(boneIndex(foot)),1.0-t},
                    {static_cast<std::uint16_t>(boneIndex(toes)),t}};}
            loop.push_back(builder.vertex(point,weights,level<3U?shade(.47):shade(1.0+(level%2U)*.11),
                {xx,0.0,zz},{static_cast<double>(index)/segments*3.0,y*15.0},0U));}
        if(previous.empty()){const std::array<ReferenceInfluence,1U> weight{{{
            static_cast<std::uint16_t>(boneIndex(foot)),1.0}}};builder.cap(loop,weight,shade(.4),
            {0.0,-1.0,0.0},2U);triangles+=loop.size();}else{builder.bridge(previous,loop,2U);triangles+=loop.size()*2U;}
        previous=std::move(loop);}
    builder.setTag(left?"boot.L":"boot.R");const std::array<ReferenceInfluence,1U> shin_weight{{{
        static_cast<std::uint16_t>(boneIndex(shin)),1.0}}};builder.cap(previous,shin_weight,shade(.45),
        {0.0,1.0,0.0},2U);triangles+=previous.size();
    if(detail_level==3U){const std::array<ReferenceInfluence,2U> lace_weights{{
        {static_cast<std::uint16_t>(boneIndex(shin)),.3},{static_cast<std::uint16_t>(boneIndex(foot)),.7}}};
        for(std::size_t lace=0U;lace<5U;++lace){const double y=map_leg_y(.065+lace*.013),
            z=(.037-lace*.0008)*foot_scale;const ReferenceVec3 a{sign*foot_x-.016*foot_scale,y-.004,z},
            b{sign*foot_x+.016*foot_scale,y+.004,z+.001};ReferenceVec3 direction{b.x-a.x,b.y-a.y,b.z-a.z};
            const double length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
            direction={direction.x/length,direction.y/length,direction.z/length};ReferenceVec3 u{direction.y,-direction.x,0.0};
            const double ul=std::sqrt(u.x*u.x+u.y*u.y);u={u.x/ul,u.y/ul,0.0};const ReferenceVec3 v{
                u.y*direction.z,u.z*direction.x-u.x*direction.z,
                u.x*direction.y-u.y*direction.x};
            ReferenceSurfaceBuilder::Ring first,second;const auto append_lace_ring=[&](ReferenceVec3 center,double uv_y){
                ReferenceSurfaceBuilder::Ring result;for(std::size_t index=0U;index<6U;++index){const double angle=
                6.283185307179586476925286766559*static_cast<double>(index)/6.0,c=std::cos(angle),s=std::sin(angle),r=.0013*foot_scale;
                const ReferenceVec3 offset{u.x*r*c+v.x*r*s,u.y*r*c+v.y*r*s,u.z*r*c+v.z*r*s};
                result.push_back(builder.vertex({center.x+offset.x,center.y+offset.y,center.z+offset.z},
                    lace_weights,shade(.4),offset,{static_cast<double>(index)/2.0,uv_y}));}return result;};
            first=append_lace_ring(a,0.0);second=append_lace_ring(b,.5);
            builder.bridge(first,second,2U);triangles+=12U;}}
    return {builder.vertexCount()-first_vertex,triangles};
}

ReferenceJacketBuild ReferenceBodySurfaceGenerator::appendClothDetails(
    ReferenceSurfaceBuilder& builder,const EquipmentFit& fit,foundation::Color uniform,
    std::uint32_t detail_level) {
    const std::size_t first_vertex=builder.vertexCount();std::size_t triangles=0U;
    const ReferenceColor base{uniform.r,uniform.g,uniform.b};const auto shade=[&](double value){
        return ReferenceColor{std::min(1.0,base.r*value),std::min(1.0,base.g*value),
            std::min(1.0,base.b*value)};};const auto mix=[](double a,double b,double t){return a+(b-a)*t;};
    constexpr std::array<std::array<double,3U>,14U> jacket{{{{.504,.112,.071}},{{.519,.113,.071}},
        {{.552,.110,.067}},{{.595,.094,.056}},{{.640,.100,.059}},{{.698,.113,.066}},
        {{.735,.122,.066}},{{.759,.129,.064}},{{.785,.134,.062}},{{.809,.127,.057}},
        {{.825,.112,.051}},{{.841,.070,.040}},{{.852,.045,.036}},{{.859,.042,.034}}}};
    const auto base_profile=[&](double y){for(std::size_t i=0;i+1U<jacket.size();++i)if(y<=jacket[i+1U][0]){
        const double t=std::clamp((y-jacket[i][0])/(jacket[i+1U][0]-jacket[i][0]),0.0,1.0);
        return std::array<double,2U>{mix(jacket[i][1],jacket[i+1U][1],t),mix(jacket[i][2],jacket[i+1U][2],t)};}
        return std::array<double,2U>{jacket.back()[1],jacket.back()[2]};};
    const auto profile=[&](double y){for(std::size_t i=0;i+1U<fit.jacket.size();++i)if(y<=fit.jacket[i+1U].reference_y){
        const auto& a=fit.jacket[i];const auto& b=fit.jacket[i+1U];const double t=std::clamp((y-a.reference_y)/(b.reference_y-a.reference_y),0.0,1.0);
        return std::array<double,2U>{mix(a.reference_half_width,b.reference_half_width,t),
            mix(a.reference_half_depth,b.reference_half_depth,t)};}
        return std::array<double,2U>{fit.jacket.back().reference_half_width,
            fit.jacket.back().reference_half_depth};};
    const auto cloth_point=[&](double x,double y,double depth){
        // The JS fit maps the authoring-number y directly. Calling the public
        // float helper here quantizes y before the interpolation and creates
        // measurable normal drift on the narrow tailoring patches.
        const double yy=fit.mapTorsoYExact(y);
        const auto source=base_profile(y);
        std::array<double,2U> target{};
        bool found=false;
        for(std::size_t index=0U;index+1U<fit.jacket.size();++index)
            if(yy<=fit.jacket[index+1U].reference_y){
                const auto& a=fit.jacket[index];const auto& b=fit.jacket[index+1U];
                const double t=std::clamp((yy-a.reference_y)/(b.reference_y-a.reference_y),0.0,1.0);
                target={a.reference_half_width+(b.reference_half_width-a.reference_half_width)*t,
                        a.reference_half_depth+(b.reference_half_depth-a.reference_half_depth)*t};found=true;break;}
        if(!found)target={fit.jacket.back().reference_half_width,
            fit.jacket.back().reference_half_depth};
        const double xx=x*(target[0]/std::max(.001,source[0]));
        return ReferenceVec3{xx,yy,target[1]*std::sqrt(std::max(0.0,1.0-xx*xx/(target[0]*target[0])))+depth};};
    builder.setTag("tailoring");
    const auto patch_grid=[&](double x0,double x1,double y0,double y1,double depth,ReferenceColor color){
        const std::size_t rows=detail_level==3U?4U:detail_level==1U?1U:2U,cols=rows;
        std::vector<ReferenceSurfaceBuilder::Ring> grid;for(std::size_t row=0;row<=rows;++row){
            ReferenceSurfaceBuilder::Ring line;const double y=mix(y0,y1,static_cast<double>(row)/rows);
            for(std::size_t col=0;col<=cols;++col){const double x=mix(x0,x1,static_cast<double>(col)/cols),u=static_cast<double>(col)/cols;
                const auto point=cloth_point(x,y,depth);const auto weights=torsoWeights(fit,point);
                line.push_back(builder.vertex(point,weights,color,{0.0,0.0,1.0},{u,static_cast<double>(row)/rows}));}grid.push_back(std::move(line));}
        for(std::size_t row=0;row<rows;++row)for(std::size_t col=0;col<cols;++col){builder.triangle(grid[row][col],grid[row+1U][col],grid[row][col+1U]);
            builder.triangle(grid[row][col+1U],grid[row+1U][col],grid[row+1U][col+1U]);triangles+=2U;}};
    const auto tube=[&](const std::vector<ReferenceVec3>& points,double radius,ReferenceColor color,std::size_t segments){
        ReferenceSurfaceBuilder::Ring previous;for(std::size_t at=0;at<points.size();++at){const auto a=at+1U<points.size()?points[at]:points[at-1U];
            const auto b=at+1U<points.size()?points[at+1U]:points[at];ReferenceVec3 direction{b.x-a.x,b.y-a.y,b.z-a.z};
            const double dl=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);direction={direction.x/dl,direction.y/dl,direction.z/dl};
            const ReferenceVec3 guide=std::abs(direction.z)<.9?ReferenceVec3{0,0,1}:ReferenceVec3{0,1,0};ReferenceVec3 u{
                direction.y*guide.z-direction.z*guide.y,direction.z*guide.x-direction.x*guide.z,direction.x*guide.y-direction.y*guide.x};
            const double ul=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);u={u.x/ul,u.y/ul,u.z/ul};const ReferenceVec3 v{
                u.y*direction.z-u.z*direction.y,u.z*direction.x-u.x*direction.z,u.x*direction.y-u.y*direction.x};
            ReferenceSurfaceBuilder::Ring ring;for(std::size_t index=0;index<segments;++index){const double angle=6.283185307179586476925286766559*index/segments,c=std::cos(angle),s=std::sin(angle);
                // Match THREE.Vector3's sequential addScaledVector calls in
                // SurfaceBuilder.ring(), rather than summing the offset first.
                ReferenceVec3 point=points[at];
                point.x+=u.x*(radius*c);point.y+=u.y*(radius*c);point.z+=u.z*(radius*c);
                point.x+=v.x*(radius*s);point.y+=v.y*(radius*s);point.z+=v.z*(radius*s);
                ReferenceVec3 normal{u.x*c+v.x*s,u.y*c+v.y*s,u.z*c+v.z*s};
                const double nl=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
                normal={normal.x/nl,normal.y/nl,normal.z/nl};
                ring.push_back(builder.vertex(point,torsoWeights(fit,point),color,normal,{static_cast<double>(index)/segments*3.0,static_cast<double>(at)/points.size()}));}
            if(!previous.empty()){builder.bridge(previous,ring);triangles+=segments*2U;}previous=std::move(ring);}};
    for(const double sign:{1.0,-1.0}){const double x=sign*.054;patch_grid(x-.023,x+.023,.694,.750,.0025,shade(.88));
        patch_grid(x-.025,x+.025,.738,.755,.004,shade(.74));if(detail_level==3U){std::vector<ReferenceVec3> line,hem;
            for(std::size_t i=0;i<6U;++i)line.push_back(cloth_point(x-.021,.697+i*.009,.003));
            for(std::size_t i=0;i<6U;++i)hem.push_back(cloth_point(x-.022+i*.009,.696,.003));tube(line,.00065,shade(.57),5U);tube(hem,.0006,shade(.62),5U);}}
    patch_grid(-.005,.005,.522,.819,.0021,shade(.76));
    if(detail_level==3U)for(std::size_t button=0;button<6U;++button){const auto center=cloth_point(0,.552+button*.048,.0045);
        const auto weights=torsoWeights(fit,center);
        std::vector<ReferenceSurfaceBuilder::Ring> rings;for(std::size_t row=0;row<=4U;++row){const double phi=-1.5707963267948966+3.14159265358979323846*row/4.0;
            ReferenceSurfaceBuilder::Ring ring;for(std::size_t col=0;col<8U;++col){const double angle=6.283185307179586476925286766559*col/8.0;
                const ReferenceVec3 local{std::cos(phi)*std::cos(angle)*.0021,std::sin(phi)*.0021,std::cos(phi)*std::sin(angle)*.0012};
                ring.push_back(builder.vertex({center.x+local.x,center.y+local.y,center.z+local.z},weights,shade(.42),
                    {local.x/.0021/.0021,local.y/.0021/.0021,local.z/.0012/.0012},{static_cast<double>(col)/8.0,static_cast<double>(row)/4.0}));}rings.push_back(std::move(ring));}
        for(std::size_t row=1;row<rings.size();++row){const auto before=triangles;builder.bridge(rings[row-1U],rings[row]);
            triangles+=row==1U||row==4U?8U:16U;(void)before;}}
    builder.setTag("collar");const double ratio=(.0285*fit.body.neck_scale)/.030;
    for(const double sign:{1.0,-1.0}){const std::array<std::array<double,2U>,4U> corners{{
        {{sign*.031*ratio,.853}},{{sign*.049*ratio,.829}},{{sign*.022*ratio,.806}},{{sign*.011*ratio,.833}}}};
        std::array<ReferenceSurfaceBuilder::Ring,5U> grid;for(std::size_t row=0;row<=4U;++row){const double t=static_cast<double>(row)/4.0;
            for(std::size_t col=0;col<=4U;++col){const double u=static_cast<double>(col)/4.0,x=mix(mix(corners[0][0],corners[3][0],t),mix(corners[1][0],corners[2][0],t),u),
                y=mix(mix(corners[0][1],corners[3][1],t),mix(corners[1][1],corners[2][1],t),u);const auto point=cloth_point(x,y,.0014);
                grid[row].push_back(builder.vertex(point,torsoWeights(fit,point),shade(.80),{0,0,1},{u,t}));}}
        for(std::size_t row=0;row<4U;++row)for(std::size_t col=0;col<4U;++col){builder.triangle(grid[row][col],grid[row+1U][col],grid[row][col+1U]);
            builder.triangle(grid[row][col+1U],grid[row+1U][col],grid[row+1U][col+1U]);triangles+=2U;}}
    const double y=.858;const auto dimensions=profile(y);const std::size_t count=detail_level==3U?32U:detail_level==1U?12U:24U;
    const auto collar_ring=[&](double yy,double rx,double rz){ReferenceSurfaceBuilder::Ring ring;for(std::size_t index=0;index<count;++index){const double angle=6.283185307179586476925286766559*index/count,c=std::cos(angle),s=std::sin(angle);
        const ReferenceVec3 point{rx*c,yy,rz*s};ring.push_back(builder.vertex(point,torsoWeights(fit,point),shade(.86),{c,0,s},{static_cast<double>(index)/count*3.0,0.0}));}return ring;};
    const auto c1=collar_ring(y-.001,dimensions[0]+.0006,dimensions[1]+.0006),c2=collar_ring(y+.0007,dimensions[0]+.0005,dimensions[1]+.0005);
    builder.bridge(c1,c2);triangles+=count*2U;return {builder.vertexCount()-first_vertex,triangles};
}

} // namespace genomes::infantry
