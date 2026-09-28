#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>
#include <span>
#include <string_view>

namespace genomes::foundation {

inline constexpr std::uint32_t StableHashAlgorithmVersion = 1;

[[nodiscard]] std::uint64_t stableHashBytes(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] std::uint64_t stableHashString(std::string_view value) noexcept;
[[nodiscard]] std::uint64_t stableHashU64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t stableHashCombine(std::uint64_t seed,
                                              std::uint64_t value) noexcept;

[[nodiscard]] inline std::uint64_t stableHashFloat(float value) noexcept {
    return stableHashU64(static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

} // namespace genomes::foundation
