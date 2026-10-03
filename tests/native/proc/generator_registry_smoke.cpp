#include <genomes/proc/GeneratorRegistry.hpp>

#include <cassert>
#include <memory>

int main() {
    genomes::proc::GenerationContext context{genomes::proc::SeedPath(1234)};
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

    genomes::proc::GeneratorRegistry::Builder typed_builder;
    const genomes::proc::GeneratorDescriptor typed_descriptor{
        genomes::proc::generatorId("typed.terrain"),
        "typed.terrain",
        {1, 0, 0},
        input_type,
        output_type,
        true,
        genomes::proc::GeneratorExecutionPolicy::Cpu,
        genomes::proc::GeneratorCachePolicy::None};
    const auto typed_added = typed_builder.addTyped<int, int>(
        typed_descriptor,
        [](const int&, genomes::proc::GenerationContext&) {
            return genomes::foundation::Result<std::shared_ptr<const int>,
                                                genomes::foundation::Error>::success(
                std::make_shared<const int>(7));
        });
    assert(typed_added);
    const auto typed_registry_result = std::move(typed_builder).freeze();
    assert(typed_registry_result);
    assert(!typed_registry_result.value().run(typed_descriptor.id, context));
    return 0;
}
