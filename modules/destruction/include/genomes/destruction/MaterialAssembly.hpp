#pragma once

#include <genomes/destruction/Solid.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace genomes::destruction {

struct MaterialFieldSample final {
    MaterialId material{};
    PhysicalSolidId physical_solid{};
    foundation::Vec3 local_position{};
    float density_scale{1.0F};
    float strength_scale{1.0F};
    float roughness{0.0F};
    std::uint64_t cell_seed{0};
    bool void_layer{true};
};

class MaterialAssembly final {
public:
    [[nodiscard]] static foundation::Result<MaterialAssembly, foundation::Error> create(
        proc::Seed root_seed,
        MaterialFrame frame,
        std::vector<Layer> layers,
        const MaterialCatalog& catalog);

    [[nodiscard]] foundation::Result<void, foundation::Error> validate(
        const MaterialCatalog& catalog) const noexcept;

    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] proc::Seed rootSeed() const noexcept { return root_seed_; }
    [[nodiscard]] const MaterialFrame& frame() const noexcept { return frame_; }
    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }

    [[nodiscard]] std::optional<std::size_t> layerIndexAt(float distance) const noexcept;
    [[nodiscard]] std::vector<TraversalSegment> traverse(float start,
                                                          float end) const;
    [[nodiscard]] std::vector<InterfaceGroup> interfaceGroups() const;
    [[nodiscard]] MaterialFieldSample sample(foundation::Vec3 world_point) const noexcept;

private:
    std::uint32_t version_{MaterialAssemblyVersion};
    proc::Seed root_seed_{0};
    MaterialFrame frame_{};
    std::vector<Layer> layers_;
};

} // namespace genomes::destruction
