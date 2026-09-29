#include <genomes/weapons/WeaponCatalog.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <unordered_map>
#include <vector>

namespace genomes::weapons {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 a,foundation::Vec3 b) noexcept{return {a.x+b.x,a.y+b.y,a.z+b.z};}
[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 a,foundation::Vec3 b) noexcept{return {a.x-b.x,a.y-b.y,a.z-b.z};}
[[nodiscard]] float dot(foundation::Vec3 a,foundation::Vec3 b) noexcept{return a.x*b.x+a.y*b.y+a.z*b.z;}
[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 a,foundation::Vec3 b) noexcept{return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 v) noexcept{const float l=std::sqrt(dot(v,v));return l>0?multiply(v,1/l):foundation::Vec3{};}
[[nodiscard]] foundation::Color linearColor(std::uint32_t hex) noexcept{const auto channel=[](std::uint32_t value){const float c=static_cast<float>(value)/255.0F;return c<.04045F?c*.0773993808F:std::pow(c*.9478672986F+.0521327014F,2.4F);};return {channel((hex>>16U)&255U),channel((hex>>8U)&255U),channel(hex&255U),1};}

struct DoubleVec3 final { double x; double y; double z; };

class ReferenceWeaponMeshBuilder final {
public:
    using Ring=std::vector<std::uint32_t>;
    std::uint32_t vertex(foundation::Vec3 p,foundation::Color color,foundation::Vec3 normal,
                         foundation::Vec2 uv={},std::uint32_t material=0){
        const auto index=static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({p,normal,uv,color,material});return index;}
    void triangle(std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t material=0){
        const auto pa=mesh.vertices[a].position,pb=mesh.vertices[b].position,pc=mesh.vertices[c].position;
        const auto n=cross(subtract(pb,pa),subtract(pc,pa));if(dot(n,n)<1e-18F)return;
        const auto hint=add(add(mesh.vertices[a].normal,mesh.vertices[b].normal),mesh.vertices[c].normal);
        if(dot(n,hint)<0)std::swap(b,c);triangles[material].insert(triangles[material].end(),{a,b,c});}
    Ring ring(foundation::Vec3 center,foundation::Vec3 u,foundation::Vec3 v,float rx,float rz,
              std::size_t count,foundation::Color color,std::uint32_t material=0,float uv_y=0){Ring result;result.reserve(count);
        for(std::size_t j=0;j<count;++j){const float angle=2*3.14159265358979323846F*static_cast<float>(j)/static_cast<float>(count);
            const float x=std::cos(angle),z=std::sin(angle);const auto p=add(center,add(multiply(u,rx*x),multiply(v,rz*z)));
            const auto n=normalized(add(multiply(u,x/std::max(rx,1e-5F)),multiply(v,z/std::max(rz,1e-5F))));
            result.push_back(vertex(p,color,n,{static_cast<float>(j)/static_cast<float>(count)*3,uv_y},material));}return result;}
    void bridge(const Ring& a,const Ring& b,std::uint32_t material=0){for(std::size_t j=0;j<a.size();++j){const auto k=(j+1)%a.size();triangle(a[j],b[j],a[k],material);triangle(a[k],b[j],b[k],material);}}
    void cap(const Ring& loop,foundation::Color color,foundation::Vec3 normal,std::uint32_t material=0){foundation::Vec3 center{};for(const auto index:loop)center=add(center,mesh.vertices[index].position);center=multiply(center,1.0F/static_cast<float>(loop.size()));const auto middle=vertex(center,color,normal,{},material);for(std::size_t i=0;i<loop.size();++i)triangle(middle,loop[i],loop[(i+1)%loop.size()],material);}
    void cylinder(foundation::Vec3 a,foundation::Vec3 b,float radius,foundation::Color color,std::uint32_t material=1,std::size_t segments=10){const auto direction=normalized(subtract(b,a));const foundation::Vec3 guide=std::abs(direction.z)<.9F?foundation::Vec3{0,0,1}:foundation::Vec3{0,1,0};const auto u=normalized(cross(direction,guide)),v=normalized(cross(u,direction));const auto one=ring(a,u,v,radius,radius,segments,color,material),two=ring(b,u,v,radius,radius,segments,color,material);bridge(one,two,material);cap(one,color,multiply(direction,-1),material);cap(two,color,direction,material);}
    void tubePath(const std::vector<foundation::Vec3>& points,float radius,foundation::Color color,std::uint32_t material=0,std::size_t segments=6){Ring previous;for(std::size_t i=0;i<points.size();++i){const auto direction=normalized(i+1<points.size()?subtract(points[i+1],points[i]):subtract(points[i],points[i-1]));const foundation::Vec3 guide=std::abs(direction.z)<.9F?foundation::Vec3{0,0,1}:foundation::Vec3{0,1,0};const auto u=normalized(cross(direction,guide)),v=normalized(cross(u,direction));auto current=ring(points[i],u,v,radius,radius,segments,color,material,static_cast<float>(i)/points.size());if(!previous.empty())bridge(previous,current,material);previous=std::move(current);}}
    void profile(const std::vector<foundation::Vec2>& points,float width,foundation::Color color,std::uint32_t material=1){for(const float sign:{-1.0F,1.0F}){Ring ids;for(const auto p:points)ids.push_back(vertex({sign*width/2,p.x,p.y},color,{sign,0,0},{},material));for(std::size_t i=1;i+1<ids.size();++i)triangle(ids[0],ids[i],ids[i+1],material);}for(std::size_t i=0;i<points.size();++i){const auto a=points[i],b=points[(i+1)%points.size()];const auto normal=normalized({0,b.y-a.y,a.x-b.x});const std::array<std::uint32_t,4> ids{vertex({-width/2,a.x,a.y},color,normal,{},material),vertex({width/2,a.x,a.y},color,normal,{},material),vertex({width/2,b.x,b.y},color,normal,{},material),vertex({-width/2,b.x,b.y},color,normal,{},material)};triangle(ids[0],ids[1],ids[2],material);triangle(ids[0],ids[2],ids[3],material);}}
    void box(DoubleVec3 center,DoubleVec3 size,foundation::Color color,std::uint32_t material=0,double round=.13){
        const std::array<double,3> half{size.x*.5,size.y*.5,size.z*.5};const double radius=std::min({half[0],half[1],half[2]})*round*2;
        const std::array<double,3> core{half[0]-radius,half[1]-radius,half[2]-radius};constexpr std::size_t segments=2;
        constexpr std::array<std::array<int,2>,6> faces{{{{0,1}},{{0,-1}},{{1,1}},{{1,-1}},{{2,1}},{{2,-1}}}};
        for(const auto face:faces){const int axis=face[0],sign=face[1],u=(axis+1)%3,v=(axis+2)%3;std::array<std::array<std::uint32_t,3>,3> loops{};
            for(std::size_t j=0;j<=segments;++j)for(std::size_t i=0;i<=segments;++i){std::array<double,3> raw{};raw[axis]=half[axis]*sign;
                raw[u]=(static_cast<double>(i)/segments*2-1)*half[u];raw[v]=(static_cast<double>(j)/segments*2-1)*half[v];
                std::array<double,3> p{std::clamp(raw[0],-core[0],core[0]),std::clamp(raw[1],-core[1],core[1]),std::clamp(raw[2],-core[2],core[2])};
                const double nx=raw[0]-p[0],ny=raw[1]-p[1],nz=raw[2]-p[2],length=std::sqrt(nx*nx+ny*ny+nz*nz);
                const foundation::Vec3 normal{static_cast<float>(nx/length),static_cast<float>(ny/length),static_cast<float>(nz/length)};
                const foundation::Vec3 point{static_cast<float>(p[0]+nx/length*radius+center.x),static_cast<float>(p[1]+ny/length*radius+center.y),static_cast<float>(p[2]+nz/length*radius+center.z)};
                loops[j][i]=vertex(point,color,normal,{static_cast<float>(i)/segments,static_cast<float>(j)/segments},material);}
            for(std::size_t j=0;j<segments;++j)for(std::size_t i=0;i<segments;++i){triangle(loops[j][i],loops[j+1][i],loops[j][i+1],material);triangle(loops[j][i+1],loops[j+1][i],loops[j+1][i+1],material);}}
    }
    void ellipsoid(foundation::Vec3 center,foundation::Vec3 radii,foundation::Color color,std::uint32_t material,std::size_t segments,std::size_t rows){
        std::vector<Ring> rings;for(std::size_t row=0;row<=rows;++row){const float phi=-3.14159265358979323846F/2+3.14159265358979323846F*static_cast<float>(row)/rows;Ring ring_values;
            for(std::size_t j=0;j<segments;++j){const float a=2*3.14159265358979323846F*static_cast<float>(j)/segments;
                const foundation::Vec3 local{std::cos(phi)*std::cos(a)*radii.x,std::sin(phi)*radii.y,std::cos(phi)*std::sin(a)*radii.z};
                const auto normal=normalized({std::cos(phi)*std::cos(a)/radii.x,std::sin(phi)/radii.y,std::cos(phi)*std::sin(a)/radii.z});
                ring_values.push_back(vertex(add(center,local),color,normal,{static_cast<float>(j)/segments,static_cast<float>(row)/rows},material));}
            if(!rings.empty())bridge(rings.back(),ring_values,material);rings.push_back(std::move(ring_values));}}
    WeaponMesh finish(bool orient=false){mesh.indices.clear();for(auto& stream:triangles)mesh.indices.insert(mesh.indices.end(),stream.begin(),stream.end());
        if(orient){const std::size_t count=mesh.indices.size()/3,vertex_count=mesh.vertices.size();struct Use{std::uint32_t triangle;bool direction;};std::unordered_map<std::uint64_t,Use> edges;std::vector<std::vector<std::pair<std::uint32_t,std::uint8_t>>> neighbours(count);
            for(std::uint32_t t=0;t<count;++t)for(std::size_t j=0;j<3;++j){const auto a=mesh.indices[t*3+j],b=mesh.indices[t*3+(j+1)%3],lo=std::min(a,b),hi=std::max(a,b);const std::uint64_t key=static_cast<std::uint64_t>(lo)*vertex_count+hi;const Use entry{t,a<b};const auto found=edges.find(key);if(found!=edges.end()){const std::uint8_t different=found->second.direction==entry.direction?1:0;neighbours[t].push_back({found->second.triangle,different});neighbours[found->second.triangle].push_back({t,different});}else edges.emplace(key,entry);}
            std::vector<std::int8_t> flips(count,-1);for(std::uint32_t start=0;start<count;++start){if(flips[start]!=-1)continue;flips[start]=0;std::vector<std::uint32_t> queue{start};double score=0;for(std::size_t at=0;at<queue.size();++at){const auto t=queue[at];for(const auto [next,delta]:neighbours[t])if(flips[next]==-1){flips[next]=static_cast<std::int8_t>(flips[t]^delta);queue.push_back(next);}const auto a=mesh.indices[t*3],b=mesh.indices[t*3+1],c=mesh.indices[t*3+2];const auto pa=mesh.vertices[a].position,pb=subtract(mesh.vertices[b].position,pa),pc=subtract(mesh.vertices[c].position,pa),normal=cross(pb,pc);const auto hint=add(add(mesh.vertices[a].normal,mesh.vertices[b].normal),mesh.vertices[c].normal);score+=dot(normal,hint)*(flips[t]?-1:1);}const std::int8_t invert=score<0?1:0;for(const auto t:queue)if((flips[t]^invert)!=0)std::swap(mesh.indices[t*3+1],mesh.indices[t*3+2]);}}
        std::vector<foundation::Vec3> normals(mesh.vertices.size());
        for(std::size_t at=0;at<mesh.indices.size();at+=3){const auto a=mesh.indices[at],b=mesh.indices[at+1],c=mesh.indices[at+2];
            const auto pa=mesh.vertices[a].position,pb=mesh.vertices[b].position,pc=mesh.vertices[c].position;
            const double cbx=static_cast<double>(pc.x)-pb.x,cby=static_cast<double>(pc.y)-pb.y,cbz=static_cast<double>(pc.z)-pb.z;
            const double abx=static_cast<double>(pa.x)-pb.x,aby=static_cast<double>(pa.y)-pb.y,abz=static_cast<double>(pa.z)-pb.z;
            const double nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
            for(const auto index:{a,b,c}){normals[index].x=static_cast<float>(normals[index].x+nx);normals[index].y=static_cast<float>(normals[index].y+ny);normals[index].z=static_cast<float>(normals[index].z+nz);}}
        for(std::size_t index=0;index<mesh.vertices.size();++index){const auto n=normals[index];const double length=std::sqrt(static_cast<double>(n.x)*n.x+static_cast<double>(n.y)*n.y+static_cast<double>(n.z)*n.z);
            mesh.vertices[index].normal=length>0?foundation::Vec3{static_cast<float>(n.x/length),static_cast<float>(n.y/length),static_cast<float>(n.z/length)}:foundation::Vec3{};}
        if(!mesh.vertices.empty()){mesh.minimum=mesh.maximum=mesh.vertices[0].position;for(const auto& v:mesh.vertices){mesh.minimum.x=std::min(mesh.minimum.x,v.position.x);mesh.minimum.y=std::min(mesh.minimum.y,v.position.y);mesh.minimum.z=std::min(mesh.minimum.z,v.position.z);mesh.maximum.x=std::max(mesh.maximum.x,v.position.x);mesh.maximum.y=std::max(mesh.maximum.y,v.position.y);mesh.maximum.z=std::max(mesh.maximum.z,v.position.z);}}return std::move(mesh);}
private: WeaponMesh mesh;std::array<std::vector<std::uint32_t>,3> triangles;
};

} // namespace

