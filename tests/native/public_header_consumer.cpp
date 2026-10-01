#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/content/ConfigurationSnapshot.hpp>
#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>

int main() {
    const genomes::content::ContentReadLimits limits{};
    const genomes::content::ConfigurationSchema schema{};
    const genomes::gameplay::WorldScenarioStatus scenario{};
    const genomes::navigation::NavGridSpec navigation{};
    const genomes::physics::BodyDesc body{};
    const genomes::world::GridLayout grid{};
    const genomes::world_render::WorldMeshArtifact mesh{};
    (void)limits;
    (void)schema;
    (void)scenario;
    (void)navigation;
    (void)body;
    (void)grid;
    (void)mesh;
    return 0;
}
