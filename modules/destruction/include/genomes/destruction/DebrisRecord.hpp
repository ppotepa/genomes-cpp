#pragma once

#include <genomes/destruction/Material.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::destruction {

enum class DebrisRepresentation : std::uint8_t {
    SourceAttached,
    Hero,
    Cheap,
    Baked,
    RemovedByDamage,
};

struct MaterialVolume final {
    MaterialId material{};
    double volume{0.0};
};

struct DebrisRecord final {
    foundation::StableId id{0};
    foundation::StableId source_component_id{0};
    std::vector<MaterialVolume> material_volumes;
    foundation::Vec3 position{};
    foundation::Vec3 linear_velocity{};
    foundation::Vec3 angular_velocity{};
    DebrisRepresentation representation{DebrisRepresentation::Cheap};
    std::uint64_t age_ticks{0};
    float importance{0.0F};
    bool awake{true};
    bool shape_representable{true};

    [[nodiscard]] double totalVolume() const noexcept {
        double total = 0.0;
        for (const MaterialVolume& volume : material_volumes) {
            total += volume.volume;
        }
        return total;
    }
};

} // namespace genomes::destruction
