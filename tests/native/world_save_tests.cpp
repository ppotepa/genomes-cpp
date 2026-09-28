#include <genomes/world/WorldSave.hpp>

#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    using namespace genomes;
    world::WorldSaveModel model{};
    model.header.generator_version = 2U;
    model.header.seed = 0x1234ULL;
    model.header.tick = {90U};
    model.header.content_hash = 0xAA55ULL;
    model.header.catalog_hash = 0x55AAULL;
    model.regions = {{world::RegionId(20U), 0x20ULL, {9U, 3U}},
                     {world::RegionId(10U), 0x10ULL, {7U, 2U}}};
    model.entities = {{{2U, 1U}, {2.0F, 1.0F, 0.0F}, 4U, 0xBULL},
                      {{1U, 1U}, {1.0F, 1.0F, 0.0F}, 2U, 0xAULL}};
    const auto first = world::WorldSaveCodec::serialize(model);
    assert(first);
    const auto second = world::WorldSaveCodec::serialize(model);
    assert(second && first.value() == second.value());
    const auto loaded = world::WorldSaveCodec::deserialize(first.value());
    assert(loaded && loaded.value().valid());
    assert(loaded.value().regions.front().id.value() == 10U);
    assert(loaded.value().regions.front().destroyed_objects.front() == 2U);
    assert(loaded.value().entities.front().id.index == 1U);

    std::vector<std::byte> corrupt = first.value();
    corrupt.back() ^= std::byte{1};
    assert(!world::WorldSaveCodec::deserialize(corrupt));

    const std::filesystem::path path = "genomes_world_save_test.bin";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    assert(world::WorldSaveCodec::saveFile(path, model));
    const auto from_file = world::WorldSaveCodec::loadFile(path);
    assert(from_file && from_file.value().valid());
    std::filesystem::remove(path, ignored);
    return 0;
}
