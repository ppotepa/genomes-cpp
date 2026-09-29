#pragma once

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <cstdint>

namespace genomes::proc {

class RandomStream final {
public:
    explicit RandomStream(Seed seed, std::uint64_t stream_selector = 0) noexcept {
        const std::uint64_t selector =
            stream_selector == 0
                ? foundation::stableHashCombine(seed, 0x50434733325f5631ull)
                : stream_selector;
        increment_ = (selector << 1U) | 1U;
        state_ = 0;
        (void)nextU32();
        state_ += seed;
        (void)nextU32();
    }

    explicit RandomStream(const SeedPath& path) noexcept : RandomStream(path.seed()) {}

    [[nodiscard]] std::uint32_t nextU32() noexcept {
        const std::uint64_t old_state = state_;
        state_ = old_state * 6364136223846793005ull + increment_;
        const std::uint32_t xorshifted =
            static_cast<std::uint32_t>(((old_state >> 18U) ^ old_state) >> 27U);
        const std::uint32_t rotation = static_cast<std::uint32_t>(old_state >> 59U);
        return (xorshifted >> rotation) | (xorshifted << ((0U - rotation) & 31U));
    }

    [[nodiscard]] double uniform01() noexcept {
        return static_cast<double>(nextU32()) / 4294967296.0;
    }

    [[nodiscard]] double uniformRange(double minimum, double maximum) noexcept {
        return minimum + (maximum - minimum) * uniform01();
    }

    [[nodiscard]] std::uint32_t bounded(std::uint32_t bound) noexcept {
        if (bound == 0) {
            return 0;
        }
        const std::uint32_t threshold = (0U - bound) % bound;
        for (;;) {
            const std::uint32_t value = nextU32();
            if (value >= threshold) {
                return value % bound;
            }
        }
    }

private:
    std::uint64_t state_{0};
    std::uint64_t increment_{1};
};

} // namespace genomes::proc
