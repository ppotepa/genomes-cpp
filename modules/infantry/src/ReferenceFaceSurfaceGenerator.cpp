#include <genomes/infantry/ReferenceFaceSurfaceGenerator.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <vector>

namespace genomes::infantry {
namespace {

[[nodiscard]] double mix(double a,double b,double t){return a+(b-a)*t;}
[[nodiscard]] double smooth(double value){value=std::clamp(value,0.0,1.0);return value*value*(3.0-2.0*value);}
[[nodiscard]] double bell(double x,double y=0.0){return std::exp(-(x*x+y*y));}
[[nodiscard]] ReferenceColor rgb(std::uint32_t hex){const auto channel=[](std::uint32_t value){const double c=static_cast<double>(value)/255.0;
    return c<.04045?c*.0773993808:std::pow(c*.9478672986+.0521327014,2.4);};return{channel((hex>>16U)&255U),channel((hex>>8U)&255U),channel(hex&255U)};}

struct Section {double y{},rx{},rz{},z{};};
struct Feature {double x{},y{},z{},w{},h{},radius{},tilt{},roundness{};};
struct Brow {ReferenceVec3 inner{},outer{};};
struct Patch {Feature feature{};double y0{},y1{},t0{},t1{},half_x{},half_y{};std::size_t r0{},r1{},c0{},c1{};};

class Layout final {
public:
    explicit Layout(const EquipmentFit& fit,bool emulate_cache):face(fit.face),height(fit.body.reference_height),cache_enabled(emulate_cache) {
        const auto& body=fit.body;
        const double neck_x=.0285*body.neck_scale,neck_z=.024*body.neck_scale;
        constexpr std::array<Section,12U> base{{
            {.881,.034,.030,.013},{.890,.039,.037,.010},{.900,.041,.041,.008},
            {.913,.045,.044,.005},{.925,.049,.046,.003},{.938,.048,.046,.002},
            {.948,.047,.046,.001},{.959,.0465,.046,0},{.973,.043,.043,-.001},
            {.986,.032,.034,-.003},{.995,.015,.018,-.004},{.999,0,0,-.004}}};
        std::array<Section,12U> shaped{};
        for(std::size_t i=0;i<base.size();++i){const double y0=base[i].y,low=smooth(std::clamp((.928-y0)/.058,0.0,1.0));
            const double temple=bell((y0-.951)/.020),forehead=smooth((y0-.948)/.038),chin=bell((y0-.887)/.015);
            double y=y0;if(y0<.925)y=.925+(y0-.925)*face.jaw_length_scale+face.chin_height*chin*.65;
            if(y0>.947)y=.999-(.999-y0)*face.head_length_scale;
            double scale=mix(1,face.jaw_width_scale,low)*mix(1,face.jaw_angle,low*low*.40);
            scale=mix(scale,face.temple_width_scale,temple*.55);scale=mix(scale,face.forehead_width_scale,forehead*.65);
            shaped[i]={y,base[i].rx*scale*face.head_width_scale*mix(1,body.head_scale,.4),
                base[i].rz*face.head_depth_scale*mix(1,body.head_scale,.4),base[i].z};}
        shaped[0].y=std::clamp(shaped[0].y,.8755,.8855);
        for(std::size_t i=1;i<shaped.size();++i)shaped[i].y=std::max(shaped[i].y,shaped[i-1].y+.003);
        levels.reserve(19U);levels.push_back({.828,neck_x*1.14,neck_z*1.12,-.002});
        levels.push_back({.841,neck_x*1.06,neck_z*1.04,-.002});levels.push_back({.853,neck_x,neck_z,-.002});
        for(const double t:{.2,.4,.6,.8}){const double blend=smooth(t);levels.push_back({mix(.853,shaped[0].y,t),
            mix(neck_x*.99,shaped[0].rx,blend),mix(neck_z*1.01,shaped[0].rz,blend),mix(.001,shaped[0].z,blend)});}
        levels.insert(levels.end(),shaped.begin(),shaped.end());
        chin_y=levels.at(7U).y;head_pivot_y=std::clamp(chin_y+.018,.892,.904);top_y=levels.back().y;
        mouth_y=std::clamp(static_cast<double>(face.mouth_y_ratio),chin_y+.014,
            static_cast<double>(face.eye_y_ratio)-.031);
        const double mouth_requested=face.mouth_width/height;
        mouth_half=std::clamp(mouth_requested*.5,.007,section(mouth_y).rx*.57);
        const double eye_w=.0083*face.eye_width_scale,eye_h=.0034*face.eye_height_scale;
        const double radius=std::max(eye_w*1.12,eye_h*1.65)*.82;
        const double minimum=radius*1.20+.003,maximum=std::max(minimum,section(face.eye_y_ratio).rx*.66);
        const double spacing=std::clamp(static_cast<double>(face.eye_spacing_ratio),minimum,maximum);
        for(std::size_t index=0;index<2U;++index){const double sign=index==0U?1.0:-1.0;
            auto& eye=eyes[index];eye.x=sign*spacing;eye.y=face.eye_y_ratio+sign*face.eye_asymmetry*.5;
            eye.radius=radius;eye.w=eye_w;eye.h=eye_h;eye.tilt=sign*face.eye_tilt;
            eye.roundness=face.eye_roundness;eye.z=front(eye.x,eye.y)-radius*.80+face.eye_depth*.16;
            const double requested=face.brow_y/height+sign*face.brow_asymmetry*.5,by=std::max(requested,eye.y+eye_h+.006);
            const double inner_x=sign*std::max(.006,spacing-.007+face.brow_spacing*.5),outer_x=sign*(spacing+.008+face.brow_spacing*.5);
            brows[index].inner={inner_x,by,front(inner_x,by)+.0012};const double outer_y=by+face.brow_tilt*.006;
            brows[index].outer={outer_x,outer_y,front(outer_x,outer_y)+.0012};}
        hair_floor=std::max({brows[0].inner.y,brows[1].inner.y,brows[0].outer.y,brows[1].outer.y})+.006;
        neck_joint_y=mix(.837,.843,std::clamp((static_cast<double>(fit.body.neck_scale)-.76)/.56,0.0,1.0));
        nose_base_y=std::max(static_cast<double>(face.eye_y_ratio)-.031*face.nose_length_scale,mouth_y+.010);
        for(std::size_t index=0;index<2U;++index){const double sign=index==0U?1.0:-1.0;
            (void)front(sign*mouth_half,mouth_y);(void)front(eyes[index].x+sign*.006,face.eye_y_ratio-.016);}
        for(const double jacket_y:{.841,.852,.859}){const double t=std::clamp((jacket_y-.504)/.355,0.0,1.0);
            (void)section(mix(static_cast<double>(fit.body.hip_y)-.036,.859,t));}
        (void)front(0.0,face.eye_y_ratio);
    }
    [[nodiscard]] Section section(double y) const {
        if(cache_enabled){const auto cached=std::find_if(section_cache.begin(),section_cache.end(),[&](const auto& entry){return entry.first==y;});
            if(cached!=section_cache.end()){const auto slot=cached->second;section_cache.erase(cached);section_cache.emplace_back(y,slot);
                const auto& value=section_cache_values[slot];return {value[1],value[2],value[3],value[4]};}}
        std::size_t lower=section_cursor;const bool inside=lower+1U<levels.size()&&y<=levels[lower+1U].y&&
            (y>levels[lower].y||(lower==0U&&y==levels[0].y));
        if(!inside){lower=0U;while(lower+1U<levels.size()&&levels[lower+1U].y<y)++lower;section_cursor=lower;}
        Section result{};
        if(lower+1U>=levels.size())result={y,0,0,levels.back().z};else {const double span=levels[lower+1U].y-levels[lower].y;
        const double t=std::clamp((y-levels[lower].y)/span,0.0,1.0),t2=t*t,t3=t2*t;
        const auto component=[](const Section& s,std::size_t i){return i==0?s.y:i==1?s.rx:i==2?s.rz:s.z;};
        const auto slope=[&](std::size_t at,std::size_t field){if(at==0U)return(component(levels[1],field)-component(levels[0],field))/(levels[1].y-levels[0].y);
            if(at+1U==levels.size())return(component(levels[at],field)-component(levels[at-1U],field))/(levels[at].y-levels[at-1U].y);
            const double dl=(component(levels[at],field)-component(levels[at-1U],field))/(levels[at].y-levels[at-1U].y),
                dr=(component(levels[at+1U],field)-component(levels[at],field))/(levels[at+1U].y-levels[at].y);
            return dl*dr<=0.0?0.0:2.0*dl*dr/(dl+dr);};
        const auto h=[&](std::size_t field){const double a=component(levels[lower],field),b=component(levels[lower+1U],field);
            return(2*t3-3*t2+1)*a+(t3-2*t2+t)*slope(lower,field)*span+(-2*t3+3*t2)*b+(t3-t2)*slope(lower+1U,field)*span;};
        result={y,std::max(0.0,h(1)),std::max(0.0,h(2)),h(3)};}
        if(cache_enabled){if(section_cache.size()>=256U)section_cache.pop_front();const auto slot=section_cache_next;
            section_cache_values[slot]={y,result.y,result.rx,result.rz,result.z};section_cache.emplace_back(y,slot);section_cache_next=(slot+1U)%256U;}
        return result;
    }
    [[nodiscard]] ReferenceVec3 pointFromSection(double y,double theta,const Section& s) const {const double sx=std::sin(theta),cz=std::cos(theta);
        const double low=smooth(std::clamp((.925-y)/.055,0.0,1.0)),chin=bell((y-(chin_y+.014))/.020),side=smooth((std::abs(sx)-.10)/.72);
        (void)low;const double x=s.rx*sx*(1.0+(face.chin_width_scale-1.0)*.30*chin*side);double z=s.z+s.rz*cz;
        if(cz>0.0&&y>.882){const double front=smooth((cz-.02)/.48),cheek=bell((std::abs(x)-.030*face.cheekbone_scale)/.014,(y-(.925+face.cheekbone_y))/.016);
            z+=(.0014+face.cheek_fullness*.42+(face.cheekbone_scale-1.0)*.0018)*cheek*front;
            z+=face.chin_projection*.30*bell(x/.023,(y-(chin_y+.014))/.020)*front;
            z+=face.brow_ridge*bell((std::abs(x)-face.eye_spacing_ratio)/.017,(y-(face.eye_y_ratio+.010))/.009)*cz;
            z+=face.midface_projection*.38*bell(x/.034,(y-.918)/.024)*cz;
            z+=face.forehead_slope*smooth((y-.947)/.045)*cz;}return{x,y,z};}
    [[nodiscard]] ReferenceVec3 point(double y,double theta) const {return pointFromSection(y,theta,section(y));}
    [[nodiscard]] double front(double x,double y) const {const auto s=section(y);const double sx=std::clamp(x/std::max(.00001,s.rx),-.9999,.9999);
        return pointFromSection(y,std::asin(sx),s).z;}
    [[nodiscard]] double hairBottom(double theta) const {const double front_weight=std::max(0.0,std::cos(theta)),side=std::abs(std::sin(theta)),back=std::max(0.0,-std::cos(theta));
        double y=face.hairline_ratio+face.temple_recession*front_weight*std::pow(side,1.8)-face.widow_peak*front_weight*std::pow(1.0-side,1.5);
        y-=back*.011;const double safety=mix(.931,hair_floor,smooth((front_weight-.25)/.50));return std::clamp(std::max(y,safety),.933,top_y-.012);}
    [[nodiscard]] std::vector<ReferenceInfluence> weights(ReferenceVec3 p) const {std::array<double,10U> value{};
        const double front_weight=smooth((p.z-.003)/.025),jaw=(1.0-smooth((p.y-(mouth_y-.002))/.018))*front_weight*.92;
        value[2]=1.0-jaw;value[3]=jaw;if(p.y>head_pivot_y-.040)for(std::size_t side=0;side<2U;++side){const double sign=side==0U?1.0:-1.0;
            const double cx=sign*(eyes[side].x*sign+.006),cheek=.30*front_weight*bell((p.x-cx)/.016,(p.y-(face.eye_y_ratio-.016))/.012);
            const std::size_t cheek_index=side==0U?4U:7U,inner_index=side==0U?5U:8U,outer_index=side==0U?6U:9U;
            if(cheek>.0005){for(std::size_t i=2;i<10U;++i)value[i]*=1.0-cheek;value[cheek_index]=cheek;}
            const auto& brow=brows[side];const double t=std::clamp((std::abs(p.x)-std::min(std::abs(brow.inner.x),std::abs(brow.outer.x)))/
                std::max(.001,std::abs(brow.outer.x-brow.inner.x)),0.0,1.0),brow_y=mix(brow.inner.y,brow.outer.y,t),
                brow_x=sign*mix(std::abs(brow.inner.x),std::abs(brow.outer.x),t),amount=.28*front_weight*bell((p.x-brow_x)/.016,(p.y-brow_y)/.010);
            if(amount>.0005){for(std::size_t i=2;i<10U;++i)value[i]*=1.0-amount;value[inner_index]=amount*(1.0-t);value[outer_index]=amount*t;}}
        const double transition=smooth((p.y-(head_pivot_y-.038))/.032),remaining=1.0-transition,h=smooth((p.y-(head_pivot_y-.022))/.030),
            neck_chest=(1.0-smooth((p.y-(neck_joint_y-.010))/.030))*(1.0-h);
        value[0]=neck_chest*remaining;value[1]=(1.0-h-neck_chest)*remaining;value[2]=h*remaining+value[2]*transition;
        for(std::size_t i=3;i<10U;++i)value[i]*=transition;constexpr std::array<BoneId,10U> bones{BoneId::Chest,BoneId::Neck,BoneId::Head,
            BoneId::Jaw,BoneId::CheekL,BoneId::BrowInnerL,BoneId::BrowOuterL,BoneId::CheekR,BoneId::BrowInnerR,BoneId::BrowOuterR};
        std::vector<ReferenceInfluence> out;for(std::size_t i=0;i<10U;++i)if(value[i]>0.0)out.push_back({static_cast<std::uint16_t>(boneIndex(bones[i])),value[i]});return out;}
    const FacePhenotype& face;double height{},chin_y{},head_pivot_y{},top_y{},mouth_y{},mouth_half{},neck_joint_y{},nose_base_y{},hair_floor{};
    std::vector<Section> levels;std::array<Feature,2U> eyes{};std::array<Brow,2U> brows{};
    mutable std::deque<std::pair<double,std::size_t>> section_cache{};
    mutable std::array<std::array<double,5U>,256U> section_cache_values{};
    mutable std::size_t section_cache_next{},section_cursor{};bool cache_enabled{};
};

[[nodiscard]] Patch patchBox(const Layout& layout,Feature feature,double half_x,double half_y){Patch p{};p.feature=feature;p.y0=feature.y-half_y;p.y1=feature.y+half_y;p.half_x=half_x;p.half_y=half_y;
    std::array<double,6U> theta{};std::size_t at=0;for(const double y:{p.y0,feature.y,p.y1}){const double rx=layout.section(y).rx;
        theta[at++]=std::asin(std::clamp((feature.x-half_x)/rx,-.94,.94));theta[at++]=std::asin(std::clamp((feature.x+half_x)/rx,-.94,.94));}
    p.t0=*std::min_element(theta.begin(),theta.end());p.t1=*std::max_element(theta.begin(),theta.end());return p;}

} // namespace

ReferenceJacketBuild ReferenceFaceSurfaceGenerator::appendShell(ReferenceSurfaceBuilder& builder,
    const EquipmentFit& fit,foundation::Color skin_value,std::uint32_t detail_level) {
    const std::size_t first_vertex=builder.vertexCount();std::size_t triangles=0U;const Layout layout(fit,detail_level!=3U);const bool high=detail_level==3U,far=detail_level==1U;
    Feature mouth{};mouth.y=layout.mouth_y;mouth.w=layout.mouth_half;mouth.h=.00014;
    std::array<Patch,3U> patches{patchBox(layout,layout.eyes[0],layout.eyes[0].radius*1.30,layout.eyes[0].radius*1.23),
        patchBox(layout,layout.eyes[1],layout.eyes[1].radius*1.30,layout.eyes[1].radius*1.23),
        patchBox(layout,mouth,mouth.w*1.48+.001,.0065)};
    std::vector<double> ys;for(const auto& value:layout.levels)ys.push_back(value.y);const double step=detail_level==3U?.0035:detail_level==1U?.010:.0055;
    for(double y=.828;y<layout.top_y-.001;y+=step)ys.push_back(y);const std::size_t angle_count=detail_level==3U?56U:detail_level==1U?24U:40U;
    std::vector<double> angles;for(std::size_t j=0;j<angle_count;++j)angles.push_back(-3.14159265358979323846+6.283185307179586476925286766559*j/angle_count);
    for(auto& patch:patches){ys.push_back(patch.y0);ys.push_back(patch.y1);for(std::size_t i=0;i<=8U;++i)angles.push_back(mix(patch.t0,patch.t1,static_cast<double>(i)/8.0));
        for(std::size_t i=1;i<6U;++i)ys.push_back(mix(patch.y0,patch.y1,static_cast<double>(i)/6.0));}
    const auto unique=[](std::vector<double>& values){std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end(),[](double a,double b){return b-a<=1e-7;}),values.end());};
    unique(ys);unique(angles);ys.erase(std::remove_if(ys.begin(),ys.end(),[&](double y){return y>=layout.top_y-.0001;}),ys.end());
    const auto locate=[](const std::vector<double>& values,double target){return static_cast<std::size_t>(std::find_if(values.begin(),values.end(),[&](double value){return std::abs(value-target)<1e-7;})-values.begin());};
    for(auto& patch:patches){patch.r0=locate(ys,patch.y0);patch.r1=locate(ys,patch.y1);patch.c0=locate(angles,patch.t0);patch.c1=locate(angles,patch.t1);}
    const auto inside=[&](std::size_t r,std::size_t c,bool boundary){for(const auto& p:patches)if(boundary?(r>=p.r0&&r<p.r1&&c>=p.c0&&c<p.c1):(r>p.r0&&r<p.r1&&c>p.c0&&c<p.c1))return true;return false;};
    const ReferenceColor skin{skin_value.r,skin_value.g,skin_value.b};const std::array<ReferenceInfluence,1U> head{{{static_cast<std::uint16_t>(boneIndex(BoneId::Head)),1.0}}};
    std::vector<std::vector<std::int32_t>> grid;for(std::size_t r=0;r<ys.size();++r){std::vector<std::int32_t> row;const double y=ys[r];const auto row_section=layout.section(y);builder.setTag(y<layout.head_pivot_y-.010?"neck":"head");
        for(std::size_t c=0;c<angles.size();++c){if(inside(r,c,false)){row.push_back(-1);continue;}const double t=angles[c];
            const auto point=layout.pointFromSection(y,t,row_section);const double factor=.96+.04*std::cos(t);
            ReferenceVec3 hint{std::sin(t),y>.973?(y-.973)*45.0:0.0,std::cos(t)};const double hl=std::sqrt(hint.x*hint.x+hint.y*hint.y+hint.z*hint.z);hint={hint.x/hl,hint.y/hl,hint.z/hl};
            const auto weights=layout.weights(point);const auto id=builder.vertex(point,weights,{std::min(1.0,skin.r*factor),std::min(1.0,skin.g*factor),std::min(1.0,skin.b*factor)},hint,
                {static_cast<double>(c)/angles.size(),y*6.0},1U);row.push_back(static_cast<std::int32_t>(id));if(y>.833&&y<.888){const double gain=std::sin(3.14159265358979323846*
                    std::clamp((y-.833)/.055,0.0,1.0));builder.morph("neckFlex",id,{std::sin(t)*.00065*gain,0,std::cos(t)*.0011*gain});}}
        grid.push_back(std::move(row));}
    for(std::size_t r=0;r+1U<grid.size();++r)for(std::size_t c=0;c<angles.size();++c){if(inside(r,c,true))continue;const auto k=(c+1U)%angles.size();
        builder.triangle(grid[r][c],grid[r+1U][c],grid[r][k],1U);builder.triangle(grid[r][k],grid[r+1U][c],grid[r+1U][k],1U);triangles+=2U;}
    builder.setTag("neck");std::vector<std::uint32_t> bottom;for(const auto value:grid.front())bottom.push_back(static_cast<std::uint32_t>(value));
    const std::array<ReferenceInfluence,1U> chest{{{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),1.0}}};builder.cap(bottom,chest,skin,{0,-1,0},1U);triangles+=bottom.size();
    builder.setTag("head");const auto pole=builder.vertex(layout.point(layout.top_y,0),head,skin,{0,1,0});const auto& top=grid.back();
    for(std::size_t j=0;j<top.size();++j){builder.triangle(top[j],pole,top[(j+1U)%top.size()],1U);++triangles;}
    const auto loop_of=[&](const Patch& p){std::vector<std::uint32_t> out;for(std::size_t c=p.c0;c<=p.c1;++c)out.push_back(grid[p.r0][c]);
        for(std::size_t r=p.r0+1U;r<=p.r1;++r)out.push_back(grid[r][p.c1]);for(std::size_t c=p.c1;c-->p.c0;)out.push_back(grid[p.r1][c]);
        for(std::size_t r=p.r1;r-->p.r0+1U;)out.push_back(grid[r][p.c0]);return out;};
    const auto ellipsoid=[&](ReferenceVec3 center,ReferenceVec3 radii,std::span<const ReferenceInfluence> weights,
        ReferenceColor color,std::size_t segments,std::size_t rows,std::string_view tag,double rotation_y=0.0){builder.setTag(tag);std::vector<ReferenceSurfaceBuilder::Ring> rings;
        const double rc=std::cos(rotation_y),rs=std::sin(rotation_y);
        for(std::size_t row=0;row<=rows;++row){const double phi=-1.5707963267948966+3.14159265358979323846*row/rows,cp=std::cos(phi),sp=std::sin(phi);
            ReferenceSurfaceBuilder::Ring ring;for(std::size_t j=0;j<segments;++j){const double angle=6.283185307179586476925286766559*j/segments,ca=std::cos(angle),sa=std::sin(angle);
                const ReferenceVec3 source{cp*ca*radii.x,sp*radii.y,cp*sa*radii.z},source_normal{cp*ca/radii.x,sp/radii.y,cp*sa/radii.z},
                    local{rc*source.x+rs*source.z,source.y,-rs*source.x+rc*source.z},normal{rc*source_normal.x+rs*source_normal.z,source_normal.y,-rs*source_normal.x+rc*source_normal.z};
                ring.push_back(builder.vertex({center.x+local.x,center.y+local.y,center.z+local.z},weights,color,normal,{static_cast<double>(j)/segments,static_cast<double>(row)/rows},1U));}
            if(!rings.empty()){builder.bridge(rings.back(),ring,1U);triangles+=row==1U||row==rows?segments:segments*2U;}rings.push_back(std::move(ring));}};
    const auto append_eye=[&](std::size_t side,const Patch& patch){const auto& eye=layout.eyes[side];const bool left=side==0U;const BoneId eye_bone=left?BoneId::EyeL:BoneId::EyeR;
        const std::array<ReferenceInfluence,1U> eye_weight{{{static_cast<std::uint16_t>(boneIndex(eye_bone)),1.0}}};const std::string suffix=left?"L":"R";
        ellipsoid({eye.x,eye.y,eye.z},{eye.radius,eye.radius,eye.radius},eye_weight,rgb(0xdad8cfU),high?32U:far?12U:24U,high?20U:far?8U:14U,"eye."+suffix);
        const auto disc=[&](double radius,ReferenceColor color,double offset,std::string_view tag){builder.setTag(tag);ReferenceSurfaceBuilder::Ring previous;
            const std::vector<double> fractions=far?std::vector<double>{0.0,.65,1.0}:std::vector<double>{0.0,.4,.75,1.0};const std::size_t disc_segments=high?28U:far?12U:20U;
            for(std::size_t fraction_index=0;fraction_index<fractions.size();++fraction_index){const double fraction=fractions[fraction_index];ReferenceSurfaceBuilder::Ring ring;for(std::size_t j=0;j<disc_segments;++j){const double angle=6.283185307179586476925286766559*j/disc_segments;
                const double x=eye.x+radius*fraction*std::cos(angle),y=eye.y+radius*fraction*std::sin(angle),d=(x-eye.x)*(x-eye.x)+(y-eye.y)*(y-eye.y),
                    z=eye.z+std::sqrt(std::max(0.0,eye.radius*eye.radius-d))+offset;ReferenceVec3 normal{x-eye.x,y-eye.y,z-eye.z};
                const double nl=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);normal={normal.x/nl,normal.y/nl,normal.z/nl};
                ring.push_back(builder.vertex({x,y,z},eye_weight,color,normal));}if(!previous.empty()){builder.bridge(previous,ring,1U);triangles+=fraction_index==1U?disc_segments:disc_segments*2U;}previous=std::move(ring);}};
        disc(.00285*layout.face.eye_width_scale,rgb(layout.face.eye_color_hex),.00012,"iris."+suffix);
        disc(.00122*layout.face.eye_width_scale,rgb(0x14191bU),.00022,"pupil."+suffix);
        const auto outer_loop=loop_of(patch);auto last=outer_loop;const std::vector<double> lid_steps=far?std::vector<double>{.5,1.0}:std::vector<double>{.16,.32,.48,.64,.80,1.0};for(const double t:lid_steps){ReferenceSurfaceBuilder::Ring ring;
            for(const auto index:outer_loop){const auto edge=builder.point(index);const double angle=std::atan2((edge.y-eye.y)/patch.half_y,(edge.x-eye.x)/patch.half_x),
                    lx=eye.w*std::cos(angle),sn=std::sin(angle),ly=eye.h*sn*std::pow(std::abs(sn),mix(.12,.45,std::clamp(1.3-eye.roundness,0.0,1.0))),
                    c=std::cos(eye.tilt),ss=std::sin(eye.tilt);ReferenceVec3 target{eye.x+lx*c-ly*ss,eye.y+lx*ss+ly*c,0};
                const bool upper=sn>=(high?1e-7:0.0);const double sign=high?(upper?1.0:-1.0):(sn>0.0?1.0:sn<0.0?-1.0:0.0);
                const double seal=-sign*.00005*std::sqrt(std::abs(sn));const ReferenceVec3 closed_target{
                    eye.x+lx*c-seal*ss,eye.y+lx*ss+seal*c,0};ReferenceVec3 open{mix(edge.x,target.x,t),mix(edge.y,target.y,t),0},
                    closed{open.x+(closed_target.x-target.x)*t,open.y+(closed_target.y-target.y)*t,0};const auto surface_z=[&](ReferenceVec3 p){const double d=(p.x-eye.x)*(p.x-eye.x)+(p.y-eye.y)*(p.y-eye.y),skin_radius=eye.radius+.00135,
                    globe=d<skin_radius*skin_radius?eye.z+std::sqrt(skin_radius*skin_radius-d):-1e9;return std::max(layout.front(p.x,p.y),globe)+.00008;};
                open.z=surface_z(open);closed.z=surface_z(closed)+((high?upper:sn>0.0)?.00010*t:0.0);ReferenceVec3 midpoint{mix(open.x,closed.x,.5),mix(open.y,closed.y,.5),0};midpoint.z=surface_z(midpoint);
                auto weights=layout.weights(edge);const double share=(upper?.32:.24)*smooth((t-.20)/.80),blend=smooth(t);
                for(auto& weight:weights)weight.weight*=1.0-blend;const auto add_weight=[&](BoneId bone,double value){const auto index=static_cast<std::uint16_t>(boneIndex(bone));
                    const auto found=std::find_if(weights.begin(),weights.end(),[&](const ReferenceInfluence& weight){return weight.bone_index==index;});
                    if(found==weights.end())weights.push_back({index,value});else found->weight+=value;};
                add_weight(BoneId::Head,blend*(1.0-share));add_weight(upper?(left?BoneId::LidUpperL:BoneId::LidUpperR):
                    (left?BoneId::LidLowerL:BoneId::LidLowerR),blend*share);
                builder.setTag((upper?"lidUpper.":"lidLower.")+suffix);const double factor=t>.99?.86:.98;
                (void)layout.section(open.y);(void)layout.section(open.y+.0001);(void)layout.section(open.y-.0001);
                const auto id=builder.vertex(open,weights,{skin.r*factor,skin.g*factor,skin.b*factor},{0,0,1});ring.push_back(id);
                builder.morph("eyelidsClose",id,{closed.x-open.x,closed.y-open.y,closed.z-open.z});const ReferenceVec3 linear{
                    mix(open.x,closed.x,.5),mix(open.y,closed.y,.5),mix(open.z,closed.z,.5)};builder.morph("eyelidsArc",id,
                    {midpoint.x-linear.x,midpoint.y-linear.y,midpoint.z-linear.z});}
            builder.bridge(last,ring,1U);triangles+=last.size()*2U;last=std::move(ring);}};
    append_eye(0U,patches[0]);append_eye(1U,patches[1]);
    const auto append_mouth=[&](const Patch& patch){const auto outer=loop_of(patch);auto last=outer;
        const ReferenceColor lip{std::min(1.0,skin.r*1.05),std::min(1.0,skin.g*.91),std::min(1.0,skin.b*.89)},inside=rgb(0x40282aU);
        std::vector<std::pair<ReferenceVec3,std::vector<ReferenceInfluence>>> mouth_points;
        const auto blend_weights=[](std::vector<ReferenceInfluence> a,const std::vector<ReferenceInfluence>& b,double t){for(auto& value:a)value.weight*=1.0-t;
            for(const auto& value:b){const auto found=std::find_if(a.begin(),a.end(),[&](const ReferenceInfluence& current){return current.bone_index==value.bone_index;});
                if(found==a.end())a.push_back({value.bone_index,value.weight*t});else found->weight+=value.weight*t;}return a;};
        const std::vector<double> mouth_steps=far?std::vector<double>{.55,1.0}:std::vector<double>{.28,.55,.78,1.0};for(const double t:mouth_steps){ReferenceSurfaceBuilder::Ring ring;for(const auto index:outer){const auto edge=builder.point(index);
                const double angle=std::atan2((edge.y-layout.mouth_y)/patch.half_y,(edge.x)/patch.half_x),cs=std::cos(angle),sn=std::sin(angle);
                const ReferenceVec3 target{layout.mouth_half*cs,layout.mouth_y+patch.feature.h*sn+layout.face.neutral_mouth*.0010*std::abs(cs)+layout.face.mouth_asymmetry*.35*cs,0};
                ReferenceVec3 point{mix(edge.x,target.x,t),mix(edge.y,target.y,t),0};const double flesh=sn>=0.0?layout.face.upper_lip:layout.face.lower_lip;
                point.z=layout.front(point.x,point.y)+.0003+std::sin(t*3.14159265358979323846)*flesh*.34;
                const bool left=cs>=0.0;const double corner=std::pow(std::abs(cs),6.0)*.75;std::vector<ReferenceInfluence> control;
                control.push_back({static_cast<std::uint16_t>(boneIndex(sn>=0.0?BoneId::MouthUpper:BoneId::MouthLower)),1.0-corner});
                control.push_back({static_cast<std::uint16_t>(boneIndex(left?BoneId::MouthCornerL:BoneId::MouthCornerR)),corner*.70});
                if(sn<0.0)control.push_back({static_cast<std::uint16_t>(boneIndex(BoneId::Jaw)),corner*.30});
                else control.push_back({static_cast<std::uint16_t>(boneIndex(BoneId::Head)),corner*.30});
                auto weights=blend_weights(layout.weights(edge),control,smooth(t));const bool is_lip=t>.55;
                builder.setTag(is_lip?(sn>=0.0?"upperLip":"lowerLip"):"head");const auto color=is_lip?
                    ReferenceColor{lip.r*(sn>=0.0?.94:1.05),lip.g*(sn>=0.0?.94:1.05),lip.b*(sn>=0.0?.94:1.05)}:skin;
                ring.push_back(builder.vertex(point,weights,color,{0,0,1}));if(t==1.0)mouth_points.push_back({point,weights});}
            builder.bridge(last,ring,1U);triangles+=last.size()*2U;last=std::move(ring);}
        builder.setTag("mouthInner");const std::vector<double> depths=far?std::vector<double>{.011}:std::vector<double>{.003,.011};for(const double depth:depths){ReferenceSurfaceBuilder::Ring ring;for(const auto& [point,weights]:mouth_points)
                ring.push_back(builder.vertex({point.x*.90,layout.mouth_y+(point.y-layout.mouth_y)*1.4,point.z-depth},weights,inside,{0,0,1}));
            builder.bridge(last,ring,1U);triangles+=last.size()*2U;last=std::move(ring);}
        const std::array<ReferenceInfluence,2U> cap_weights{{{static_cast<std::uint16_t>(boneIndex(BoneId::Head)),.5},
            {static_cast<std::uint16_t>(boneIndex(BoneId::Jaw)),.5}}};builder.cap(last,cap_weights,inside,{0,0,1},1U);triangles+=last.size();};
    append_mouth(patches[2]);
    builder.setTag("nose");const double nose_y0=layout.face.eye_y_ratio+.005,nose_yb=layout.nose_base_y;
    const std::vector<std::array<double,4U>> nose_profiles=far?std::vector<std::array<double,4U>>{
        {nose_y0,.0028*layout.face.nose_bridge_scale,.0013,.0005},
        {mix(nose_y0,nose_yb,.5),.0046*layout.face.nose_bridge_scale,.004,.003},
        {nose_yb,.0068*layout.face.nose_width_scale*layout.face.nose_tip_width_scale,.006,.0075*layout.face.nose_projection_scale}}:
        std::vector<std::array<double,4U>>{{nose_y0,.0028*layout.face.nose_bridge_scale,.0013,.0005},
        {mix(nose_y0,nose_yb,.26),.0036*layout.face.nose_bridge_scale,.0035,.002},
        {mix(nose_y0,nose_yb,.70),.0053*layout.face.nose_width_scale,.0055,.004},
        {nose_yb,.0068*layout.face.nose_width_scale*layout.face.nose_tip_width_scale,.006,.0075*layout.face.nose_projection_scale},
        {nose_yb-.003+layout.face.nose_tip_rotation*.004,.0063*layout.face.nose_width_scale,.0035,.007*layout.face.nose_projection_scale}};
    const std::size_t nose_segments=high?20U:14U;
    ReferenceSurfaceBuilder::Ring previous_nose;for(const auto& row:nose_profiles){ReferenceSurfaceBuilder::Ring ring;const double z=layout.front(0,row[0])+row[3];
        for(std::size_t index=0;index<nose_segments;++index){const double angle=6.283185307179586476925286766559*index/nose_segments,c=std::cos(angle),s=std::sin(angle);
            ring.push_back(builder.vertex({row[1]*c,row[0],z+row[2]*s},head,
                {std::min(1.0,skin.r*1.01),std::min(1.0,skin.g*1.01),std::min(1.0,skin.b*1.01)},
                {c,0,s},{static_cast<double>(index)/nose_segments*3.0,0.0},1U));}
        if(previous_nose.empty()){builder.cap(ring,head,skin,{0,1,0},1U);triangles+=nose_segments;}else{builder.bridge(previous_nose,ring,1U);triangles+=nose_segments*2U;}previous_nose=std::move(ring);}
    builder.cap(previous_nose,head,{skin.r*.84,skin.g*.84,skin.b*.84},{0,-1,0},1U);triangles+=nose_segments;
    for(const double sign:{-1.0,1.0}){const double x=sign*.0065*layout.face.nose_width_scale*layout.face.nostril_width_scale,y=nose_yb-.001,
            z=layout.front(x,y)+.0058*layout.face.nose_projection_scale;
        ellipsoid({x,y,z},{.0035*layout.face.nostril_width_scale,.0026,.0035},head,{skin.r*.95,skin.g*.95,skin.b*.95},high?14U:far?6U:10U,far?4U:8U,"nose");
        if(high)ellipsoid({x,y-.0018,z+.0016},{.0016,.00065,.0011},head,{skin.r*.42,skin.g*.42,skin.b*.42},10U,6U,"nose");}
    builder.setTag("ears");for(std::size_t side=0;side<2U;++side){const double sign=side==0U?1.0:-1.0,y=.927+sign*layout.face.ear_asymmetry*.5;
        const auto side_point=layout.point(y,sign*1.5707963267948966);const ReferenceVec3 center{side_point.x+sign*.003,y,side_point.z-.004};
        ellipsoid(center,{.0065*layout.face.ear_scale,.0135*layout.face.ear_scale,.0075*layout.face.ear_scale},head,
            {skin.r*.94,skin.g*.94,skin.b*.94},high?20U:far?8U:14U,high?12U:far?5U:9U,"ears",sign*layout.face.ear_angle);
        if(high)ellipsoid({center.x+sign*.003,center.y,center.z+.004},{.0025*layout.face.ear_scale,.0073*layout.face.ear_scale,.0025},head,
            {skin.r*.68,skin.g*.68,skin.b*.68},12U,8U,"ears",sign*layout.face.ear_angle);}
    const ReferenceColor brow_color=rgb(layout.face.hair_color_hex);for(std::size_t side=0;side<2U;++side){const bool left=side==0U;const auto& brow=layout.brows[side];
        std::vector<ReferenceVec3> points;for(std::size_t index=0;index<=10U;++index){const double t=static_cast<double>(index)/10.0,x=mix(brow.inner.x,brow.outer.x,t),
            y=mix(brow.inner.y,brow.outer.y,t)+std::sin(t*3.14159265358979323846)*.0015;points.push_back({x,y,layout.front(x,y)+.0013});}
        builder.setTag(left?"browInner.L":"browInner.R");ReferenceSurfaceBuilder::Ring previous;for(std::size_t at=0;at<points.size();++at){const auto a=at+1U<points.size()?points[at]:points[at-1U],
                b=at+1U<points.size()?points[at+1U]:points[at];ReferenceVec3 direction{b.x-a.x,b.y-a.y,b.z-a.z};const double dl=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
            direction={direction.x/dl,direction.y/dl,direction.z/dl};const ReferenceVec3 guide=std::abs(direction.z)<.9?ReferenceVec3{0,0,1}:ReferenceVec3{0,1,0};
            ReferenceVec3 u{direction.y*guide.z-direction.z*guide.y,direction.z*guide.x-direction.x*guide.z,direction.x*guide.y-direction.y*guide.x};const double ul=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
            u={u.x/ul,u.y/ul,u.z/ul};const ReferenceVec3 v{u.y*direction.z-u.z*direction.y,u.z*direction.x-u.x*direction.z,u.x*direction.y-u.y*direction.x};
            const double weight_t=std::clamp((points[at].x-brow.inner.x)/(brow.outer.x-brow.inner.x),0.0,1.0);const std::array<ReferenceInfluence,2U> weights{{
                {static_cast<std::uint16_t>(boneIndex(left?BoneId::BrowInnerL:BoneId::BrowInnerR)),1.0-weight_t},
                {static_cast<std::uint16_t>(boneIndex(left?BoneId::BrowOuterL:BoneId::BrowOuterR)),weight_t}}};ReferenceSurfaceBuilder::Ring ring;
            const std::size_t brow_segments=high?8U:far?4U:6U;for(std::size_t index=0;index<brow_segments;++index){const double angle=6.283185307179586476925286766559*index/brow_segments,c=std::cos(angle),s=std::sin(angle),radius=layout.face.brow_thickness;
                const ReferenceVec3 offset{u.x*radius*c+v.x*radius*s,u.y*radius*c+v.y*radius*s,u.z*radius*c+v.z*radius*s};ring.push_back(builder.vertex(
                    {points[at].x+offset.x,points[at].y+offset.y,points[at].z+offset.z},weights,brow_color,offset,{static_cast<double>(index)/brow_segments*3.0,static_cast<double>(at)/points.size()},1U));}
            if(!previous.empty()){builder.bridge(previous,ring,1U);triangles+=brow_segments*2U;}previous=std::move(ring);}}
    struct HairStyle {double side{},top{},front{},part{};};constexpr std::array<HairStyle,7U> styles{{
        {0,0,0,0},{.12,.10,.08,0},{.36,.72,.40,0},{.30,.82,.65,0},{.32,.93,.50,.35},{.12,.94,.52,0},{.42,1.05,.67,0}}};
    const std::size_t style_index=std::min<std::size_t>(layout.face.hair_style,styles.size()-1U);const auto style=styles[style_index];
    if(style_index!=0U){builder.setTag("hair");const ReferenceColor hair=rgb(layout.face.hair_color_hex);const std::size_t rows=high?17U:far?6U:12U,
            segments=high?56U:far?24U:40U;
        const auto visible=[&](ReferenceVec3 point){return fit.headgear_kind.has_value()?point.y<fit.headBottom(static_cast<float>(std::atan2(point.x,point.z)))-.001:true;};
        const auto bridge_hair=[&](const std::vector<std::int32_t>& a,const std::vector<std::int32_t>& b){for(std::size_t j=0;j<a.size();++j){const auto k=(j+1U)%a.size();
            if(a[j]>=0&&b[j]>=0&&a[k]>=0){builder.triangle(a[j],b[j],a[k],2U);++triangles;}if(a[k]>=0&&b[j]>=0&&b[k]>=0){builder.triangle(a[k],b[j],b[k],2U);++triangles;}}};
        std::vector<std::int32_t> last,first;for(std::size_t row=0;row<rows;++row){const double t=static_cast<double>(row)/rows;std::vector<std::int32_t> loop;
            for(std::size_t j=0;j<segments;++j){const double angle=-3.14159265358979323846+6.283185307179586476925286766559*j/segments,
                    front_weight=std::max(0.0,std::cos(angle)),side=std::abs(std::sin(angle));double bottom=layout.hairBottom(angle);if(style_index==5U)bottom+=.0025*side;const double y=mix(bottom,layout.top_y-.0003,t);
                auto point=layout.point(y,angle);const double wave=std::sin(angle*15.0+t*13.0)*.08+std::sin(angle*9.0-t*7.0)*.04;
                double radial=(.0008+layout.face.hair_thickness*.22+layout.face.hair_volume*(style.side*side+style.front*front_weight)*.30)*std::sin((1.0-t)*1.5707963267948966);
                if(fit.headgear_kind.has_value())radial=std::min(radial,.0018);point.x+=std::sin(angle)*radial*(1.0+wave);point.z+=std::cos(angle)*radial*(1.0+wave);
                const double top_offset=layout.face.hair_volume*style.top*std::pow(t,1.4)*.58;point.y+=top_offset*(1.0+wave*(style_index==6U?1.0:.3));if(style.part!=0.0)point.x+=style.part*top_offset*std::sin(t*3.14159265358979323846);
                if(!visible(point)){loop.push_back(-1);continue;}ReferenceVec3 normal{std::sin(angle)*(1.0-t*.75),t,std::cos(angle)*(1.0-t*.75)};
                const double nl=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z),factor=.92+wave*.32;normal={normal.x/nl,normal.y/nl,normal.z/nl};
                loop.push_back(static_cast<std::int32_t>(builder.vertex(point,head,{std::clamp(hair.r*factor,0.0,1.0),std::clamp(hair.g*factor,0.0,1.0),std::clamp(hair.b*factor,0.0,1.0)},normal,
                    {static_cast<double>(j)/segments,t},1U)));}if(!last.empty())bridge_hair(last,loop);else first=loop;last=std::move(loop);}
        auto tip_point=layout.point(layout.top_y,0);tip_point.y+=layout.face.hair_volume*style.top*.58;const std::int32_t tip=visible(tip_point)?
            static_cast<std::int32_t>(builder.vertex(tip_point,head,hair,{0,1,0}, {},1U)):-1;if(tip>=0)for(std::size_t j=0;j<segments;++j)if(last[j]>=0&&last[(j+1U)%segments]>=0){builder.triangle(last[j],tip,last[(j+1U)%segments],2U);++triangles;}
        std::vector<std::int32_t> inset;for(std::size_t j=0;j<segments;++j){const double angle=-3.14159265358979323846+6.283185307179586476925286766559*j/segments,
                y=layout.hairBottom(angle)+(style_index==5U?.0025*std::abs(std::sin(angle)):0.0);
            auto point=layout.point(y,angle);point.y+=.0002;if(!visible(point)){inset.push_back(-1);continue;}inset.push_back(static_cast<std::int32_t>(builder.vertex(point,head,{hair.r*.87,hair.g*.87,hair.b*.87},
                {std::sin(angle),0,std::cos(angle)}, {},1U)));}bridge_hair(inset,first);}
    return {builder.vertexCount()-first_vertex,triangles};
}

} // namespace genomes::infantry