foundation::StableId WeaponGeometryGenerator::cacheKey(const WeaponDefinition& definition,
                                                        const WeaponVariant& variant) noexcept {
    foundation::StableId key = foundation::stableHashCombine(
        foundation::stable_id("weapon.artifact.v1"), definition.id);
    key = foundation::stableHashCombine(key, foundation::stableHashU64(variant.seed));
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(variant.scale));
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(variant.wear));
    key = foundation::stableHashCombine(key, variant.material_variant);
    return key;
}

foundation::Result<WeaponArtifact, foundation::Error> WeaponGeometryGenerator::build(
    const WeaponDefinition& definition, const WeaponVariant& variant) {
    if (!definition.valid() || !variant.valid()) {
        return foundation::Result<WeaponArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon geometry input"});
    }
    const foundation::Vec3 half = multiply(definition.dimensions, 0.5F * variant.scale);
    const foundation::Vec3 minimum{-half.x, -half.y, -half.z};
    const foundation::Vec3 maximum{half.x, half.y, half.z};
    constexpr std::array<foundation::Vec3, 8> corners{{
        {-1.0F, -1.0F, -1.0F}, {1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, -1.0F}, {-1.0F, 1.0F, -1.0F},
        {-1.0F, -1.0F, 1.0F},  {1.0F, -1.0F, 1.0F},  {1.0F, 1.0F, 1.0F},  {-1.0F, 1.0F, 1.0F},
    }};
    WeaponArtifact artifact{};
    artifact.weapon_id = definition.id;
    artifact.cache_key = cacheKey(definition, variant);
    if(definition.visual_kind==WeaponVisualKind::Long||definition.visual_kind==WeaponVisualKind::Pistol||definition.visual_kind==WeaponVisualKind::Knife||definition.visual_kind==WeaponVisualKind::Grenade){
        ReferenceWeaponMeshBuilder builder;const foundation::Color metal=linearColor(0x586164),dark=linearColor(0x252c30),cloth=linearColor(0x525b43),rubber=linearColor(0x161b1b);
        if(definition.visual_kind==WeaponVisualKind::Long){const float length=definition.visual_length,stock=length*.27F,front=length-stock;const bool support=definition.identifier.find("support")!=std::string_view::npos,heavy=definition.identifier=="heavy_support_gun",marksman=definition.identifier=="marksman_rifle";const float hand_end=front*(support?.68F:.65F),hand_start=.155F;
            const auto box=[&](DoubleVec3 p,DoubleVec3 s,foundation::Color c=foundation::Color{-1,-1,-1,-1},std::uint32_t group=0){if(c.a<0)c=dark;builder.box(p,s,c,group,.18);};
            const auto tube=[&](foundation::Vec3 a,foundation::Vec3 b,float r,foundation::Color c=foundation::Color{-1,-1,-1,-1},std::uint32_t group=0){if(c.a<0)c=metal;builder.cylinder(a,b,r,c,group,12);};
            box({0,.069F,.070F},{heavy?.058F:.046F,.057F,.235F});box({0,.032F,.071F},{.041F,.033F,.172F},metal);tube({0,.080F,-stock+.04F},{0,.080F,-.044F},.017F,dark);
            builder.profile({{.105F,-stock+.014F},{.105F,-.078F},{.063F,-.060F},{.003F,-stock+.04F},{-.011F,-stock+.014F}},.045F,cloth,1);
            box({0,.043F,-stock+.007F},{.053F,.120F,.016F},rubber,2);box({0,.107F,-stock*.62F},{.046F,.015F,stock*.43F},cloth,1);
            builder.profile({{.033F,-.029F},{.028F,.021F},{-.070F,-.006F},{-.068F,-.046F}},.031F,dark,1);
            for(double y=-.054;y<.020;y+=.014)box({0,static_cast<float>(y),static_cast<float>(-.015+(y+.020)*.24)},{.033F,.004F,.026F},rubber,2);
            builder.tubePath({{0,.024F,.023F},{0,-.006F,.034F},{0,-.012F,.071F},{0,.026F,.088F}},.003F,metal);tube({0,.026F,.058F},{0,.002F,.049F},.003F,dark);
            if(!support){builder.profile({{.040F,.088F},{.040F,.148F},{-.058F,.168F},{-.128F,.158F},{-.128F,.103F},{-.047F,.096F}},.033F,dark,0);for(const float x:{-.018F,.018F})for(const float z:{.109F,.130F,.150F})box({x,-.036F,z},{.003F,.093F,.005F},metal);}
            box({0,.076F,(hand_start+hand_end)/2},{heavy?.064F:.054F,heavy?.063F:.055F,hand_end-hand_start},cloth,1);
            const auto rail=[&](float y,float z0,float z1){box({0,y,(z0+z1)/2},{.026F,.009F,z1-z0});for(double z=z0;z<z1;z+=.019)box({0,y+.006F,static_cast<float>(z)},{.033F,.005F,.008F},metal);};rail(.108F,-.026F,hand_end-.016F);
            for(const double x:{-1.0,1.0})for(double z=.180;z<static_cast<double>(hand_end)-.012;z+=.029)box({x*(heavy?.033:.028),.074,z},{.002,.012,.018},rubber,2);
            tube({0,.080F,hand_end-.018F},{0,.080F,front-.022F},heavy?.017F:support?.013F:.010F,dark);tube({0,.080F,front-.027F},{0,.080F,front},heavy?.022F:.015F,metal);tube({0,.080F,front+.0002F},{0,.080F,front+.001F},.007F,rubber,2);
            box({0,.125F,-.016F},{.024F,.030F,.022F});box({0,.132F,hand_end-.025F},{.016F,.043F,.016F});box({.024F,.074F,.057F},{.003F,.020F,.053F},rubber,2);box({.029F,.061F,.083F},{.013F,.010F,.019F},metal);
            for(const float x:{-.025F,.025F})for(const float z:{.015F,.135F})tube({x,.041F,z},{x*1.08F,.041F,z},.005F,metal);
            if(marksman){tube({0,.167F,.005F},{0,.167F,.204F},.023F,dark);tube({0,.167F,.182F},{0,.167F,.227F},.030F,dark);tube({0,.167F,.227F},{0,.167F,.228F},.023F,linearColor(0x243f46));for(const float z:{.036F,.146F})box({0,.136F,z},{.035F,.031F,.022F},metal);tube({0,.178F,.096F},{0,.200F,.096F},.012F,dark);}
            if(support){box({.032F,-.017F,.113F},{heavy?.133F:.105F,heavy?.139F:.117F,.122F},cloth,1);box({.032F,heavy?.057F:.046F,.113F},{heavy?.138F:.110F,.012F,.126F},dark);for(const float x:{-.032F,.032F})tube({x,.048F,hand_end-.035F},{x*1.5F,.029F,front-.060F},.005F,dark);builder.tubePath({{0,.113F,.02F},{0,.167F,.036F},{0,.167F,.137F},{0,.113F,.150F}},.003F,metal);}
            artifact.primary_grip={{-.020F,0,0},{1,0,0}};artifact.support_grip={{.022F,heavy?.039F:.043F,std::min(.24F,hand_end-.04F)},{1,0,0}};artifact.muzzle={{0,.080F,front+.002F},{1,0,0}};
        }else if(definition.visual_kind==WeaponVisualKind::Pistol){const float rear=-.061F,front=rear+definition.visual_length;const auto box=[&](DoubleVec3 p,DoubleVec3 s,foundation::Color c=foundation::Color{-1,-1,-1,-1},std::uint32_t group=0){if(c.a<0)c=dark;builder.box(p,s,c,group,.18);};const auto tube=[&](foundation::Vec3 a,foundation::Vec3 b,float r,foundation::Color c=foundation::Color{-1,-1,-1,-1},std::uint32_t group=0){if(c.a<0)c=metal;builder.cylinder(a,b,r,c,group,12);};
            builder.profile({{.042F,-.047F},{.033F,.020F},{-.077F,.001F},{-.073F,-.054F}},.032F,dark,1);box({0,-.075F,-.027F},{.039F,.012F,.059F},rubber,2);box({0,.030F,.066F},{.034F,.019F,.139F},dark,1);box({0,.037F,-.043F},{.039F,.014F,.046F},dark,1);
            for(const float x:{-.0168F,.0168F}){box({x,-.019F,-.024F},{.003F,.071F,.033F},rubber,2);for(double y=-.044;y<.013;y+=.012)box({x*1.05F,static_cast<float>(y),-.024F},{.002F,.003F,.029F},metal);box({x,.024F,-.017F},{.006F,.005F,.025F},metal);}
            builder.tubePath({{0,.028F,.018F},{0,-.015F,.030F},{0,-.015F,.071F},{0,.027F,.083F}},.003F,metal);builder.tubePath({{0,.027F,.046F},{0,.008F,.046F},{0,.001F,.038F}},.003F,dark);
            for(const float z:{.089F,.111F,.133F})box({0,.018F,z},{.028F,.005F,.009F},metal);tube({0,.060F,front-.035F},{0,.060F,front+.001F},.010F,metal);tube({0,.060F,front+.001F},{0,.060F,front+.0015F},.006F,rubber,2);
            ReferenceWeaponMeshBuilder slide_builder;const auto slide_box=[&](DoubleVec3 p,DoubleVec3 s,foundation::Color c=foundation::Color{-1,-1,-1,-1},std::uint32_t group=0){if(c.a<0)c=metal;slide_builder.box(p,s,c,group,.22);};
            slide_box({0,.060F,(front+rear)/2},{.036F,.039F,definition.visual_length},metal);slide_box({0,.081F,rear+.023F},{.029F,.006F,.016F},dark);slide_box({0,.085F,rear+.023F},{.011F,.003F,.017F},rubber,2);slide_box({0,.083F,front-.014F},{.007F,.007F,.012F},dark);slide_box({0,.087F,front-.014F},{.003F,.002F,.005F},linearColor(0xc4c9ab));slide_box({.0185F,.065F,.026F},{.002F,.014F,.029F},rubber,2);for(const float x:{-.0185F,.0185F})for(double z=static_cast<double>(rear)+.007;z<static_cast<double>(rear)+.044;z+=.008)slide_box({x,.058F,static_cast<float>(z)},{.002F,.026F,.003F},dark);
            artifact.slide=slide_builder.finish(true);artifact.primary_grip={{-.018F,-.006F,-.010F},{1,0,0}};artifact.support_grip={{.023F,-.009F,.015F},{1,0,0}};artifact.muzzle={{0,.060F,front+.002F},{1,0,0}};
        }else if(definition.visual_kind==WeaponVisualKind::Knife){builder.box({0,0,0},{.027F,.095F,.027F},rubber,2,.18F);builder.box({0,.052F,0},{.085F,.012F,.036F},metal,0,.18F);
            const auto a=builder.ring({0,.058F,0},{1,0,0},{0,0,1},.018F,.0035F,8,metal);const auto b=builder.ring({0,definition.visual_length-.06F,0},{1,0,0},{0,0,1},.012F,.003F,8,metal);builder.bridge(a,b);
            const auto tip=builder.vertex({0,definition.visual_length-.035F,0},metal,{0,1,0});for(std::size_t j=0;j<b.size();++j)builder.triangle(b[j],tip,b[(j+1)%b.size()]);
            artifact.primary_grip={{-.022F,0,0},{1,0,0}};
        }else{builder.ellipsoid({},{.030F,.045F,.030F},cloth,1,20,14);builder.box({0,.047F,0},{.024F,.019F,.023F},metal,0,.18F);builder.box({.025F,.012F,0},{.009F,.074F,.017F},metal,0,.18F);artifact.primary_grip={{-.022F,0,0},{1,0,0}};}
        artifact.mesh=builder.finish(definition.firearm);for(auto& vertex:artifact.mesh.vertices)vertex.position=multiply(vertex.position,variant.scale);
        artifact.mesh.minimum=multiply(artifact.mesh.minimum,variant.scale);artifact.mesh.maximum=multiply(artifact.mesh.maximum,variant.scale);
        if(!artifact.slide.vertices.empty()){for(auto& vertex:artifact.slide.vertices)vertex.position=multiply(vertex.position,variant.scale);artifact.slide.minimum=multiply(artifact.slide.minimum,variant.scale);artifact.slide.maximum=multiply(artifact.slide.maximum,variant.scale);}
        artifact.primary_grip.local_position=multiply(artifact.primary_grip.local_position,variant.scale);artifact.support_grip.local_position=multiply(artifact.support_grip.local_position,variant.scale);artifact.muzzle.local_position=multiply(artifact.muzzle.local_position,variant.scale);
        if(definition.firearm){ReferenceWeaponMeshBuilder flash;flash.ellipsoid({0,0,.032F},{.028F,.028F,.058F},{1,.65F,.14F,1},0,8,4);artifact.muzzle_flash=flash.finish();}
    }else{
    artifact.mesh.minimum = minimum;
    artifact.mesh.maximum = maximum;
    artifact.mesh.vertices.reserve(corners.size());
    for (const foundation::Vec3 corner : corners) {
        artifact.mesh.vertices.push_back({{corner.x * half.x, corner.y * half.y, corner.z * half.z},
                                          {corner.x, corner.y, corner.z}, {}, {}, variant.material_variant % 4U});
    }
    constexpr std::array<std::uint32_t, 36> indices{{
        0U, 1U, 2U, 0U, 2U, 3U, 4U, 6U, 5U, 4U, 7U, 6U,
        0U, 4U, 5U, 0U, 5U, 1U, 3U, 2U, 6U, 3U, 6U, 7U,
        0U, 3U, 7U, 0U, 7U, 4U, 1U, 5U, 6U, 1U, 6U, 2U,
    }};
    artifact.mesh.indices.assign(indices.begin(), indices.end());
    }
    if(artifact.mesh.vertices.size()==8U){
        artifact.muzzle = {definition.muzzle, {1.0F, 0.0F, 0.0F}};
        artifact.primary_grip = {definition.primary_grip, {1.0F, 0.0F, 0.0F}};
        artifact.support_grip = {definition.support_grip, {1.0F, 0.0F, 0.0F}};
    }
    artifact.stow_anchor = {definition.stow_anchor, {1.0F, 0.0F, 0.0F}};
    if(definition.visual_kind==WeaponVisualKind::Knife||definition.visual_kind==WeaponVisualKind::Grenade||definition.visual_kind==WeaponVisualKind::Long)
        artifact.primary_grip.local_rotation={.7071067811865475F,0,.7071067811865476F,0};
    if(definition.visual_kind==WeaponVisualKind::Long)
        artifact.support_grip.local_rotation={-.5F,.5F,.5F,.5F};
    if(definition.visual_kind==WeaponVisualKind::Pistol){artifact.primary_grip.local_rotation={.7048830810703123F,-.056034293257108715F,.7048830810703122F,-.05603429325710873F};artifact.support_grip.local_rotation={-.7048830810703123F,-.056034293257108715F,.7048830810703122F,.05603429325710873F};}
    return artifact.valid(definition)
               ? foundation::Result<WeaponArtifact, foundation::Error>::success(std::move(artifact))
               : foundation::Result<WeaponArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "weapon artifact validation failed"});
}

