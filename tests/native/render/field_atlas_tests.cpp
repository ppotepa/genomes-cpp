#include <genomes/compute/FieldAtlas.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <cassert>
#include <cstdint>
#include <variant>
#include <vector>

int main() {
    using genomes::compute::FieldAtlas;
    using genomes::compute::FieldAtlasDescriptor;
    using genomes::compute::FieldAuthority;
    using genomes::compute::FieldFormat;
    using genomes::compute::FieldRegion;

    FieldAtlas atlas;
    const auto id = genomes::foundation::stableHashString("test.threat");
    const FieldAtlasDescriptor descriptor{
        id, 4U, 3U, 2U, genomes::foundation::Vec2{10.0F, -4.0F},
        genomes::foundation::Vec2{2.0F, 2.0F}, FieldFormat::R32Float,
        FieldAuthority::DerivedAuthoritativeWithCpuFallback,
    };
    assert(atlas.define(descriptor));
    assert(atlas.contains(id));
    assert(atlas.revision(id) == 1U);

    const std::vector<float> values{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    const FieldRegion region{1U, 1U, 0U, 3U, 2U};
    assert(atlas.writeFloat(id, region, values));
    assert(atlas.revision(id) == 2U);
    const auto read = atlas.readFloat(id, region);
    assert(read && read.value() == values);
    const auto mapped = atlas.worldToCell(id, genomes::foundation::Vec2{12.1F, -3.9F}, 1U);
    assert(mapped && mapped.value().x == 1U && mapped.value().y == 0U &&
           mapped.value().layer == 1U);
    assert(atlas.sampleFloat(id, genomes::foundation::Vec2{12.1F, -3.9F}, 1U));
    assert(atlas.sampleFloat(id, genomes::foundation::Vec2{12.1F, -3.9F}, 1U).value() ==
           1.0F);
    assert(!atlas.sampleFloat(id, genomes::foundation::Vec2{2.0F, 0.0F}, 0U));

    const auto dirty = atlas.consumeDirtyRegions();
    assert(dirty.size() == 2U);
    assert(dirty.back().region.layer == 1U);
    assert(atlas.consumeDirtyRegions().empty());

    const auto readback_token = atlas.requestReadback(id, region);
    assert(readback_token);
    const auto readback = atlas.pollReadback(readback_token.value());
    assert(readback && readback.value().state ==
                          genomes::compute::FieldReadbackState::Ready &&
           readback.value().source_revision == 2U);
    assert(std::get<std::vector<float>>(readback.value().values) == values);
    const std::vector<float> replacement{42.0F};
    assert(atlas.writeFloat(id, FieldRegion{1U, 1U, 0U, 1U, 1U}, replacement));
    const auto stale = atlas.pollReadback(readback_token.value());
    assert(stale && stale.value().state == genomes::compute::FieldReadbackState::Stale &&
           std::get<std::vector<float>>(stale.value().values) == values);
    assert(atlas.releaseReadback(readback_token.value()));
    assert(!atlas.pollReadback(readback_token.value()));

    const auto recreated_token = atlas.requestReadback(id, region);
    assert(recreated_token);
    assert(atlas.define(descriptor));
    const auto recreated_stale = atlas.pollReadback(recreated_token.value());
    assert(recreated_stale && recreated_stale.value().state ==
                                  genomes::compute::FieldReadbackState::Stale);
    assert(atlas.releaseReadback(recreated_token.value()));

    const auto all_layers = atlas.readFloat(id, FieldRegion{0U, 0U, 0U, 1U, 1U, 2U});
    assert(all_layers && all_layers.value().size() == 2U && all_layers.value()[0] == 0.0F &&
           all_layers.value()[1] == 0.0F);

    const auto uint_id = genomes::foundation::stableHashString("test.visibility");
    assert(atlas.define(FieldAtlasDescriptor{
        uint_id, 2U, 2U, 1U, {}, {1.0F, 1.0F}, FieldFormat::R32Uint,
        FieldAuthority::DerivedAdvisory,
    }));
    const std::vector<std::uint32_t> masks{7U, 8U, 9U, 10U};
    assert(atlas.writeUint(uint_id, FieldRegion{0U, 0U, 0U, 2U, 2U}, masks));
    assert(atlas.readUint(uint_id, FieldRegion{0U, 0U, 0U, 2U, 2U}).value() == masks);
    assert(!atlas.writeFloat(uint_id, FieldRegion{0U, 0U, 0U, 1U, 1U},
                             std::span<const float>{}));
    const auto uint_readback = atlas.requestReadback(
        uint_id, FieldRegion{0U, 0U, 0U, 2U, 2U});
    assert(uint_readback);
    const auto uint_snapshot = atlas.pollReadback(uint_readback.value());
    assert(uint_snapshot && uint_snapshot.value().state ==
                               genomes::compute::FieldReadbackState::Ready &&
           std::get<std::vector<std::uint32_t>>(uint_snapshot.value().values) == masks);
    assert(atlas.releaseReadback(uint_readback.value()));
    assert(atlas.remove(uint_id));
    assert(!atlas.contains(uint_id));
    return 0;
}
