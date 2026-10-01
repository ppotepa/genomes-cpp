#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::content {

struct ContentReadLimits final {
    std::size_t maximum_bytes{1024U * 1024U};
};

struct ContentProvenance final {
    std::string source_id;
    std::filesystem::path path;
    foundation::StableId content_hash{0U};
};

struct ContentDocument final {
    std::string text;
    ContentProvenance provenance;
};

// Domain loaders own their field schema. The neutral manifest records only
// package identity, version and ordering dependencies.
struct ContentManifest final {
    std::string id;
    std::uint32_t schema_version{0U};
    std::string version;
    std::vector<std::string> dependencies;
};

struct FrozenContentSnapshot final {
    std::string package_id;
    std::uint32_t schema_version{0U};
    std::vector<ContentProvenance> sources;
    foundation::StableId fingerprint{0U};
};

[[nodiscard]] foundation::Result<std::filesystem::path, foundation::Error>
resolveContentPath(const std::filesystem::path& root, const std::filesystem::path& relative);

[[nodiscard]] foundation::Result<ContentDocument, foundation::Error>
readContentText(const std::filesystem::path& path, ContentReadLimits limits = {});

class ContentSnapshotBuilder final {
public:
    ContentSnapshotBuilder(std::string package_id, std::uint32_t schema_version);

    [[nodiscard]] foundation::Result<void, foundation::Error>
    add(ContentProvenance provenance);

    [[nodiscard]] foundation::Result<FrozenContentSnapshot, foundation::Error>
    freeze() &&;

private:
    std::string package_id_;
    std::uint32_t schema_version_{0U};
    std::vector<ContentProvenance> sources_;
    bool frozen_{false};
};

} // namespace genomes::content
