#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/runtime/InfantryPresentation.hpp>

#include <cassert>

int main() {
    genomes::infantry::InfantryModelCompiler compiler;
    genomes::infantry::InfantryModelRequest request{};
    request.seed = 8841U;

    const auto model = compiler.compile(request);
    assert(model);
    const auto first = genomes::runtime::infantry_presentation::makePrototype(model.value());
    const auto second = genomes::runtime::infantry_presentation::makePrototype(model.value());
    assert(first);
    assert(first == second);
    assert(first->revision == model.value().cache_key);

    auto changed = request;
    changed.wear = 0.5;
    const auto changed_model = compiler.compile(changed);
    assert(changed_model);
    const auto changed_prototype =
        genomes::runtime::infantry_presentation::makePrototype(changed_model.value());
    assert(changed_prototype);
    assert(changed_prototype != first);
    assert(changed_prototype->mesh_id != first->mesh_id);
    return 0;
}
