#include <genomes/destruction/StructuralGraph.hpp>

#include <cassert>
#include <utility>
#include <vector>

int main() {
    using namespace genomes::destruction;
    const auto created = StructuralGraph::create(
        {{1U, 10U, 1.0F, true}, {2U, 20U, 1.0F, false}, {3U, 30U, 1.0F, false}},
        {{11U, 1U, 2U, 1.0F}, {12U, 2U, 3U, 1.0F}});
    assert(created);
    StructuralGraph graph = std::move(created.value());
    assert(graph.evaluateSupport().empty());
    assert(graph.nodeStates()[0].supported);
    assert(graph.nodeStates()[2].supported);

    assert(graph.applyEdgeDamage(11U, 1.0F));
    const auto detached = graph.evaluateSupport();
    assert(detached.size() == 2U);
    assert(detached[0].component_id == 20U);
    assert(detached[1].component_id == 30U);
    assert(graph.nodeStates()[1].detached);
    assert(graph.nodeStates()[2].detached);
    assert(graph.evaluateSupport().empty());

    const auto anchored = StructuralGraph::create(
        {{1U, 10U, 1.0F, true}, {2U, 20U, 1.0F, true}, {3U, 30U, 1.0F, false}},
        {{11U, 1U, 3U, 1.0F}, {12U, 2U, 3U, 1.0F}});
    assert(anchored);
    StructuralGraph multi_anchor = std::move(anchored.value());
    assert(multi_anchor.evaluateSupport().empty());
    assert(multi_anchor.applyEdgeDamage(11U, 1.0F));
    assert(multi_anchor.evaluateSupport().empty());
    assert(multi_anchor.nodeStates()[2].supported);

    const auto invalid = StructuralGraph::create(
        {{1U, 10U, 1.0F, true}}, {{11U, 1U, 99U, 1.0F}});
    assert(!invalid);
    return 0;
}
