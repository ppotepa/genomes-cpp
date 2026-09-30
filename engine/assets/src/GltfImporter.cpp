#include <genomes/assets/GltfImporter.hpp>
#include <genomes/geometry/MeshOperations.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <variant>
#include <utility>

namespace genomes::assets {
namespace { foundation::Error error(foundation::ErrorCode code,const char* message){return {code,message};} }

foundation::Result<math::Transform, foundation::Error>
readNodeTransform(const fastgltf::Node& node) {
    using Result = foundation::Result<math::Transform, foundation::Error>;
    math::Transform result{};
    if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
        result.translation = {trs->translation.x(), trs->translation.y(), trs->translation.z()};
        result.rotation = {trs->rotation.x(), trs->rotation.y(), trs->rotation.z(), trs->rotation.w()};
        result.scale = {trs->scale.x(), trs->scale.y(), trs->scale.z()};
    } else if (const auto* matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
        fastgltf::math::fvec3 scale{};
        fastgltf::math::fquat rotation{};
        fastgltf::math::fvec3 translation{};
        fastgltf::math::decomposeTransformMatrix(*matrix, scale, rotation, translation);
        result.translation = {translation.x(), translation.y(), translation.z()};
        result.rotation = {rotation.x(), rotation.y(), rotation.z(), rotation.w()};
        result.scale = {scale.x(), scale.y(), scale.z()};
    } else {
        return Result::failure(error(foundation::ErrorCode::Unsupported,
                                     "glTF node transform variant is unsupported"));
    }
    if (!result.valid())
        return Result::failure(error(foundation::ErrorCode::InvalidArgument,
                                     "glTF node transform is invalid"));
    return Result::success(result);
}

