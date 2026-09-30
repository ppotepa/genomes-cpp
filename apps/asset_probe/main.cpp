#include <genomes/assets/GltfImporter.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: genomes_asset_probe <file.gltf|file.glb>\n"; return 2; }
    const auto scene=genomes::assets::importStaticGltf(argv[1]);
    if (!scene) { std::cerr << "asset import failed: " << scene.error().message << "\n"; return 1; }
    std::size_t vertices=0,indices=0;
    for (const auto& mesh:scene.value().meshes) { vertices+=mesh.vertices.size(); indices+=mesh.indices.size(); }
    std::cout << "meshes=" << scene.value().meshes.size() << " nodes=" << scene.value().nodes.size()
              << " materials=" << scene.value().materials.size() << " vertices=" << vertices
              << " indices=" << indices << '\n';
    return 0;
}
