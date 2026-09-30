#include <genomes/assets/GltfImporter.hpp>
#include <cassert>

int main() {
    const auto scene=genomes::assets::importStaticGltf(
        std::filesystem::path(GENOMES_SOURCE_DIR) / "tests/fixtures/assets/static_triangle.gltf");
    assert(scene);
    assert(scene.value().meshes.size()==1U && scene.value().meshes[0].vertices.size()==3U);
    assert(scene.value().meshes[0].indices.size()==3U && scene.value().nodes.size()==1U);
    const auto invalid=genomes::assets::importStaticGltf("does-not-exist.gltf");
    assert(!invalid);
    return 0;
}
