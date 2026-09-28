#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/graph/GraphResource.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::render::graph {

using PassId = std::uint32_t;
constexpr PassId InvalidPass = 0xFFFF'FFFFu;

struct GraphPassContext final {
    PassId pass{InvalidPass};
};

using GraphPassCallback = std::function<void(const GraphPassContext&)>;

struct GraphPassDesc final {
    std::string name;
    std::vector<GraphResourceUse> uses;
    std::vector<PassId> before;
    std::vector<PassId> after;
    bool side_effect{false};
    GraphPassCallback callback;
};

struct CompiledResource final {
    GraphResourceHandle handle{};
    GraphResourceDesc desc{};
    std::size_t first_use{static_cast<std::size_t>(-1)};
    std::size_t last_use{static_cast<std::size_t>(-1)};
    std::size_t physical_slot{static_cast<std::size_t>(-1)};
};

struct CompiledGraph final {
    std::vector<PassId> order;
    std::vector<CompiledResource> resources;
    std::vector<GraphBarrier> barriers;
    std::string dot;

    [[nodiscard]] bool empty() const noexcept { return order.empty(); }
};

class RenderGraph final {
public:
    [[nodiscard]] foundation::Result<GraphTexture, foundation::Error> createTexture(
        GraphResourceDesc desc);
    [[nodiscard]] foundation::Result<GraphBuffer, foundation::Error> createBuffer(
        GraphResourceDesc desc);
    [[nodiscard]] foundation::Result<void, foundation::Error> exportResource(
        GraphResourceHandle resource);
    [[nodiscard]] foundation::Result<PassId, foundation::Error> addPass(GraphPassDesc desc);
    [[nodiscard]] foundation::Result<CompiledGraph, foundation::Error> compile() const;
    [[nodiscard]] foundation::Result<void, foundation::Error> execute(
        const CompiledGraph& compiled) const;

    void clear() noexcept;

    [[nodiscard]] std::size_t resourceCount() const noexcept { return resources_.size(); }
    [[nodiscard]] std::size_t passCount() const noexcept { return passes_.size(); }

private:
    struct ResourceNode final {
        GraphResourceDesc desc{};
        std::uint32_t generation{1};
    };

    struct PassNode final {
        GraphPassDesc desc;
    };

    [[nodiscard]] bool validResource(GraphResourceHandle resource) const noexcept;
    [[nodiscard]] foundation::Error error(const char* message) const noexcept;

    std::vector<ResourceNode> resources_;
    std::vector<PassNode> passes_;
};

} // namespace genomes::render::graph
