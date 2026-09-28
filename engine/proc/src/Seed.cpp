#include <genomes/proc/SeedPath.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <cstddef>
#include <vector>

namespace genomes::proc {

namespace {

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (std::uint32_t index = 0; index < 4; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffu));
    }
}

void appendU64(std::vector<std::byte>& bytes, std::uint64_t value) {
    for (std::uint32_t index = 0; index < 8; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffu));
    }
}

} // namespace

SeedPath SeedPath::child(std::string_view label, std::uint64_t index) const noexcept {
    std::vector<std::byte> encoded;
    encoded.reserve(4 + 8 + 4 + label.size() + 8);
    appendU32(encoded, SeedDerivationVersion);
    appendU64(encoded, seed_);
    appendU32(encoded, static_cast<std::uint32_t>(label.size()));
    for (const unsigned char character : label) {
        encoded.push_back(static_cast<std::byte>(character));
    }
    appendU64(encoded, index);
    Seed result = foundation::stableHashBytes(encoded);
    if (result == 0) {
        result = 1;
    }
    return SeedPath(result);
}

} // namespace genomes::proc
