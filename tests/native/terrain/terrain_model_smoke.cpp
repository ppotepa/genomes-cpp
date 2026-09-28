#include <genomes/terrain/HeightField.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    genomes::terrain::TerrainSpec spec;
    spec.world_id = genomes::world::WorldId(1);
    spec.region = {0, 0, 0};
    spec.samples_x = 5;
    spec.samples_z = 5;
    spec.cell_size_m = 1.0F;

    auto result = genomes::terrain::HeightField::create(spec);
    assert(result);
    auto& field = result.value();
    for (std::uint32_t z = 0; z < field.height(); ++z) {
        for (std::uint32_t x = 0; x < field.width(); ++x) {
            field.at(x, z) = 2.0F * static_cast<float>(x) +
                             3.0F * static_cast<float>(z) + 1.0F;
        }
    }

    assert(std::abs(field.sampleBilinear(0.25, 0.75) - 3.75F) < 1e-5F);
    assert(std::abs(field.sampleBilinear(-100.0, -100.0) - 1.0F) < 1e-5F);
    const auto normal = field.normal(2.0, 2.0);
    assert(std::abs(normal.x + 2.0F / std::sqrt(14.0F)) < 1e-5F);
    assert(std::abs(normal.y - 1.0F / std::sqrt(14.0F)) < 1e-5F);
    assert(std::abs(normal.z + 3.0F / std::sqrt(14.0F)) < 1e-5F);

    genomes::terrain::TerrainSpec invalid;
    invalid.samples_x = 1;
    assert(!genomes::terrain::HeightField::create(invalid));
    return 0;
}
