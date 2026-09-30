#include <genomes/assets/AssetPresentation.hpp>
#include <genomes/foundation/StableHash.hpp>

namespace genomes::assets {
PresentedAsset present(const ImportedScene& source) {
    PresentedAsset result{}; result.meshes=source.meshes; result.materials=source.materials;
    auto revision=genomes::foundation::stableHashU64(source.meshes.size());
    for(const auto& mesh:result.meshes){for(const auto& vertex:mesh.vertices)result.bounds.include(vertex.position);revision=genomes::foundation::stableHashCombine(revision,mesh.vertices.size());revision=genomes::foundation::stableHashCombine(revision,mesh.indices.size());}
    result.revision=revision; return result;
}
} // namespace genomes::assets