foundation::Result<ImportedScene, foundation::Error> importStaticGltf(const std::filesystem::path& path,ImportLimits limits) {
    using Result=foundation::Result<ImportedScene,foundation::Error>;
    if(path.empty()||!std::filesystem::exists(path))return Result::failure(error(foundation::ErrorCode::NotFound,"glTF path does not exist"));
    fastgltf::Parser parser; auto file=fastgltf::MappedGltfFile::FromPath(path); if(!file)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"cannot map glTF file"));
    constexpr auto options=fastgltf::Options::LoadExternalBuffers|fastgltf::Options::GenerateMeshIndices|fastgltf::Options::DecomposeNodeMatrices;
    auto parsed=parser.loadGltf(file.get(),path.parent_path(),options); if(parsed.error()!=fastgltf::Error::None)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"fastgltf rejected the asset")); auto asset=std::move(parsed.get());
    if(asset.meshes.size()>limits.max_meshes||asset.nodes.size()>limits.max_nodes)return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF exceeds import limits"));
    if(!asset.skins.empty()||!asset.animations.empty())return Result::failure(error(foundation::ErrorCode::Unsupported,"skins and animations are unsupported in glTF v1 importer"));
    for (const auto& buffer : asset.buffers) {
        if (const auto* uri = std::get_if<fastgltf::sources::URI>(&buffer.data);
            uri != nullptr && !uri->uri.isLocalPath() && !uri->uri.isDataUri())
            return Result::failure(error(foundation::ErrorCode::Unsupported,
                                         "remote glTF buffer URI is unsupported"));
    }
    ImportedScene scene{}; scene.materials.resize(asset.materials.size());
    for(std::size_t material_index=0;material_index<asset.materials.size();++material_index){const auto& source=asset.materials[material_index].pbrData;auto& target=scene.materials[material_index];target.base_color={static_cast<float>(source.baseColorFactor.x()),static_cast<float>(source.baseColorFactor.y()),static_cast<float>(source.baseColorFactor.z()),static_cast<float>(source.baseColorFactor.w())};target.metallic=static_cast<float>(source.metallicFactor);target.roughness=static_cast<float>(source.roughnessFactor);}
    std::size_t total_vertices=0,total_indices=0;
    std::vector<std::vector<geometry::MeshData>> primitive_meshes(asset.meshes.size());
    const auto accessor_in_bounds = [&asset](const std::size_t index) {
        return index < asset.accessors.size();
    };
    for (std::size_t mesh_index = 0; mesh_index < asset.meshes.size(); ++mesh_index)
        for(const auto& primitive:asset.meshes[mesh_index].primitives){
        if(primitive.type!=fastgltf::PrimitiveType::Triangles||primitive.dracoCompression)return Result::failure(error(foundation::ErrorCode::Unsupported,"only uncompressed triangles are supported"));
        const auto position_it=primitive.findAttribute("POSITION"); if(position_it==primitive.attributes.end())return Result::failure(error(foundation::ErrorCode::InvalidArgument,"POSITION is required"));
        if (!accessor_in_bounds(position_it->accessorIndex))
            return Result::failure(error(foundation::ErrorCode::OutOfRange,
                                         "POSITION accessor index is out of range"));
        const auto& position_accessor=asset.accessors[position_it->accessorIndex];
        if(position_accessor.type!=fastgltf::AccessorType::Vec3)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"POSITION must be VEC3"));
        if(position_accessor.count > limits.max_vertices - std::min(total_vertices, limits.max_vertices))return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF vertex limit exceeded"));
        geometry::MeshData output{}; output.vertices.resize(position_accessor.count); fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,position_accessor,[&](auto p,std::size_t i){output.vertices[i].position={p.x(),p.y(),p.z()};});
        const auto normal_it=primitive.findAttribute("NORMAL"); if(normal_it!=primitive.attributes.end()){if(!accessor_in_bounds(normal_it->accessorIndex))return Result::failure(error(foundation::ErrorCode::OutOfRange,"NORMAL accessor index is out of range"));const auto& a=asset.accessors[normal_it->accessorIndex];if(a.type!=fastgltf::AccessorType::Vec3||a.count!=position_accessor.count)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"NORMAL must match POSITION as VEC3"));fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,a,[&](auto n,std::size_t i){output.vertices[i].normal={n.x(),n.y(),n.z()};});}
        const auto uv_it=primitive.findAttribute("TEXCOORD_0"); if(uv_it!=primitive.attributes.end()){if(!accessor_in_bounds(uv_it->accessorIndex))return Result::failure(error(foundation::ErrorCode::OutOfRange,"TEXCOORD_0 accessor index is out of range"));const auto& a=asset.accessors[uv_it->accessorIndex];if(a.type!=fastgltf::AccessorType::Vec2||a.count!=position_accessor.count)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"TEXCOORD_0 must match POSITION as VEC2"));fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset,a,[&](auto uv,std::size_t i){output.vertices[i].uv={uv.x(),uv.y()};});}
        const auto tangent_it=primitive.findAttribute("TANGENT"); if(tangent_it!=primitive.attributes.end()){if(!accessor_in_bounds(tangent_it->accessorIndex))return Result::failure(error(foundation::ErrorCode::OutOfRange,"TANGENT accessor index is out of range"));const auto& a=asset.accessors[tangent_it->accessorIndex];if(a.type!=fastgltf::AccessorType::Vec4||a.count!=position_accessor.count)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"TANGENT must match POSITION as VEC4"));output.tangents.resize(position_accessor.count);fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset,a,[&](auto tangent,std::size_t i){output.tangents[i]={tangent.x(),tangent.y(),tangent.z(),tangent.w()};});}
        const auto color_it=primitive.findAttribute("COLOR_0"); if(color_it!=primitive.attributes.end()){if(!accessor_in_bounds(color_it->accessorIndex))return Result::failure(error(foundation::ErrorCode::OutOfRange,"COLOR_0 accessor index is out of range"));const auto& a=asset.accessors[color_it->accessorIndex];if(a.count!=position_accessor.count)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"COLOR_0 must match POSITION"));output.colors.resize(position_accessor.count);if(a.type==fastgltf::AccessorType::Vec3){fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,a,[&](auto color,std::size_t i){output.colors[i]={color.x(),color.y(),color.z(),1.0F};});}else if(a.type==fastgltf::AccessorType::Vec4){fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset,a,[&](auto color,std::size_t i){output.colors[i]={color.x(),color.y(),color.z(),color.w()};});}else return Result::failure(error(foundation::ErrorCode::InvalidArgument,"COLOR_0 must be VEC3 or VEC4"));}
        if(primitive.indicesAccessor.has_value()){if(!accessor_in_bounds(primitive.indicesAccessor.value()))return Result::failure(error(foundation::ErrorCode::OutOfRange,"index accessor is out of range"));const auto& index_accessor=asset.accessors[primitive.indicesAccessor.value()]; output.indices.resize(index_accessor.count); fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset,index_accessor,[&](std::uint32_t index,std::size_t i){output.indices[i]=index;});}else{if(position_accessor.count%3U!=0U)return Result::failure(error(foundation::ErrorCode::InvalidArgument,"non-indexed triangles require a multiple of three vertices"));output.indices.resize(position_accessor.count);for(std::size_t index=0;index<output.indices.size();++index)output.indices[index]=static_cast<std::uint32_t>(index);}
        if(output.indices.size()>limits.max_indices-std::min(total_indices,limits.max_indices))return Result::failure(error(foundation::ErrorCode::OutOfRange,"glTF index limit exceeded"));
        if(primitive.materialIndex.has_value()&&primitive.materialIndex.value()>=scene.materials.size())return Result::failure(error(foundation::ErrorCode::OutOfRange,"primitive material index is out of range"));
        total_vertices+=output.vertices.size();total_indices+=output.indices.size(); output.rebuildStreams();output.submeshes.push_back({0U,static_cast<std::uint32_t>(output.indices.size()),primitive.materialIndex.value_or(0U)});if(!output.valid())return Result::failure(error(foundation::ErrorCode::InvalidArgument,"imported primitive failed validation"));primitive_meshes[mesh_index].push_back(std::move(output));
    }
    scene.meshes.reserve(asset.meshes.size());
    for (auto& primitives : primitive_meshes) {
        if (primitives.empty()) {
            scene.meshes.emplace_back();
            continue;
        }
        const bool any_tangents = std::any_of(primitives.begin(), primitives.end(),
                                              [](const geometry::MeshData& mesh) {
                                                  return !mesh.tangents.empty();
                                              });
        const bool any_colors = std::any_of(primitives.begin(), primitives.end(),
                                            [](const geometry::MeshData& mesh) {
                                                return !mesh.colors.empty();
                                            });
        for (auto& primitive : primitives) {
            const auto vertex_count = primitive.vertices.size();
            if (any_tangents && primitive.tangents.empty())
                primitive.tangents.assign(vertex_count, math::Vec4{1.0F, 0.0F, 0.0F, 1.0F});
            if (any_colors && primitive.colors.empty())
                primitive.colors.assign(vertex_count, foundation::Color{});
        }
        const auto combined = geometry::combine(std::span<const geometry::MeshData>(
            primitives.data(), primitives.size()));
        if (!combined)
            return Result::failure(error(foundation::ErrorCode::InvalidArgument,
                                         "glTF mesh primitives could not be combined"));
        scene.meshes.push_back(combined.value());
    }
    std::vector<std::size_t> parents(asset.nodes.size(), std::numeric_limits<std::size_t>::max());
    for (std::size_t parent_index = 0; parent_index < asset.nodes.size(); ++parent_index) {
        for (const auto child_index : asset.nodes[parent_index].children) {
            if (child_index >= asset.nodes.size())
                return Result::failure(error(foundation::ErrorCode::OutOfRange,
                                             "glTF child node index is out of range"));
            if (parents[child_index] != std::numeric_limits<std::size_t>::max() &&
                parents[child_index] != parent_index)
                return Result::failure(error(foundation::ErrorCode::InvalidArgument,
                                             "glTF node has multiple parents"));
            parents[child_index] = parent_index;
        }
    }
    std::vector<std::optional<math::Transform>> world_transforms(asset.nodes.size());
    std::vector<bool> resolving(asset.nodes.size(), false);
    std::function<foundation::Result<math::Transform, foundation::Error>(std::size_t)> resolve_world =
        [&](const std::size_t node_index) -> foundation::Result<math::Transform, foundation::Error> {
            if (world_transforms[node_index].has_value())
                return foundation::Result<math::Transform, foundation::Error>::success(
                    *world_transforms[node_index]);
            if (resolving[node_index])
                return foundation::Result<math::Transform, foundation::Error>::failure(
                    error(foundation::ErrorCode::InvalidArgument, "glTF node hierarchy contains a cycle"));
            resolving[node_index] = true;
            const auto local = readNodeTransform(asset.nodes[node_index]);
            if (!local) {
                resolving[node_index] = false;
                return foundation::Result<math::Transform, foundation::Error>::failure(local.error());
            }
            math::Transform world = local.value();
            if (parents[node_index] != std::numeric_limits<std::size_t>::max()) {
                const auto parent_world = resolve_world(parents[node_index]);
                if (!parent_world) {
                    resolving[node_index] = false;
                    return foundation::Result<math::Transform, foundation::Error>::failure(parent_world.error());
                }
                world = parent_world.value() * local.value();
            }
            resolving[node_index] = false;
            world_transforms[node_index] = world;
            return foundation::Result<math::Transform, foundation::Error>::success(world);
        };
    for (std::size_t node_index = 0; node_index < asset.nodes.size(); ++node_index) {
        if (!asset.nodes[node_index].meshIndex.has_value()) continue;
        if (asset.nodes[node_index].meshIndex.value() >= scene.meshes.size())
            return Result::failure(error(foundation::ErrorCode::OutOfRange,
                                         "node mesh index is out of range"));
        const auto local = readNodeTransform(asset.nodes[node_index]);
        if (!local) return Result::failure(local.error());
        const auto world = resolve_world(node_index);
        if (!world) return Result::failure(world.error());
        ImportedNode imported{};
        imported.name = asset.nodes[node_index].name;
        imported.mesh_index = asset.nodes[node_index].meshIndex.value();
        imported.local = local.value();
        imported.world = world.value();
        scene.nodes.push_back(std::move(imported));
    }
    return Result::success(std::move(scene));
}
} // namespace genomes::assets
