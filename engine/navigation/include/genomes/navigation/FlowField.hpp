#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace genomes::navigation {

struct FlowFieldKey final {
    std::uint64_t navigation_revision{0U};
    foundation::StableId cost_profile{0U};
    foundation::StableId goal_set_hash{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};

    [[nodiscard]] bool operator==(const FlowFieldKey&) const noexcept = default;
};

struct FlowFieldKeyHash final {
    [[nodiscard]] std::size_t operator()(const FlowFieldKey& key) const noexcept {
        auto hash = foundation::stableHashU64(key.navigation_revision);
        hash = foundation::stableHashCombine(hash, key.cost_profile);
        hash = foundation::stableHashCombine(hash, key.goal_set_hash);
        hash = foundation::stableHashCombine(hash, key.width);
        hash = foundation::stableHashCombine(hash, key.height);
        return static_cast<std::size_t>(hash);
    }
};

struct FlowFieldDescriptor final {
    FlowFieldKey key{};
    std::vector<std::uint32_t> goal_cells;

    [[nodiscard]] bool valid() const noexcept;
};

struct FlowDirection final {
    std::int8_t x{0};
    std::int8_t z{0};

    [[nodiscard]] bool isZero() const noexcept { return x == 0 && z == 0; }
    [[nodiscard]] bool operator==(const FlowDirection&) const noexcept = default;
};

struct FlowSample final {
    std::uint32_t integration_cost{0U};
    FlowDirection direction{};
    bool reachable{false};
    bool goal{false};
};

class FlowField final {
public:
    FlowField() = default;

    [[nodiscard]] const FlowFieldDescriptor& descriptor() const noexcept { return descriptor_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return descriptor_.key.width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return descriptor_.key.height; }
    [[nodiscard]] bool validFor(std::uint64_t navigation_revision) const noexcept {
        return descriptor_.valid() && descriptor_.key.navigation_revision == navigation_revision;
    }

    [[nodiscard]] FlowSample sample(std::uint32_t x, std::uint32_t z) const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> integrationCosts() const noexcept {
        return integration_costs_;
    }
    [[nodiscard]] std::span<const FlowDirection> directions() const noexcept {
        return directions_;
    }

private:
    friend class FlowFieldBuilder;
    FlowFieldDescriptor descriptor_{};
    std::vector<std::uint8_t> blocked_;
    std::vector<std::uint32_t> integration_costs_;
    std::vector<FlowDirection> directions_;
    std::vector<std::uint8_t> goals_;
};

class FlowFieldBuilder final {
public:
    [[nodiscard]] static foundation::Result<FlowField, foundation::Error> build(
        const FlowFieldDescriptor& descriptor,
        std::span<const std::uint8_t> blocked,
        std::span<const std::uint32_t> traversal_cost = {});
};

// Cache is intentionally keyed by the complete semantic field key. A changed
// navigation revision cannot accidentally reuse an older field.
class FlowFieldCache final {
public:
    [[nodiscard]] bool insert(FlowField field);
    [[nodiscard]] const FlowField* find(const FlowFieldKey& key) const noexcept;
    [[nodiscard]] std::size_t invalidateNavigationRevision(
        std::uint64_t navigation_revision) noexcept;
    void clear() noexcept { fields_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return fields_.size(); }

private:
    std::unordered_map<FlowFieldKey, FlowField, FlowFieldKeyHash> fields_;
};

} // namespace genomes::navigation
