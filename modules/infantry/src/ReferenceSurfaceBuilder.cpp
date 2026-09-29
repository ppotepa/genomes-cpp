#include <genomes/infantry/ReferenceSurfaceBuilder.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include <utility>

namespace genomes::infantry {

namespace {

[[nodiscard]] ReferenceVec3 subtract(ReferenceVec3 a, ReferenceVec3 b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
[[nodiscard]] ReferenceVec3 cross(ReferenceVec3 a, ReferenceVec3 b) noexcept {
    // Match JavaScript's separate IEEE double multiply/subtract operations;
    // do not let the optimizer contract them into fused multiply-adds.
    volatile double xy=a.y*b.z,xz=a.z*b.y;
    volatile double yz=a.z*b.x,yx=a.x*b.z;
    volatile double zx=a.x*b.y,zy=a.y*b.x;
    return {static_cast<double>(xy)-static_cast<double>(xz),
            static_cast<double>(yz)-static_cast<double>(yx),
            static_cast<double>(zx)-static_cast<double>(zy)};
}
[[nodiscard]] double dot(ReferenceVec3 a, ReferenceVec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

[[nodiscard]] double jsSubtract(float left, float right) noexcept {
    volatile double value=static_cast<double>(left)-static_cast<double>(right);
    return value;
}

void addFloat32(float& accumulator, double value) noexcept {
    volatile double sum=static_cast<double>(accumulator)+value;
    volatile float rounded=static_cast<float>(sum);
    accumulator=rounded;
}

[[nodiscard]] double jsLength(float x,float y,float z) noexcept {
    volatile double xx=static_cast<double>(x)*static_cast<double>(x);
    volatile double yy=static_cast<double>(y)*static_cast<double>(y);
    volatile double zz=static_cast<double>(z)*static_cast<double>(z);
    volatile double xy=static_cast<double>(xx)+static_cast<double>(yy);
    return std::sqrt(static_cast<double>(xy)+static_cast<double>(zz));
}

} // namespace

ReferenceSurfaceBuilder::ReferenceSurfaceBuilder(double height, std::size_t bone_count)
    : height_(height), bone_count_(bone_count) {}

void ReferenceSurfaceBuilder::setTag(std::string_view name) {
    current_tag_=name;
    if(current_tag_.empty())return;
    const auto found=std::find_if(tags_.begin(),tags_.end(),[this](const RawTag& tag){
        return tag.name==current_tag_;});
    if(found==tags_.end())tags_.push_back({current_tag_,{}});
}

ReferenceSurfaceBuilder::VertexIndex ReferenceSurfaceBuilder::vertex(
    ReferenceVec3 position,std::span<const ReferenceInfluence> influences,
    ReferenceColor color,ReferenceVec3 hint,std::array<double,2U> uv,
    std::uint16_t material_region) {
    RawVertex raw{};raw.position=position;raw.hint=hint;raw.uv=uv;raw.color=color;
    raw.material_region=material_region;
    for(const auto influence:influences)if(influence.weight>0.0F&&influence.bone_index>=bone_count_)
        throw std::invalid_argument("reference surface vertex has an unknown bone");
    std::array<ReferenceInfluence,4U> sorted{};std::uint8_t count=0U;double total=0.0;
    for(const auto influence:influences){if(!(influence.weight>0.0))continue;
        std::uint8_t at=count;if(count==4U){if(influence.weight<=sorted[3].weight)continue;at=3U;}else ++count;
        while(at>0U&&sorted[at-1U].weight<influence.weight){if(at<4U)sorted[at]=sorted[at-1U];--at;}
        sorted[at]=influence;}
    for(std::uint8_t index=0U;index<count;++index)total+=sorted[index].weight;
    if(!(total>0.0))throw std::invalid_argument("reference surface vertex has no weights");
    for(std::uint8_t index=0U;index<count;++index)sorted[index].weight/=total;
    raw.influences=sorted;raw.influence_count=count;
    const auto index=static_cast<VertexIndex>(vertices_.size());vertices_.push_back(raw);
    if(!current_tag_.empty()){
        const auto found=std::find_if(tags_.begin(),tags_.end(),[this](const RawTag& tag){
            return tag.name==current_tag_;});
        found->vertices.push_back(index);
    }
    return index;
}

ReferenceVec3 ReferenceSurfaceBuilder::point(VertexIndex index) const noexcept {
    return vertices_[index].position;
}

void ReferenceSurfaceBuilder::morph(std::string_view name,VertexIndex index,ReferenceVec3 delta) {
    constexpr std::array<std::string_view,4U> names{"eyelidsClose","eyelidsArc","neckFlex","handsRelax"};
    if(omit_facial_morphs_&&name!="handsRelax")return;
    const auto found=std::find(names.begin(),names.end(),name);
    if(found==names.end()||index>=vertices_.size())throw std::invalid_argument("unknown reference surface morph");
    auto& values=morphs_[static_cast<std::size_t>(found-names.begin())];
    if(values.size()<vertices_.size())values.resize(vertices_.size());values[index]=delta;
}

std::vector<foundation::Vec3> ReferenceSurfaceBuilder::morphPositions(std::string_view name) const {
    constexpr std::array<std::string_view,4U> names{"eyelidsClose","eyelidsArc","neckFlex","handsRelax"};
    const auto found=std::find(names.begin(),names.end(),name);std::vector<foundation::Vec3> result(vertices_.size());if(found==names.end())return result;
    const auto& values=morphs_[static_cast<std::size_t>(found-names.begin())];
    for(std::size_t index=0;index<values.size();++index){if(!values[index])continue;result[index]={
        static_cast<float>(values[index]->x*height_),static_cast<float>(values[index]->y*height_),
        static_cast<float>(values[index]->z*height_)};}
    return result;
}

std::vector<foundation::Vec3> ReferenceSurfaceBuilder::morphNormals(
    std::string_view name,const AppearanceMesh& base) const {
    const auto deltas=morphPositions(name);std::vector<foundation::Vec3> positions;
    positions.reserve(base.vertices.size());for(std::size_t index=0;index<base.vertices.size();++index){const auto& p=base.vertices[index].position;
        positions.push_back({static_cast<float>(p.x+deltas[index].x),static_cast<float>(p.y+deltas[index].y),static_cast<float>(p.z+deltas[index].z)});}
    std::vector<foundation::Vec3> normals(base.vertices.size());for(std::size_t offset=0;offset<base.indices.size();offset+=3U){const auto a=base.indices[offset],b=base.indices[offset+1U],c=base.indices[offset+2U];
        const auto& pa=positions[a];const auto& pb=positions[b];const auto& pc=positions[c];const auto normal=cross(
            {jsSubtract(pc.x,pb.x),jsSubtract(pc.y,pb.y),jsSubtract(pc.z,pb.z)},
            {jsSubtract(pa.x,pb.x),jsSubtract(pa.y,pb.y),jsSubtract(pa.z,pb.z)});
        for(const auto vertex:{a,b,c}){addFloat32(normals[vertex].x,normal.x);addFloat32(normals[vertex].y,normal.y);addFloat32(normals[vertex].z,normal.z);}}
    for(std::size_t index=0;index<normals.size();++index){auto& value=normals[index];const double length=jsLength(value.x,value.y,value.z);
        if(length>0){value.x=static_cast<float>(value.x/length);value.y=static_cast<float>(value.y/length);value.z=static_cast<float>(value.z/length);}value.x-=base.vertices[index].normal.x;value.y-=base.vertices[index].normal.y;value.z-=base.vertices[index].normal.z;}
    return normals;
}

void ReferenceSurfaceBuilder::triangle(VertexIndex a,VertexIndex b,VertexIndex c,
                                       std::uint16_t material) {
    if(material>=triangles_.size()||a>=vertices_.size()||b>=vertices_.size()||c>=vertices_.size())return;
    // JS performs the primitive-local winding test while positions are still
    // held in the builder's number arrays. The Float32 conversion happens
    // later, during finish(), before the global adjacency pass.
    const auto& pa=vertices_[a].position;
    const auto& pb=vertices_[b].position;
    const auto& pc=vertices_[c].position;
    const auto normal=cross(subtract(pb,pa),subtract(pc,pa));
    if(dot(normal,normal)<1e-18)return;
    const ReferenceVec3 hint{vertices_[a].hint.x+vertices_[b].hint.x+vertices_[c].hint.x,
                             vertices_[a].hint.y+vertices_[b].hint.y+vertices_[c].hint.y,
                             vertices_[a].hint.z+vertices_[b].hint.z+vertices_[c].hint.z};
    if(dot(normal,hint)<0.0)std::swap(b,c);
    triangles_[material].insert(triangles_[material].end(),{a,b,c});
}

void ReferenceSurfaceBuilder::bridge(std::span<const VertexIndex> first,
                                     std::span<const VertexIndex> second,
                                     std::uint16_t material) {
    if(first.size()!=second.size())return;
    for(std::size_t index=0;index<first.size();++index){const auto next=(index+1U)%first.size();
        triangle(first[index],second[index],first[next],material);
        triangle(first[next],second[index],second[next],material);}
}

void ReferenceSurfaceBuilder::cap(std::span<const VertexIndex> loop,
                                  std::span<const ReferenceInfluence> influences,
                                  ReferenceColor color,ReferenceVec3 normal,
                                  std::uint16_t material,std::uint16_t material_region) {
    if(loop.empty())return;ReferenceVec3 center{};
    for(const auto index:loop){const auto p=point(index);center.x+=p.x;center.y+=p.y;center.z+=p.z;}
    const double inverse=1.0/static_cast<double>(loop.size());
    center.x*=inverse;center.y*=inverse;center.z*=inverse;
    const auto middle=vertex(center,influences,color,normal,{},material_region);
    for(std::size_t index=0;index<loop.size();++index)
        triangle(middle,loop[index],loop[(index+1U)%loop.size()],material);
}

AppearanceMesh ReferenceSurfaceBuilder::finalize() {
    AppearanceMesh mesh{};mesh.vertices.reserve(vertices_.size());
    const auto jsFloatPosition=[this](double value) noexcept {
        volatile double product=value*height_;
        return static_cast<float>(product);
    };
    for(const auto& raw:vertices_){AppearanceVertex vertex{};
        vertex.position={jsFloatPosition(raw.position.x),jsFloatPosition(raw.position.y),
                         jsFloatPosition(raw.position.z)};
        vertex.normal={static_cast<float>(raw.hint.x),static_cast<float>(raw.hint.y),
                       static_cast<float>(raw.hint.z)};
        vertex.uv={static_cast<float>(raw.uv[0]),static_cast<float>(raw.uv[1])};
        vertex.color={static_cast<float>(raw.color.r),static_cast<float>(raw.color.g),
                      static_cast<float>(raw.color.b),1.0F};
        for(std::size_t index=0U;index<raw.influences.size();++index){
            vertex.influences[index].bone_index=raw.influences[index].bone_index;
            vertex.influences[index].weight=static_cast<float>(raw.influences[index].weight);}
        vertex.influence_count=raw.influence_count;
        vertex.material_region=raw.material_region;mesh.vertices.push_back(vertex);}
    for(std::uint16_t material=0U;material<triangles_.size();++material){const auto& values=triangles_[material];
        if(values.empty())continue;mesh.groups.push_back({static_cast<std::uint32_t>(mesh.indices.size()),
            static_cast<std::uint32_t>(values.size()),material});mesh.indices.insert(mesh.indices.end(),values.begin(),values.end());}

    const std::size_t triangle_count=mesh.indices.size()/3U,vertex_count=mesh.vertices.size();
    struct Edge final {std::uint32_t triangle;bool direction;};
    std::unordered_map<std::uint64_t,Edge> edges;
    std::vector<std::vector<std::pair<std::uint32_t,std::uint8_t>>> neighbours(triangle_count);
    for(std::uint32_t triangle=0U;triangle<triangle_count;++triangle)for(std::uint32_t edge=0U;edge<3U;++edge){
        const auto a=mesh.indices[triangle*3U+edge],b=mesh.indices[triangle*3U+(edge+1U)%3U];
        const auto low=std::min(a,b),high=std::max(a,b);
        const std::uint64_t key=static_cast<std::uint64_t>(low)*vertex_count+high;
        const Edge entry{triangle,a<b};const auto found=edges.find(key);
        if(found==edges.end())edges.emplace(key,entry);else{const std::uint8_t different=
            found->second.direction==entry.direction?1U:0U;neighbours[triangle].push_back({found->second.triangle,different});
            neighbours[found->second.triangle].push_back({triangle,different});}}
    std::vector<std::int8_t> flips(triangle_count,-1);
    // Three.js performs the connected-face orientation while the builder's
    // authoring-number position array is still alive (before the Float32
    // BufferAttribute is created). Reconstruct that point() path here rather
    // than using the already-quantized mesh positions.
    const auto authoringPoint=[this](std::uint32_t index) noexcept {
        // SurfaceBuilder.point() reads the already-created Float32 position
        // attribute and divides by H; mirror that quantization exactly for
        // connected-face orientation.
        const float x=static_cast<float>(vertices_[index].position.x*height_);
        const float y=static_cast<float>(vertices_[index].position.y*height_);
        const float z=static_cast<float>(vertices_[index].position.z*height_);
        return ReferenceVec3{static_cast<double>(x)/height_,
                             static_cast<double>(y)/height_,
                             static_cast<double>(z)/height_};
    };
    for(std::uint32_t start=0U;start<triangle_count;++start){if(flips[start]!=-1)continue;
        flips[start]=0;std::vector<std::uint32_t> queue{start};double score=0.0;
        for(std::size_t at=0U;at<queue.size();++at){const auto triangle=queue[at];
            for(const auto [next,delta]:neighbours[triangle])if(flips[next]==-1){flips[next]=
                static_cast<std::int8_t>(flips[triangle]^delta);queue.push_back(next);}
            const auto a=mesh.indices[triangle*3U],b=mesh.indices[triangle*3U+1U],c=mesh.indices[triangle*3U+2U];
            const auto pa=authoringPoint(a);
            const auto pb=authoringPoint(b);
            const auto pc=authoringPoint(c);
            const ReferenceVec3 normal{pc.x-pa.x,pc.y-pa.y,pc.z-pa.z};
            const ReferenceVec3 edge_b{pb.x-pa.x,pb.y-pa.y,pb.z-pa.z};
            const ReferenceVec3 edge_c{normal.x,normal.y,normal.z};
            const auto oriented=cross(edge_b,edge_c);
            const ReferenceVec3 hint{vertices_[a].hint.x+vertices_[b].hint.x+vertices_[c].hint.x,
                vertices_[a].hint.y+vertices_[b].hint.y+vertices_[c].hint.y,
                vertices_[a].hint.z+vertices_[b].hint.z+vertices_[c].hint.z};
            const double value=dot(oriented,hint);score+=flips[triangle]?-value:value;}
        const std::uint8_t invert=score<0.0?1U:0U;for(const auto triangle:queue)
            if((flips[triangle]^invert)!=0)std::swap(mesh.indices[triangle*3U+1U],mesh.indices[triangle*3U+2U]);}

    for(auto& vertex:mesh.vertices)vertex.normal={};
    const auto jsSubtractDouble=[](double left,double right) noexcept {volatile double value=left-right;return static_cast<double>(value);};
    for(std::size_t offset=0U;offset<mesh.indices.size();offset+=3U){const auto a=mesh.indices[offset],b=mesh.indices[offset+1U],c=mesh.indices[offset+2U];
        const auto& pa=vertices_[a].position;const auto& pb=vertices_[b].position;const auto& pc=vertices_[c].position;
        const auto normal=cross({jsSubtractDouble(pc.x,pb.x),jsSubtractDouble(pc.y,pb.y),jsSubtractDouble(pc.z,pb.z)},
                                {jsSubtractDouble(pa.x,pb.x),jsSubtractDouble(pa.y,pb.y),jsSubtractDouble(pa.z,pb.z)});
        for(const auto index:{a,b,c}){addFloat32(mesh.vertices[index].normal.x,normal.x);
            addFloat32(mesh.vertices[index].normal.y,normal.y);addFloat32(mesh.vertices[index].normal.z,normal.z);}}
    for(auto& vertex:mesh.vertices){const double length=jsLength(vertex.normal.x,vertex.normal.y,vertex.normal.z);if(length>0.0){vertex.normal.x=static_cast<float>(vertex.normal.x/length);
        vertex.normal.y=static_cast<float>(vertex.normal.y/length);vertex.normal.z=static_cast<float>(vertex.normal.z/length);}}
    for(auto& tag:tags_)mesh.tags.push_back({std::move(tag.name),std::move(tag.vertices)});
    if(mesh.vertices.empty())return mesh;
    mesh.minimum={std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()};
    mesh.maximum={-mesh.minimum.x,-mesh.minimum.y,-mesh.minimum.z};
    for(const auto& vertex:mesh.vertices){mesh.minimum.x=std::min(mesh.minimum.x,vertex.position.x);mesh.minimum.y=std::min(mesh.minimum.y,vertex.position.y);mesh.minimum.z=std::min(mesh.minimum.z,vertex.position.z);
        mesh.maximum.x=std::max(mesh.maximum.x,vertex.position.x);mesh.maximum.y=std::max(mesh.maximum.y,vertex.position.y);mesh.maximum.z=std::max(mesh.maximum.z,vertex.position.z);}
    for(const auto& morph:morphs_){if(morph.empty())continue;foundation::Vec3 delta_min{},delta_max{};for(const auto& value:morph){const foundation::Vec3 delta=value?foundation::Vec3{
                static_cast<float>(value->x*height_),static_cast<float>(value->y*height_),static_cast<float>(value->z*height_)}:foundation::Vec3{};
            delta_min.x=std::min(delta_min.x,delta.x);delta_min.y=std::min(delta_min.y,delta.y);delta_min.z=std::min(delta_min.z,delta.z);
            delta_max.x=std::max(delta_max.x,delta.x);delta_max.y=std::max(delta_max.y,delta.y);delta_max.z=std::max(delta_max.z,delta.z);}
        mesh.minimum.x+=std::min(0.0F,delta_min.x);mesh.minimum.y+=std::min(0.0F,delta_min.y);mesh.minimum.z+=std::min(0.0F,delta_min.z);
        mesh.maximum.x+=std::max(0.0F,delta_max.x);mesh.maximum.y+=std::max(0.0F,delta_max.y);mesh.maximum.z+=std::max(0.0F,delta_max.z);}
    mesh.sphere_center={(mesh.minimum.x+mesh.maximum.x)*.5F,(mesh.minimum.y+mesh.maximum.y)*.5F,(mesh.minimum.z+mesh.maximum.z)*.5F};
    float radius_squared=0.0F;const auto include=[&](foundation::Vec3 point){const float x=point.x-mesh.sphere_center.x,y=point.y-mesh.sphere_center.y,z=point.z-mesh.sphere_center.z;radius_squared=std::max(radius_squared,x*x+y*y+z*z);};
    for(const auto& vertex:mesh.vertices)include(vertex.position);
    for(const auto& morph:morphs_)for(std::size_t index=0;index<morph.size();++index){if(!morph[index])continue;include({
        static_cast<float>(mesh.vertices[index].position.x+static_cast<float>(morph[index]->x*height_)),
        static_cast<float>(mesh.vertices[index].position.y+static_cast<float>(morph[index]->y*height_)),
        static_cast<float>(mesh.vertices[index].position.z+static_cast<float>(morph[index]->z*height_))});}
    mesh.sphere_radius=std::sqrt(radius_squared);
    return mesh;
}

} // namespace genomes::infantry
