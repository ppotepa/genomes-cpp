#include "TestSupport.hpp"

#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>

#include <future>
#include <iostream>

int main() {
    using namespace genomes;
    using upgrade_test::check;
    using namespace game_scenes::infantry_presentation;
    try {
        infantry::InfantryModelCompiler compiler;
        infantry::InfantryModelRequest request;
        request.seed = 8841U;
        request.detail_level = infantry::InfantryDetail::High;
        const auto generated = compiler.compile(request);
        check(generated, "infantry generation failed");
        const auto& model = *generated.value().artifact;
        const auto source_indices = model.appearance.body.indices;
        const auto source_tags = model.appearance.body.tags;
        const auto reference = makePrototype(model, PrototypePreparation::ReferenceOrder);
        const auto prepared = makePrototype(model);
        check(reference && prepared, "prototype is null");
        check(makePrototype(model) == prepared, "warm cache lost prototype identity");
        check(makePrototype(model, PrototypePreparation::ReferenceOrder) == reference,
              "reference cache lost prototype identity");
        check(prepared->revision == model.cache_key, "domain revision was replaced");
        check((prepared->mesh_id != reference->mesh_id) == geometry::indexOptimizerAvailable(),
              "raw and prepared cache identities do not reflect the enabled policy");
        upgrade_test::checkVertexStreams(*reference, *prepared);
        upgrade_test::checkGroups(*reference, *prepared);
        check(model.appearance.body.indices == source_indices, "raw domain indices changed");
        check(model.appearance.body.tags.size() == source_tags.size(), "domain tags changed");
        for (std::size_t i = 0; i < source_tags.size(); ++i) {
            check(source_tags[i].name == model.appearance.body.tags[i].name &&
                  source_tags[i].vertices == model.appearance.body.tags[i].vertices,
                  "semantic tag vertex IDs changed");
        }
        check(reference->skeleton && prepared->skeleton, "missing neutral skeleton");
        check(reference->skeleton->skeleton_id == prepared->skeleton->skeleton_id,
              "skeleton identity changed");
        for (std::size_t i = 0; i < reference->skeleton->bones.size(); ++i) {
            check(reference->skeleton->bones[i].inverse_bind ==
                  prepared->skeleton->bones[i].inverse_bind, "inverse bind changed");
        }
        // Race two cache misses for a fresh key; each future retains its result.
        auto next_request = request;
        next_request.seed = 8842U;
        const auto next = compiler.compile(next_request);
        check(next, "second infantry generation failed");
        auto a = std::async(std::launch::async, [&] { return makePrototype(*next.value().artifact); });
        auto b = std::async(std::launch::async, [&] { return makePrototype(*next.value().artifact); });
        const auto first = a.get();
        const auto second = b.get();
        check(first == second, "concurrent equivalent candidates were published twice");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
