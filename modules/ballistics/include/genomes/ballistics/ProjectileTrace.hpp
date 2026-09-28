#pragma once

#include <genomes/ballistics/ProjectileState.hpp>
#include <genomes/world/WorldQuerySnapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::ballistics {

enum class TerminalReason : std::uint8_t {
    GroundContact,
    TrackingLimit,
    Stopped,
    Detonated,
    Removed,
    Capacity,
};

struct TraceSegment final {
    foundation::StableId projectile_id{0};
    TraceId trace_id{};
    foundation::Vec3 from{};
    foundation::Vec3 to{};
    float energy_from{0.0F};
    float energy_to{0.0F};
    std::uint32_t impact_index{0};
};

struct TraceContact final {
    foundation::StableId projectile_id{0};
    TraceId trace_id{};
    foundation::StableId semantic_id{0};
    world::QuerySourceKind source{world::QuerySourceKind::Static};
    foundation::Vec3 point{};
    foundation::Vec3 normal{};
    float distance{0.0F};
    std::uint32_t impact_index{0};
};

struct TraceTerminal final {
    foundation::StableId projectile_id{0};
    TraceId trace_id{};
    TerminalReason reason{TerminalReason::Stopped};
    foundation::Vec3 position{};
    std::uint64_t age_ticks{0};
};

class ProjectileTraceObserver {
public:
    virtual ~ProjectileTraceObserver() = default;
    virtual void onSegment(const TraceSegment&) noexcept = 0;
    virtual void onContact(const TraceContact&) noexcept = 0;
    virtual void onTerminal(const TraceTerminal&) noexcept = 0;
};

class ProjectileTrace final : public ProjectileTraceObserver {
public:
    explicit ProjectileTrace(std::size_t maximum_segments = 4096,
                             std::size_t maximum_contacts = 1024,
                             std::size_t maximum_terminals = 512) noexcept
        : maximum_segments_{maximum_segments},
          maximum_contacts_{maximum_contacts},
          maximum_terminals_{maximum_terminals} {}

    void onSegment(const TraceSegment&) noexcept override;
    void onContact(const TraceContact&) noexcept override;
    void onTerminal(const TraceTerminal&) noexcept override;

    [[nodiscard]] const std::vector<TraceSegment>& segments() const noexcept { return segments_; }
    [[nodiscard]] const std::vector<TraceContact>& contacts() const noexcept { return contacts_; }
    [[nodiscard]] const std::vector<TraceTerminal>& terminals() const noexcept {
        return terminals_;
    }
    void clear() noexcept;

private:
    std::size_t maximum_segments_{0};
    std::size_t maximum_contacts_{0};
    std::size_t maximum_terminals_{0};
    std::vector<TraceSegment> segments_;
    std::vector<TraceContact> contacts_;
    std::vector<TraceTerminal> terminals_;
};

} // namespace genomes::ballistics
