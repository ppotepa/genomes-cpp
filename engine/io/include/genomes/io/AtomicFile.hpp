#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace genomes::io {

struct AtomicFileConfig final {
    std::size_t max_bytes{64U * 1024U * 1024U};
};

class AtomicFile final {
public:
    [[nodiscard]] static foundation::Result<void, foundation::Error> write(
        const std::filesystem::path& target, std::span<const std::byte> bytes,
        AtomicFileConfig config = {});

    [[nodiscard]] static foundation::Result<std::vector<std::byte>, foundation::Error> read(
        const std::filesystem::path& target, AtomicFileConfig config = {});
};

} // namespace genomes::io
