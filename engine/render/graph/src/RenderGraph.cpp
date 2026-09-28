#include <genomes/render/graph/RenderGraph.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

namespace genomes::render::graph {

namespace {

[[nodiscard]] bool hasEdge(const std::vector<PassId>& successors, PassId target) noexcept {
    return std::find(successors.begin(), successors.end(), target) != successors.end();
}

} // namespace

foundation::Error RenderGraph::error(const char* message) const noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

bool RenderGraph::validResource(GraphResourceHandle resource) const noexcept {
    return resource.isValid() && resource.index < resources_.size() &&
           resources_[resource.index].generation == resource.generation &&
           resources_[resource.index].desc.kind == resource.kind;
}

foundation::Result<GraphTexture, foundation::Error> RenderGraph::createTexture(
    GraphResourceDesc desc) {
    desc.kind = GraphResourceKind::Texture;
    if (!desc.valid()) {
        return foundation::Result<GraphTexture, foundation::Error>::failure(
            error("invalid render graph texture description"));
    }
    if (resources_.size() >= 0xFFFF'FFFFu) {
        return foundation::Result<GraphTexture, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "render graph resource capacity exhausted"});
    }
    const auto index = static_cast<std::uint32_t>(resources_.size());
    resources_.push_back({desc, 1});
    return foundation::Result<GraphTexture, foundation::Error>::success(
        {index, resources_.back().generation, GraphResourceKind::Texture});
}

foundation::Result<GraphBuffer, foundation::Error> RenderGraph::createBuffer(
    GraphResourceDesc desc) {
    desc.kind = GraphResourceKind::Buffer;
    if (!desc.valid()) {
        return foundation::Result<GraphBuffer, foundation::Error>::failure(
            error("invalid render graph buffer description"));
    }
    if (resources_.size() >= 0xFFFF'FFFFu) {
        return foundation::Result<GraphBuffer, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "render graph resource capacity exhausted"});
    }
    const auto index = static_cast<std::uint32_t>(resources_.size());
    resources_.push_back({desc, 1});
    return foundation::Result<GraphBuffer, foundation::Error>::success(
        {index, resources_.back().generation, GraphResourceKind::Buffer});
}

foundation::Result<void, foundation::Error> RenderGraph::exportResource(
    GraphResourceHandle resource) {
    if (!validResource(resource)) {
        return foundation::Result<void, foundation::Error>::failure(
            error("render graph export references an invalid resource"));
    }
    resources_[resource.index].desc.exported = true;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<PassId, foundation::Error> RenderGraph::addPass(GraphPassDesc desc) {
    if (desc.name.empty() || !desc.callback) {
        return foundation::Result<PassId, foundation::Error>::failure(
            error("render graph pass requires a name and callback"));
    }
    if (passes_.size() >= InvalidPass) {
        return foundation::Result<PassId, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "render graph pass capacity exhausted"});
    }
    std::vector<GraphResourceHandle> referenced;
    referenced.reserve(desc.uses.size());
    for (const GraphResourceUse& use : desc.uses) {
        if (!validResource(use.resource)) {
            return foundation::Result<PassId, foundation::Error>::failure(
                error("render graph pass references an invalid resource"));
        }
        if (std::find(referenced.begin(), referenced.end(), use.resource) != referenced.end()) {
            return foundation::Result<PassId, foundation::Error>::failure(
                error("render graph pass declares one resource more than once"));
        }
        referenced.push_back(use.resource);
    }
    const auto id = static_cast<PassId>(passes_.size());
    passes_.push_back({std::move(desc)});
    return foundation::Result<PassId, foundation::Error>::success(id);
}

