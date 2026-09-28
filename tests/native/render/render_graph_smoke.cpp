#include <genomes/render/graph/RenderGraph.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

int main() {
    using namespace genomes;
    using namespace render::graph;

    RenderGraph graph;
    const auto output = graph.createTexture({GraphResourceKind::Texture, 1280, 720, 1, 0,
                                             true, false});
    const auto transient_a = graph.createBuffer({GraphResourceKind::Buffer, 0, 0, 0, 4096,
                                                  false, false});
    const auto transient_b = graph.createBuffer({GraphResourceKind::Buffer, 0, 0, 0, 4096,
                                                  false, false});
    assert(output && transient_a && transient_b);
    assert(graph.exportResource(output.value()));

    std::vector<PassId> executed;
    assert(graph.addPass({"prepare-a",
                          {{transient_a.value(), GraphAccess::Write}},
                          {},
                          {},
                          true,
                          [&executed](const GraphPassContext& context) {
                              executed.push_back(context.pass);
                          }}));
    assert(graph.addPass({"consume-a",
                          {{transient_a.value(), GraphAccess::Read},
                           {output.value(), GraphAccess::ColorAttachment}},
                          {},
                          {},
                          false,
                          [&executed](const GraphPassContext& context) {
                              executed.push_back(context.pass);
                          }}));
    assert(graph.addPass({"prepare-b",
                          {{transient_b.value(), GraphAccess::Write}},
                          {},
                          {},
                          true,
                          [&executed](const GraphPassContext& context) {
                              executed.push_back(context.pass);
                          }}));
    assert(graph.addPass({"culled", {}, {}, {}, false,
                          [&executed](const GraphPassContext& context) {
                              executed.push_back(context.pass);
                          }}));

    const auto compiled = graph.compile();
    assert(compiled);
    assert(compiled.value().order.size() == 3);
    assert(std::find(compiled.value().order.begin(), compiled.value().order.end(), 3) ==
           compiled.value().order.end());
    assert(!compiled.value().barriers.empty());
    assert(compiled.value().resources[transient_a.value().index].physical_slot ==
           compiled.value().resources[transient_b.value().index].physical_slot);
    assert(compiled.value().dot.find("prepare-a") != std::string::npos);
    assert(graph.execute(compiled.value()));
    assert(executed == compiled.value().order);

    RenderGraph cyclic;
    assert(cyclic.addPass({"cycle-a", {}, {1}, {}, true,
                           [](const GraphPassContext&) {}}));
    assert(cyclic.addPass({"cycle-b", {}, {0}, {}, true,
                           [](const GraphPassContext&) {}}));
    assert(!cyclic.compile());
    return 0;
}
