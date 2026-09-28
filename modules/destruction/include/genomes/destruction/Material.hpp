#pragma once

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace genomes::destruction {

// These wrappers keep material and physical-solid identity distinct from
// renderer handles, entity IDs and ordinary hash values.
struct MaterialId final {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr MaterialId fromValue(std::uint64_t id) noexcept {
        return {id};
    }

    [[nodiscard]] static MaterialId fromName(std::string_view name) noexcept {
        return {foundation::stableHashString(name)};
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(MaterialId, MaterialId) noexcept = default;
};

struct PhysicalSolidId final {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr PhysicalSolidId fromValue(std::uint64_t id) noexcept {
        return {id};
    }

    [[nodiscard]] static PhysicalSolidId fromName(std::string_view name) noexcept {
        return {foundation::stableHashString(name)};
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(PhysicalSolidId, PhysicalSolidId) noexcept = default;
};

enum class MaterialResponse : std::uint8_t {
    Brittle,
    Ductile,
    Fibrous,
    Soft,
};

// Strength and penetration values are gameplay calibration coefficients. They
// are deliberately not presented as certified engineering data.
struct MaterialDefinition final {
    MaterialId id{};
    std::string_view name{};
    float density_kg_m3{0.0F};
    float strength_pa{0.0F};
    float penetration_work_j_m3{0.0F};
    float toughness_j_m2{0.0F};
    float ricochet_factor{0.0F};
    float spall_threshold_j{0.0F};
    MaterialResponse response{MaterialResponse::Brittle};
    std::string_view provenance{};

    [[nodiscard]] bool valid() const noexcept;
};

inline constexpr std::uint32_t MaterialCatalogVersion = 1;

class MaterialCatalog final {
public:
    [[nodiscard]] bool add(MaterialDefinition definition) noexcept;
    [[nodiscard]] bool freeze() noexcept;
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }

    [[nodiscard]] const MaterialDefinition* find(MaterialId id) const noexcept;
    [[nodiscard]] const std::vector<MaterialDefinition>& definitions() const noexcept {
        return definitions_;
    }
    [[nodiscard]] std::size_t size() const noexcept { return definitions_.size(); }

    [[nodiscard]] static MaterialCatalog makeDefault();

private:
    std::vector<MaterialDefinition> definitions_;
    bool frozen_{false};
};

} // namespace genomes::destruction