foundation::Result<CompiledGraph, foundation::Error> RenderGraph::compile() const {
    const std::size_t pass_count = passes_.size();
    std::vector<std::vector<PassId>> successors(pass_count);
    std::vector<std::vector<PassId>> predecessors(pass_count);
    std::vector<std::size_t> indegree(pass_count, 0);

    const auto add_edge = [&](PassId from, PassId to) -> bool {
        if (from == to || from >= pass_count || to >= pass_count) {
            return false;
        }
        if (hasEdge(successors[from], to)) {
            return true;
        }
        successors[from].push_back(to);
        predecessors[to].push_back(from);
        ++indegree[to];
        return true;
    };

    for (PassId pass = 0; pass < pass_count; ++pass) {
        for (const PassId target : passes_[pass].desc.before) {
            if (target >= pass_count || !add_edge(pass, target)) {
                return foundation::Result<CompiledGraph, foundation::Error>::failure(
                    error("render graph before dependency references an invalid pass"));
            }
        }
        for (const PassId source : passes_[pass].desc.after) {
            if (source >= pass_count || !add_edge(source, pass)) {
                return foundation::Result<CompiledGraph, foundation::Error>::failure(
                    error("render graph after dependency references an invalid pass"));
            }
        }
    }

    struct ResourceHazard final {
        std::optional<PassId> last_writer;
        std::vector<PassId> readers;
    };
    std::vector<ResourceHazard> hazards(resources_.size());

    for (PassId pass = 0; pass < pass_count; ++pass) {
        for (const GraphResourceUse& use : passes_[pass].desc.uses) {
            ResourceHazard& hazard = hazards[use.resource.index];
            if (use.writes()) {
                if (hazard.last_writer.has_value() &&
                    !add_edge(*hazard.last_writer, pass)) {
                    return foundation::Result<CompiledGraph, foundation::Error>::failure(
                        error("render graph contains a self-dependency"));
                }
                for (const PassId reader : hazard.readers) {
                    if (!add_edge(reader, pass)) {
                        return foundation::Result<CompiledGraph, foundation::Error>::failure(
                            error("render graph contains a self-dependency"));
                    }
                }
                hazard.readers.clear();
                hazard.last_writer = pass;
            } else if (use.reads()) {
                if (hazard.last_writer.has_value() &&
                    !add_edge(*hazard.last_writer, pass)) {
                    return foundation::Result<CompiledGraph, foundation::Error>::failure(
                        error("render graph contains a self-dependency"));
                }
                hazard.readers.push_back(pass);
            }
        }
    }

    std::vector<bool> needed(pass_count, false);
    std::vector<PassId> work;
    for (PassId pass = 0; pass < pass_count; ++pass) {
        if (passes_[pass].desc.side_effect) {
            needed[pass] = true;
            work.push_back(pass);
            continue;
        }
        for (const GraphResourceUse& use : passes_[pass].desc.uses) {
            if (use.writes() && (resources_[use.resource.index].desc.exported ||
                                 resources_[use.resource.index].desc.imported)) {
                needed[pass] = true;
                work.push_back(pass);
                break;
            }
        }
    }
    while (!work.empty()) {
        const PassId pass = work.back();
        work.pop_back();
        for (const PassId predecessor : predecessors[pass]) {
            if (!needed[predecessor]) {
                needed[predecessor] = true;
                work.push_back(predecessor);
            }
        }
    }

    CompiledGraph compiled{};
    compiled.resources.reserve(resources_.size());
    for (std::size_t index = 0; index < resources_.size(); ++index) {
        compiled.resources.push_back({
            {static_cast<std::uint32_t>(index), resources_[index].generation,
             resources_[index].desc.kind},
            resources_[index].desc});
    }

    std::vector<PassId> ready;
    ready.reserve(pass_count);
    for (PassId pass = 0; pass < pass_count; ++pass) {
        if (indegree[pass] == 0) {
            ready.push_back(pass);
        }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        std::sort(ready.begin(), ready.end(), std::greater<PassId>{});
        const PassId pass = ready.back();
        ready.pop_back();
        ++visited;
        if (needed[pass]) {
            compiled.order.push_back(pass);
        }
        for (const PassId successor : successors[pass]) {
            if (--indegree[successor] == 0) {
                ready.push_back(successor);
            }
        }
    }
    if (visited != pass_count) {
        return foundation::Result<CompiledGraph, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "render graph dependency cycle detected"});
    }

    std::vector<std::vector<GraphResourceUse>> ordered_uses(pass_count);
    for (std::size_t order = 0; order < compiled.order.size(); ++order) {
        const PassId pass = compiled.order[order];
        ordered_uses[pass] = passes_[pass].desc.uses;
        for (const GraphResourceUse& use : ordered_uses[pass]) {
            auto& resource = compiled.resources[use.resource.index];
            if (resource.first_use == static_cast<std::size_t>(-1)) {
                resource.first_use = order;
                resource.last_use = order;
            } else {
                resource.last_use = std::max(resource.last_use, order);
            }
        }
    }

    struct PhysicalSlot final {
        GraphResourceDesc desc{};
        std::size_t last_use{static_cast<std::size_t>(-1)};
    };
    std::vector<std::size_t> resource_order;
    for (std::size_t index = 0; index < compiled.resources.size(); ++index) {
        if (compiled.resources[index].first_use != static_cast<std::size_t>(-1)) {
            resource_order.push_back(index);
        }
    }
    std::sort(resource_order.begin(), resource_order.end(), [&](std::size_t left,
                                                                std::size_t right) {
        return compiled.resources[left].first_use < compiled.resources[right].first_use;
    });
    std::vector<PhysicalSlot> physical_slots;
    for (const std::size_t resource_index : resource_order) {
        CompiledResource& resource = compiled.resources[resource_index];
        if (resource.desc.imported) {
            resource.physical_slot = physical_slots.size();
            physical_slots.push_back({resource.desc, resource.last_use});
            continue;
        }
        for (std::size_t slot = 0; slot < physical_slots.size(); ++slot) {
            if (physical_slots[slot].last_use < resource.first_use &&
                physical_slots[slot].desc.compatibleTransient(resource.desc)) {
                resource.physical_slot = slot;
                physical_slots[slot].last_use = resource.last_use;
                break;
            }
        }
        if (resource.physical_slot == static_cast<std::size_t>(-1)) {
            resource.physical_slot = physical_slots.size();
            physical_slots.push_back({resource.desc, resource.last_use});
        }
    }

    std::vector<std::optional<GraphAccess>> last_access(resources_.size());
    std::vector<PassId> last_pass(resources_.size(), InvalidPass);
    for (const PassId pass : compiled.order) {
        for (const GraphResourceUse& use : passes_[pass].desc.uses) {
            if (last_access[use.resource.index].has_value() &&
                *last_access[use.resource.index] != use.access) {
                compiled.barriers.push_back({use.resource, *last_access[use.resource.index],
                                              use.access, last_pass[use.resource.index], pass});
            }
            last_access[use.resource.index] = use.access;
            last_pass[use.resource.index] = pass;
        }
    }

    std::ostringstream dot;
    dot << "digraph RenderGraph {\n";
    for (const PassId pass : compiled.order) {
        dot << "  p" << pass << " [label=\"" << passes_[pass].desc.name << "\"];\n";
        for (const PassId successor : successors[pass]) {
            if (needed[successor]) {
                dot << "  p" << pass << " -> p" << successor << ";\n";
            }
        }
    }
    dot << "}\n";
    compiled.dot = dot.str();
    return foundation::Result<CompiledGraph, foundation::Error>::success(std::move(compiled));
}

void RenderGraph::clear() noexcept {
    resources_.clear();
    passes_.clear();
}

foundation::Result<void, foundation::Error> RenderGraph::execute(
    const CompiledGraph& compiled) const {
    for (const PassId pass : compiled.order) {
        if (pass >= passes_.size() || !passes_[pass].desc.callback) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "compiled render graph references an invalid pass"});
        }
        try {
            passes_[pass].desc.callback(GraphPassContext{pass});
        } catch (...) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "render graph pass callback failed"});
        }
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::render::graph
