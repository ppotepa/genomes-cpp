#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::navigation {

enum class PathStatus : std::uint8_t {
    Complete,
    NoPath,
    NodeBudgetExceeded,
};

struct PathRequest final {
    foundation::Vec3 start{};
    foundation::Vec3 goal{};
    std::uint32_t max_nodes{16'384};

    [[nodiscard]] bool valid() const noexcept;
};

struct PathResult final {
    PathStatus status{PathStatus::NoPath};
    std::vector<foundation::Vec3> points;
    std::uint32_t expanded_nodes{0};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == PathStatus::Complete && !points.empty();
    }
};

class NavigationWorld {
public:
    virtual ~NavigationWorld() = default;

    [[nodiscard]] virtual foundation::Result<PathResult, foundation::Error> findPath(
        const PathRequest&) const = 0;
};

struct NavGridSpec final {
    std::uint32_t width{1};
    std::uint32_t height{1};
    float cell_size{1.0F};
    foundation::Vec3 origin{};

    [[nodiscard]] bool valid() const noexcept;
};

// Deterministic CPU grid backend. It is deliberately independent of Recast;
// the same NavigationWorld contract can later be implemented by tiled navmesh
// backends without changing AI or movement systems.
class GridNavigationWorld final : public NavigationWorld {
public:
    explicit GridNavigationWorld(NavGridSpec spec);

    [[nodiscard]] const NavGridSpec& spec() const noexcept { return spec_; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    void bindWorldRevision(std::uint64_t revision) noexcept { world_revision_ = revision; }
    [[nodiscard]] std::uint64_t worldRevision() const noexcept { return world_revision_; }

    [[nodiscard]] bool setBlocked(std::uint32_t x, std::uint32_t z, bool blocked) noexcept;
    [[nodiscard]] bool isBlocked(std::uint32_t x, std::uint32_t z) const noexcept;

    [[nodiscard]] foundation::Result<PathResult, foundation::Error> findPath(
        const PathRequest&) const override;

private:
    [[nodiscard]] bool toCell(foundation::Vec3 position,
                               std::uint32_t& x,
                               std::uint32_t& z) const noexcept;
    [[nodiscard]] foundation::Vec3 toWorld(std::uint32_t x, std::uint32_t z) const noexcept;
    [[nodiscard]] std::uint32_t index(std::uint32_t x, std::uint32_t z) const noexcept {
        return z * spec_.width + x;
    }

    NavGridSpec spec_{};
    bool valid_{false};
    std::uint64_t world_revision_{0U};
    std::vector<std::uint8_t> blocked_;
    std::size_t blocked_count_{0};
};

} // namespace genomes::navigation
