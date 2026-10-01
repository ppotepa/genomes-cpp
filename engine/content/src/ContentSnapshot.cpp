#include <genomes/content/ContentSnapshot.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace genomes::content {

namespace {

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                      std::string_view message) noexcept {
    return {code, message};
}

[[nodiscard]] bool isSafeRelative(const std::filesystem::path& path) noexcept {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory() ||
        path.lexically_normal() != path) {
        return false;
    }
    return std::none_of(path.begin(), path.end(), [](const auto& component) {
        return component == "..";
    });
}

} // namespace

foundation::Result<std::filesystem::path, foundation::Error>
resolveContentPath(const std::filesystem::path& root, const std::filesystem::path& relative) {
    if (!isSafeRelative(relative)) {
        return foundation::Result<std::filesystem::path, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "content path must be a safe relative path"));
    }
    std::error_code ec;
    const auto canonical_root = std::filesystem::weakly_canonical(root, ec);
    if (ec || canonical_root.empty() ||
        std::filesystem::is_symlink(std::filesystem::symlink_status(canonical_root, ec)) || ec) {
        return foundation::Result<std::filesystem::path, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "content root is not a canonical directory"));
    }
    auto candidate = canonical_root;
    for (const auto& component : relative) {
        candidate /= component;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(candidate, ec)) || ec) {
            return foundation::Result<std::filesystem::path, foundation::Error>::failure(
                error(foundation::ErrorCode::InvalidArgument, "content path contains a reparse point"));
        }
    }
    candidate = std::filesystem::weakly_canonical(candidate, ec);
    if (ec || candidate.empty() || !isSafeRelative(candidate.lexically_relative(canonical_root))) {
        return foundation::Result<std::filesystem::path, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "content path escapes its root"));
    }
    return foundation::Result<std::filesystem::path, foundation::Error>::success(std::move(candidate));
}

foundation::Result<ContentDocument, foundation::Error>
readContentText(const std::filesystem::path& path, ContentReadLimits limits) {
    std::error_code ec;
    if (limits.maximum_bytes == 0U || !std::filesystem::is_regular_file(path, ec) || ec) {
        return foundation::Result<ContentDocument, foundation::Error>::failure(
            error(foundation::ErrorCode::NotFound, "content document is not a regular file"));
    }
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size > limits.maximum_bytes) {
        return foundation::Result<ContentDocument, foundation::Error>::failure(
            error(foundation::ErrorCode::OutOfRange, "content document exceeds read limit"));
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return foundation::Result<ContentDocument, foundation::Error>::failure(
            error(foundation::ErrorCode::NotFound, "content document cannot be opened"));
    }
    std::string text(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
    if ((!stream.good() && !stream.eof()) || text.size() != file_size) {
        return foundation::Result<ContentDocument, foundation::Error>::failure(
            error(foundation::ErrorCode::Internal, "content document could not be read"));
    }
    ContentDocument result{};
    result.text = std::move(text);
    result.provenance.source_id = path.filename().string();
    result.provenance.path = path;
    result.provenance.content_hash = foundation::stableHashString(result.text);
    return foundation::Result<ContentDocument, foundation::Error>::success(std::move(result));
}

ContentSnapshotBuilder::ContentSnapshotBuilder(std::string package_id,
                                               std::uint32_t schema_version)
    : package_id_{std::move(package_id)}, schema_version_{schema_version} {}

foundation::Result<void, foundation::Error>
ContentSnapshotBuilder::add(ContentProvenance provenance) {
    if (frozen_ || package_id_.empty() || schema_version_ == 0U || provenance.source_id.empty() ||
        provenance.path.empty()) {
        return foundation::Result<void, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidState, "invalid content snapshot mutation"));
    }
    sources_.push_back(std::move(provenance));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<FrozenContentSnapshot, foundation::Error>
ContentSnapshotBuilder::freeze() && {
    if (frozen_ || package_id_.empty() || schema_version_ == 0U) {
        return foundation::Result<FrozenContentSnapshot, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidState, "invalid content snapshot freeze"));
    }
    std::sort(sources_.begin(), sources_.end(), [](const auto& left, const auto& right) {
        return left.source_id != right.source_id ? left.source_id < right.source_id
                                                 : left.path.generic_string() < right.path.generic_string();
    });
    if (std::adjacent_find(sources_.begin(), sources_.end(), [](const auto& left, const auto& right) {
            return left.source_id == right.source_id || left.path == right.path;
        }) != sources_.end()) {
        return foundation::Result<FrozenContentSnapshot, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "duplicate content provenance"));
    }
    foundation::StableId fingerprint = foundation::stableHashString(package_id_);
    fingerprint = foundation::stableHashCombine(fingerprint, schema_version_);
    for (const auto& source : sources_) {
        fingerprint = foundation::stableHashCombine(fingerprint,
                                                    foundation::stableHashString(source.source_id));
        fingerprint = foundation::stableHashCombine(
            fingerprint, foundation::stableHashString(source.path.generic_string()));
        fingerprint = foundation::stableHashCombine(fingerprint, source.content_hash);
    }
    frozen_ = true;
    return foundation::Result<FrozenContentSnapshot, foundation::Error>::success(
        {std::move(package_id_), schema_version_, std::move(sources_), fingerprint});
}

} // namespace genomes::content
