#pragma once

#include <compare>
#include <cstdint>
#include <type_traits>

namespace genomes::foundation {

template <class Tag, class Rep = std::uint64_t>
class StrongId final {
    static_assert(std::is_integral_v<Rep>, "StrongId representation must be integral");

public:
    constexpr StrongId() noexcept = default;

    explicit constexpr StrongId(Rep value) noexcept : value_(value) {}

    [[nodiscard]] constexpr Rep value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return value_ != Rep{};
    }

    explicit constexpr operator bool() const noexcept {
        return isValid();
    }

    friend constexpr auto operator<=>(const StrongId&, const StrongId&) noexcept = default;

private:
    Rep value_{0};
};

} // namespace genomes::foundation
