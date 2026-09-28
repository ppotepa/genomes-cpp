#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstdint>
#include <string_view>

namespace genomes::proc {

class SeedPath final {
public:
    constexpr explicit SeedPath(Seed seed = 0) noexcept : seed_(seed) {}

    [[nodiscard]] constexpr Seed seed() const noexcept {
        return seed_;
    }

    [[nodiscard]] SeedPath child(std::string_view label, std::uint64_t index) const noexcept;

    [[nodiscard]] SeedPath childStableId(
        std::string_view label, foundation::StableId id) const noexcept {
        return child(label, id);
    }

    friend constexpr bool operator==(const SeedPath&, const SeedPath&) noexcept = default;

private:
    Seed seed_{0};
};

} // namespace genomes::proc
