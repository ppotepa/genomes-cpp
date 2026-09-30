#include <genomes/assets/GltfImporter.hpp>
#include <genomes/assets/AssetPresentation.hpp>
#include <cassert>

int main() {
    const auto scene=genomes::assets::importStaticGltf(
        std::filesystem::path(GENOMES_SOURCE_DIR) / "tests/fixtures/assets/static_triangle.gltf");
    assert(scene);
    assert(scene.value().meshes.size()==1U && scene.value().meshes[0].vertices.size()==3U);
    assert(scene.value().meshes[0].indices.size()==3U && scene.value().nodes.size()==1U);
    assert(scene.value().meshes[0].submeshes.size()==1U);
    assert(scene.value().meshes[0].submeshes[0].material_index==0U);
    assert(scene.value().materials.size()==1U);
    assert(scene.value().materials[0].base_color.x==0.8F);
    assert(scene.value().nodes[0].local.translation.x==1.0F);
    assert(scene.value().nodes[0].world.translation.z==3.0F);
    const auto presented=genomes::assets::present(scene.value());
    assert(presented.revision!=0U && !presented.bounds.empty && presented.materials.size()==1U);
    const auto glb=genomes::assets::importStaticGltf(
        std::filesystem::path(GENOMES_SOURCE_DIR) / "tests/fixtures/assets/static_triangle.glb");
    assert(glb && glb.value().meshes.size()==1U && glb.value().nodes.size()==1U);
    assert(glb.value().meshes[0].indices.size()==3U);
    const auto invalid=genomes::assets::importStaticGltf("does-not-exist.gltf");
    assert(!invalid);
    return 0;
}