bool WeaponArtifact::valid(const WeaponDefinition& definition) const noexcept {
    if (version != 1U || weapon_id != definition.id || cache_key == 0U || mesh.vertices.empty() ||
        mesh.indices.empty() || !finite(mesh.minimum) || !finite(mesh.maximum)) {
        return false;
    }
    for (const WeaponVertex& vertex : mesh.vertices) {
        if (!finite(vertex.position) || !finite(vertex.normal) ||
            !std::isfinite(vertex.color.r) || !std::isfinite(vertex.color.g) ||
            !std::isfinite(vertex.color.b) || !std::isfinite(vertex.color.a)) {
            return false;
        }
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    const auto valid_optional=[](const WeaponMesh& candidate){if(candidate.vertices.empty())return candidate.indices.empty();if(candidate.indices.empty()||!finite(candidate.minimum)||!finite(candidate.maximum))return false;for(const auto index:candidate.indices)if(index>=candidate.vertices.size())return false;return true;};
    if(!valid_optional(slide)||!valid_optional(muzzle_flash))return false;
    return finite(muzzle.local_position) && finite(primary_grip.local_position) &&
           finite(support_grip.local_position) && finite(stow_anchor.local_position);
}

const WeaponArtifact* WeaponArtifactCache::find(foundation::StableId key) const noexcept {
    for (const WeaponArtifact& artifact : artifacts_) {
        if (artifact.cache_key == key) {
            return &artifact;
        }
    }
    return nullptr;
}

void WeaponArtifactCache::store(WeaponArtifact artifact) {
    for (WeaponArtifact& existing : artifacts_) {
        if (existing.cache_key == artifact.cache_key) {
            existing = std::move(artifact);
            return;
        }
    }
    artifacts_.push_back(std::move(artifact));
}

const WeaponArtifact* WeaponArtifactCache::acquire(const WeaponDefinition& definition,
                                                    const WeaponVariant& variant) {
    const foundation::StableId key = WeaponGeometryGenerator::cacheKey(definition, variant);
    if (const WeaponArtifact* existing = find(key); existing != nullptr) {
        return existing;
    }
    const auto built = WeaponGeometryGenerator::build(definition, variant);
    if (!built) {
        return nullptr;
    }
    store(built.value());
    return find(key);
}

} // namespace genomes::weapons
