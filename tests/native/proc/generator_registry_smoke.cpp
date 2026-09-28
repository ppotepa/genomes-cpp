#include <genomes/proc/GeneratorRegistry.hpp>

#include <cassert>

int main() {
    genomes::jobs::JobSystem jobs(1);
    genomes::proc::GenerationContext context{genomes::proc::SeedPath(1234), jobs};
    const auto terrain_id = genomes::proc::generatorId("terrain");
    const auto input_type = genomes::foundation::stable_id("terrain-spec");
    const auto output_type = genomes::foundation::stable_id("height-field");
    const genomes::proc::GeneratorDescriptor descriptor{
        terrain_id,
        "terrain",
        {1, 0, 0},
        input_type,
        output_type,
        true,
        genomes::proc::GeneratorExecutionPolicy::Cpu,
        genomes::proc::GeneratorCachePolicy::Artifact};

    bool called = false;
    genomes::proc::GeneratorRegistry::Builder builder;
    assert(builder
               .add(descriptor, [&called](genomes::proc::GenerationContext& generation) {
                   called = generation.seed_path.seed() == 1234;
                   return genomes::foundation::Result<void, genomes::foundation::Error>::success();
               }));
    assert(!builder.add(descriptor, [](genomes::proc::GenerationContext&) {
        return genomes::foundation::Result<void, genomes::foundation::Error>::success();
    }));

    const auto registry_result = std::move(builder).freeze();
    assert(registry_result);
    const auto& registry = registry_result.value();
    assert(registry.size() == 1);
    assert(registry.find(terrain_id) != nullptr);
    assert(registry.run(terrain_id, context));
    assert(called);
    assert(!registry.run(genomes::proc::generatorId("missing"), context));
    return 0;
}
