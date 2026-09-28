#include <genomes/io/AtomicFile.hpp>

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <vector>

int main() {
    const auto target = std::filesystem::temp_directory_path() / "genomes_atomic_file_test.bin";
    const std::vector<std::byte> payload{std::byte{0x01}, std::byte{0x02}, std::byte{0xA5}};
    assert(genomes::io::AtomicFile::write(target, payload));
    const auto loaded = genomes::io::AtomicFile::read(target);
    assert(loaded && loaded.value() == payload);
    assert(!genomes::io::AtomicFile::write(
        target, payload, genomes::io::AtomicFileConfig{2U}));
    assert(!genomes::io::AtomicFile::read(
        target, genomes::io::AtomicFileConfig{2U}));
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    return 0;
}
