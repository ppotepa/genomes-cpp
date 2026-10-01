#include <genomes/world/WorldSave.hpp>

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <vector>

int main() {
    using namespace genomes;
    world::WorldSaveModel model{};
    model.metadata.generator_version = 2U;
    model.metadata.seed = 0x1234ULL;
    model.metadata.tick = {90U};
    model.metadata.content_hash = 0xAA55ULL;
    model.metadata.catalog_hash = 0x55AAULL;
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
    const auto canonical = world::WorldSaveCodec::serialize(loaded.value());
    assert(canonical && canonical.value() == first.value());

    std::vector<std::byte> corrupt = first.value();
    corrupt.back() ^= std::byte{1};
    assert(!world::WorldSaveCodec::deserialize(corrupt));
    std::vector<std::byte> truncated = first.value();
    truncated.pop_back();
    assert(!world::WorldSaveCodec::deserialize(truncated));
    std::vector<std::byte> unknown_schema = first.value();
    unknown_schema[4] = std::byte{2};
    assert(!world::WorldSaveCodec::deserialize(unknown_schema));

    auto zero_seed = model;
    zero_seed.metadata.seed = 0U;
    assert(!world::WorldSaveCodec::serialize(zero_seed));
    auto duplicate_entity = model;
    duplicate_entity.entities.push_back(duplicate_entity.entities.front());
    assert(!world::WorldSaveCodec::serialize(duplicate_entity));
    world::WorldSaveLimits no_working_memory{};
    no_working_memory.max_working_bytes = 1U;
    assert(!world::WorldSaveCodec::serialize(model, no_working_memory));
    const auto memory_limited = world::WorldSaveCodec::deserialize(first.value(), no_working_memory);
    assert(!memory_limited && memory_limited.error().code == foundation::ErrorCode::OutOfRange);

    const std::filesystem::path path = "genomes_world_save_test.bin";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    assert(world::WorldSaveCodec::saveFile(path, model));
    const auto from_file = world::WorldSaveCodec::loadFile(path);
    assert(from_file && from_file.value().valid());
    std::filesystem::remove(path, ignored);
    const auto missing_file = world::WorldSaveCodec::loadFile(path);
    assert(!missing_file && missing_file.error().code == foundation::ErrorCode::NotFound);
    return 0;
}
