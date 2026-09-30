#include <genomes/assets/GltfImporter.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <cmath>
#include <utility>

namespace genomes::assets {
namespace { foundation::Error error(foundation::ErrorCode code,const char* message){return {code,message};} }
foundation::Result<ImportedScene, foundation::Error> importStaticGltf(const std::filesystem::path& path,ImportLimits limits) {
    using Result=foundation::Result<ImportedScene,foundation::Error>;
    if(path.empty()||!std::filesystem::exists(path))return Result::failure(error(foundation::ErrorCode::NotFound,"glTF path does not exist"));
    fastgltf::Parser parser; auto file=fastgltf::MappedGltfFile::FromPath(path); if(!file)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"cannot map glTF file"));
    constexpr auto options=fastgltf::Options::LoadExternalBuffers|fastgltf::Options::GenerateMeshIndices|fastgltf::Options::DecomposeNodeMatrices;
    auto parsed=parser.loadGltf(file.get(),path.parent_path(),options); if(parsed.error()!=fastgltf::Error::None)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"fastgltf rejected the asset")); auto asset=std::move(parsed.get());
    if(asset.meshes.size()>limits.max_meshes||asset.nodes.size()>limits.max_nodes)return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF exceeds import limits"));
    if(!asset.skins.empty()||!asset.animations.empty())return Result::failure(error(foundation::ErrorCode::Unsupported,"skins and animations are unsupported in glTF v1 importer"));
    ImportedScene scene{}; scene.materials.resize(asset.materials.size());
    for(std::size_t material_index=0;material_index<asset.materials.size();++material_index){const auto& source=asset.materials[material_index].pbrData;auto& target=scene.materials[material_index];target.base_color={static_cast<float>(source.baseColorFactor.x()),static_cast<float>(source.baseColorFactor.y()),static_cast<float>(source.baseColorFactor.z()),static_cast<float>(source.baseColorFactor.w())};target.metallic=static_cast<float>(source.metallicFactor);target.roughness=static_cast<float>(source.roughnessFactor);}
    std::size_t total_vertices=0,total_indices=0;
    for(const auto& mesh:asset.meshes) for(const auto& primitive:mesh.primitives){
        if(primitive.type!=fastgltf::PrimitiveType::Triangles||primitive.dracoCompression)return Result::failure(error(foundation::ErrorCode::Unsupported,"only uncompressed triangles are supported"));
        const auto position_it=primitive.findAttribute("POSITION"); if(position_it==primitive.attributes.end())return Result::failure(error(foundation::ErrorCode::InvalidArgument,"POSITION is required"));
        const auto& position_accessor=asset.accessors[position_it->accessorIndex]; if(total_vertices+position_accessor.count>limits.max_vertices)return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF vertex limit exceeded"));
        geometry::MeshData output{}; output.vertices.resize(position_accessor.count); fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,position_accessor,[&](auto p,std::size_t i){output.vertices[i].position={p.x(),p.y(),p.z()};});
        const auto normal_it=primitive.findAttribute("NORMAL"); if(normal_it!=primitive.attributes.end()){const auto& a=asset.accessors[normal_it->accessorIndex];fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,a,[&](auto n,std::size_t i){output.vertices[i].normal={n.x(),n.y(),n.z()};});}
        const auto uv_it=primitive.findAttribute("TEXCOORD_0"); if(uv_it!=primitive.attributes.end()){const auto& a=asset.accessors[uv_it->accessorIndex];fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset,a,[&](auto uv,std::size_t i){output.vertices[i].uv={uv.x(),uv.y()};});}
        if(!primitive.indicesAccessor.has_value())return Result::failure(error(foundation::ErrorCode::InvalidArgument,"indexed primitive generation failed")); const auto& index_accessor=asset.accessors[primitive.indicesAccessor.value()]; output.indices.resize(index_accessor.count); fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset,index_accessor,[&](std::uint32_t index,std::size_t i){output.indices[i]=index;});
        total_vertices+=output.vertices.size();total_indices+=output.indices.size();if(total_indices>limits.max_indices)return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF index limit exceeded")); output.rebuildStreams();output.submeshes.push_back({0U,static_cast<std::uint32_t>(output.indices.size()),primitive.materialIndex.value_or(0U)});if(!output.valid())return Result::failure(error(foundation::ErrorCode::InvalidArgument,"imported primitive failed validation"));scene.meshes.push_back(std::move(output));
    }
    for(const auto& node:asset.nodes){if(!node.meshIndex.has_value())continue;const auto& trs=std::get<fastgltf::TRS>(node.transform);ImportedNode imported{};imported.name=node.name;imported.mesh_index=node.meshIndex.value();imported.local.translation={trs.translation.x(),trs.translation.y(),trs.translation.z()};imported.local.rotation={trs.rotation.x(),trs.rotation.y(),trs.rotation.z(),trs.rotation.w()};imported.local.scale={trs.scale.x(),trs.scale.y(),trs.scale.z()};imported.world=imported.local;scene.nodes.push_back(std::move(imported));}
    return Result::success(std::move(scene));
}
} // namespace genomes::assets
