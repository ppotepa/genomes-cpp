#include <genomes/foundation/StableHash.hpp>

#include <array>

namespace genomes::foundation {

namespace {

constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void feed(std::uint64_t& hash, std::byte byte) noexcept {
    hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(byte));
    hash *= FnvPrime;
}

void feedLittleEndian(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::uint32_t index = 0; index < 8; ++index) {
        feed(hash, static_cast<std::byte>((value >> (index * 8U)) & 0xffu));
    }
}

} // namespace

std::uint64_t stableHashBytes(std::span<const std::byte> bytes) noexcept {
    std::uint64_t hash = FnvOffsetBasis;
    for (const std::byte byte : bytes) {
        feed(hash, byte);
    }
    return hash;
}

std::uint64_t stableHashString(std::string_view value) noexcept {
    std::uint64_t hash = FnvOffsetBasis;
    for (const unsigned char byte : value) {
        feed(hash, static_cast<std::byte>(byte));
    }
    return hash;
}

std::uint64_t stableHashU64(std::uint64_t value) noexcept {
    std::uint64_t hash = FnvOffsetBasis;
    feedLittleEndian(hash, value);
    return hash;
}

std::uint64_t stableHashCombine(std::uint64_t seed, std::uint64_t value) noexcept {
    std::uint64_t hash = FnvOffsetBasis;
    feedLittleEndian(hash, seed);
    feedLittleEndian(hash, value);
    return hash;
}

} // namespace genomes::foundation
