#pragma once

#include <cstdint>
#include <limits>

namespace genomes::foundation {

template <class Tag>
struct Handle final {
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{InvalidIndex};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return index != InvalidIndex && generation != 0;
    }

    [[nodiscard]] constexpr std::uint64_t packed() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U) |
               static_cast<std::uint64_t>(index);
    }

    friend constexpr auto operator<=>(const Handle&, const Handle&) noexcept = default;
};

} // namespace genomes::foundation
