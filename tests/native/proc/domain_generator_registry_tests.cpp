#include <genomes/gameplay/ProductionGenerators.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    auto registry = gameplay::makeProductionGeneratorRegistry();
    assert(registry);
    const auto& frozen = registry.value();
    for (const char* id : {"buildings.plan", "hydrology.artifact",
                           "terrain.height-field", "world.plan"}) {
        const auto* entry = frozen.find(proc::generatorId(id));
        assert(entry != nullptr);
        assert(entry->generate_typed);
        assert(entry->descriptor.cache == proc::GeneratorCachePolicy::Artifact);
    }
#if GENOMES_HAS_INFANTRY
    for (const char* id : {"infantry.model", "weapons.artifact"}) {
        const auto* entry = frozen.find(proc::generatorId(id));
        assert(entry != nullptr);
        assert(entry->generate_typed);
        assert(entry->descriptor.cache == proc::GeneratorCachePolicy::Artifact);
    }
#endif
    return 0;
}
