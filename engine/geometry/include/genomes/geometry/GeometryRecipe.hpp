#pragma once

#include <genomes/foundation/StableHash.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace genomes::geometry {

enum class GeometryOperation : std::uint8_t { Primitive, Profile, Parametric, Imported };
struct GeometryRecipe final {
    GeometryOperation operation{GeometryOperation::Primitive};
    std::string_view provider_version{};
    std::span<const float> parameters{};
    std::uint64_t policy_fingerprint{0};
    [[nodiscard]] std::uint64_t identity() const noexcept;
};

} // namespace genomes::geometry
