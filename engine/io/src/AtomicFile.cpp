#include <genomes/io/AtomicFile.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace genomes::io {

namespace {

using foundation::Error;
using foundation::ErrorCode;

std::atomic<std::uint64_t> next_temp_id{1U};

[[nodiscard]] Error error(ErrorCode code, std::string_view message) noexcept {
    return Error{code, message};
}

[[nodiscard]] bool validConfig(AtomicFileConfig config) noexcept {
    return config.max_bytes > 0U &&
           config.max_bytes <= static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
}

[[nodiscard]] std::filesystem::path temporaryPath(const std::filesystem::path& target) {
    const auto suffix = std::string{std::string_view{".tmp."}} +
                        std::to_string(next_temp_id.fetch_add(1U));
    return target.parent_path() / (target.filename().string() + suffix);
}

} // namespace

foundation::Result<void, foundation::Error> AtomicFile::write(
    const std::filesystem::path& target, std::span<const std::byte> bytes,
    AtomicFileConfig config) {
    if (target.empty() || target.filename().empty() || !validConfig(config)) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::InvalidArgument, "invalid atomic file arguments"));
    }
    if (bytes.size() > config.max_bytes) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::OutOfRange, "atomic file exceeds configured limit"));
    }
    std::error_code filesystem_error;
    if (std::filesystem::exists(target, filesystem_error) &&
        std::filesystem::is_directory(target, filesystem_error)) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::InvalidArgument, "atomic file target is a directory"));
    }
    const auto temporary = temporaryPath(target);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return foundation::Result<void, Error>::failure(
                error(ErrorCode::Internal, "unable to open atomic temporary file"));
        }
        if (!bytes.empty()) {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
        }
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, filesystem_error);
            return foundation::Result<void, Error>::failure(
                error(ErrorCode::Internal, "unable to flush atomic temporary file"));
        }
    }

    std::filesystem::rename(temporary, target, filesystem_error);
    if (filesystem_error) {
        // Windows does not replace an existing file with rename(). Retry the
        // exact target after removing it; no user-visible partial target is
        // created because the temporary remains complete until this point.
        if (std::filesystem::exists(target, filesystem_error)) {
            std::filesystem::remove(target, filesystem_error);
            if (!filesystem_error) {
                std::filesystem::rename(temporary, target, filesystem_error);
            }
        }
    }
    if (filesystem_error) {
        std::filesystem::remove(temporary, filesystem_error);
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::Internal, "unable to commit atomic file"));
    }
    return foundation::Result<void, Error>::success();
}

foundation::Result<std::vector<std::byte>, foundation::Error> AtomicFile::read(
    const std::filesystem::path& target, AtomicFileConfig config) {
    if (target.empty() || !validConfig(config)) {
        return foundation::Result<std::vector<std::byte>, Error>::failure(
            error(ErrorCode::InvalidArgument, "invalid atomic read arguments"));
    }
    std::ifstream stream(target, std::ios::binary | std::ios::ate);
    if (!stream) {
        return foundation::Result<std::vector<std::byte>, Error>::failure(
            error(ErrorCode::NotFound, "atomic file cannot be opened"));
    }
    const auto end = stream.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) > config.max_bytes) {
        return foundation::Result<std::vector<std::byte>, Error>::failure(
            error(ErrorCode::OutOfRange, "atomic file exceeds configured limit"));
    }
    const auto size = static_cast<std::size_t>(end);
    stream.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(size);
    if (size > 0U) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!stream && !stream.eof()) {
        return foundation::Result<std::vector<std::byte>, Error>::failure(
            error(ErrorCode::Internal, "unable to read atomic file"));
    }
    return foundation::Result<std::vector<std::byte>, Error>::success(std::move(bytes));
}

} // namespace genomes::io
