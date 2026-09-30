#include <genomes/geometry/TangentSpace.hpp>

#if GENOMES_HAS_MIKKTSPACE
#include <mikktspace.h>
#endif

#include <utility>

namespace genomes::geometry {
#if GENOMES_HAS_MIKKTSPACE
namespace {
struct ContextData { const MeshData* source; MeshData* output; };
int faces(const SMikkTSpaceContext* context) { return static_cast<int>(context->m_pUserData ? static_cast<ContextData*>(context->m_pUserData)->source->indices.size()/3U : 0U); }
int vertices(const SMikkTSpaceContext*,int) { return 3; }
std::uint32_t index(const ContextData& data,int face,int vertex) { return data.source->indices[static_cast<std::size_t>(face)*3U+static_cast<std::size_t>(vertex)]; }
void position(const SMikkTSpaceContext* context,float out[],int face,int vertex){const auto& data=*static_cast<ContextData*>(context->m_pUserData);const auto& p=data.source->vertices[index(data,face,vertex)].position;out[0]=p.x;out[1]=p.y;out[2]=p.z;}
void normal(const SMikkTSpaceContext* context,float out[],int face,int vertex){const auto& data=*static_cast<ContextData*>(context->m_pUserData);const auto& n=data.source->vertices[index(data,face,vertex)].normal;out[0]=n.x;out[1]=n.y;out[2]=n.z;}
void uv(const SMikkTSpaceContext* context,float out[],int face,int vertex){const auto& data=*static_cast<ContextData*>(context->m_pUserData);const auto& uv=data.source->vertices[index(data,face,vertex)].uv;out[0]=uv.x;out[1]=uv.y;}
void tangent(const SMikkTSpaceContext* context,const float value[],float sign,int face,int vertex){auto& data=*static_cast<ContextData*>(context->m_pUserData);const auto i=index(data,face,vertex);data.output->tangents[i]={value[0],value[1],value[2],sign};}
}
#endif
foundation::Result<MeshData, foundation::Error> generateTangents(const MeshData& source) {
    using Result=foundation::Result<MeshData,foundation::Error>;
#if !GENOMES_HAS_MIKKTSPACE
    (void)source;return Result::failure({foundation::ErrorCode::Unsupported,"MikkTSpace is disabled"});
#else
    if(!source.valid()||source.vertices.empty())return Result::failure({foundation::ErrorCode::InvalidArgument,"tangents require an indexed vertex mesh"});
    MeshData result=source;result.tangents.assign(source.vertices.size(),{1,0,0,1});ContextData data{&source,&result};SMikkTSpaceInterface callbacks{};callbacks.m_getNumFaces=faces;callbacks.m_getNumVerticesOfFace=vertices;callbacks.m_getPosition=position;callbacks.m_getNormal=normal;callbacks.m_getTexCoord=uv;callbacks.m_setTSpaceBasic=tangent;SMikkTSpaceContext context{&callbacks,&data};if(!genTangSpaceDefault(&context))return Result::failure({foundation::ErrorCode::InvalidState,"MikkTSpace rejected mesh"});return Result::success(std::move(result));
#endif
}
} // namespace genomes::geometry
