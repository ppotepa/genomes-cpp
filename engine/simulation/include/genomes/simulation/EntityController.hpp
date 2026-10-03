#ifndef GENOMES_SIMULATION_ENTITYCONTROLLER_HPP
#define GENOMES_SIMULATION_ENTITYCONTROLLER_HPP

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>

#include <cstdint>
#include <bit>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace genomes::simulation {

// Orders are entity-neutral. AI, player input and scripted behaviour can all
// submit the same value; a controller resolves it for its entity type.
enum class EntityOrderKind : std::uint8_t {
    None,
    Hold,
    MoveTo,
    Engage,
    Retreat,
    Interact,
};

struct EntityOrder final {
    EntityId entity{};
    EntityOrderKind kind{EntityOrderKind::None};
    EntityId target{};
    foundation::Vec3 destination{};
    foundation::StableId action{0U};
    foundation::StableId source{0U};
    std::uint64_t issued_tick{0U};
    std::uint64_t expires_tick{0U};
    std::uint8_t priority{0U};

    [[nodiscard]] bool activeAt(EntityId subject, std::uint64_t tick) const noexcept {
        return entity == subject && kind != EntityOrderKind::None &&
               issued_tick <= tick && expires_tick >= tick;
    }
};

// Stable payload codec used by CommandEnvelope. It intentionally does not
// serialize sizeof(EntityOrder), enum padding or compiler ABI layout.
[[nodiscard]] inline std::vector<std::uint8_t> encodeEntityOrder(
    const EntityOrder& order) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(62U);
    const auto appendU8 = [&bytes](std::uint8_t value) { bytes.push_back(value); };
    const auto appendU64 = [&bytes](std::uint64_t value) {
        for (std::size_t index = 0U; index < sizeof(value); ++index)
            bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    };
    const auto appendF32 = [&bytes](float value) {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (std::size_t index = 0U; index < sizeof(bits); ++index)
            bytes.push_back(static_cast<std::uint8_t>(bits >> (index * 8U)));
    };
    appendU64(order.entity.packed());
    appendU8(static_cast<std::uint8_t>(order.kind));
    appendU64(order.target.packed());
    appendF32(order.destination.x); appendF32(order.destination.y);
    appendF32(order.destination.z);
    appendU64(order.action); appendU64(order.source);
    appendU64(order.issued_tick); appendU64(order.expires_tick);
    appendU8(order.priority);
    return bytes;
}

[[nodiscard]] inline std::optional<EntityOrder> decodeEntityOrder(
    std::span<const std::uint8_t> bytes) noexcept {
    constexpr std::size_t size = 62U;
    if (bytes.size() != size) return std::nullopt;
    std::size_t cursor{0U};
    const auto readU8 = [&]() { return bytes[cursor++]; };
    const auto readU64 = [&]() {
        std::uint64_t value{0U};
        for (std::size_t index = 0U; index < sizeof(value); ++index)
            value |= static_cast<std::uint64_t>(bytes[cursor++]) << (index * 8U);
        return value;
    };
    const auto readU32 = [&]() {
        std::uint32_t value{0U};
        for (std::size_t index = 0U; index < sizeof(value); ++index)
            value |= static_cast<std::uint32_t>(bytes[cursor++]) << (index * 8U);
        return value;
    };
    const auto readF32 = [&]() { return std::bit_cast<float>(readU32()); };
    const auto packedEntity = [](std::uint64_t value) noexcept {
        return EntityId{static_cast<std::uint32_t>(value & 0xFFFF'FFFFU),
                        static_cast<std::uint32_t>(value >> 32U)};
    };
    EntityOrder order{};
    order.entity = packedEntity(readU64());
    const auto kind = readU8();
    if (kind > static_cast<std::uint8_t>(EntityOrderKind::Interact)) return std::nullopt;
    order.kind = static_cast<EntityOrderKind>(kind);
    order.target = packedEntity(readU64());
    order.destination = {readF32(), readF32(), readF32()};
    order.action = readU64(); order.source = readU64();
    order.issued_tick = readU64(); order.expires_tick = readU64();
    order.priority = readU8();
    return order;
}

// Produces a deterministic winner for a single entity. Higher priority wins;
// ties use newest issue tick and then stable source ID.
[[nodiscard]] inline std::optional<EntityOrder> resolveEntityOrder(
    EntityId entity, std::span<const EntityOrder> orders,
    std::uint64_t tick) noexcept {
    std::optional<EntityOrder> selected;
    for (const EntityOrder& order : orders) {
        if (!order.activeAt(entity, tick)) continue;
        if (!selected || order.priority > selected->priority ||
            (order.priority == selected->priority &&
             (order.issued_tick > selected->issued_tick ||
              (order.issued_tick == selected->issued_tick &&
               order.source < selected->source)))) {
            selected = order;
        }
    }
    return selected;
}

struct EntityControlRequest final {
    EntityReadView state{};
    EntityOrder order{};
    float requested_speed_mps{0.0F};
    float maximum_speed_mps{0.0F};
    float acceleration_mps2{0.0F};
    float turn_rate_radians_per_second{0.0F};
    bool enabled{true};
};

struct EntityControlCommand final {
    EntityId entity{};
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float heading_radians{0.0F};
    foundation::StableId action{0U};
};

// One controller instance processes a batch of entities of the same kind.
// Implementations own type-specific rules while callers retain storage and
// commit commands in the authoritative simulation phase.
class EntityController {
public:
    virtual ~EntityController() = default;

    [[nodiscard]] virtual foundation::StableId entityType() const noexcept = 0;

    [[nodiscard]] virtual foundation::Result<void, foundation::Error> updateBatch(
        std::span<const EntityControlRequest> requests,
        std::span<EntityControlCommand> commands,
        const TickContext& context) const noexcept = 0;
};

} // namespace genomes::simulation

#endif // GENOMES_SIMULATION_ENTITYCONTROLLER_HPP
