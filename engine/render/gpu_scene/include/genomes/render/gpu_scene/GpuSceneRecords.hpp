#pragma once

#include <genomes/foundation/Handle.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/render/RenderTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace genomes::render::gpu_scene {

struct GpuInstanceTag;
using GpuInstanceHandle = foundation::Handle<GpuInstanceTag>;

enum class GpuRecordChange : std::uint8_t {
    Added,
    Updated,
    Removed,
};

// CPU shadow record. Semantic IDs stay on the CPU side; a backend resolves
// mesh/material IDs to compact GPU table indices when it stages the upload.
struct alignas(16) GpuInstanceRecord final {
    static constexpr std::uint32_t ActiveFlag = 1U;

    foundation::StableId semantic_id{0};
    foundation::StableId mesh_id{0};
    foundation::StableId material_id{0};
    foundation::Vec3 position{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    std::uint32_t flags{0};
    std::uint32_t reserved[2]{};

    [[nodiscard]] bool operator==(const GpuInstanceRecord& other) const noexcept {
        return semantic_id == other.semantic_id && mesh_id == other.mesh_id &&
               material_id == other.material_id && position.x == other.position.x &&
               position.y == other.position.y && position.z == other.position.z &&
               scale.x == other.scale.x && scale.y == other.scale.y &&
               scale.z == other.scale.z && rotation_y == other.rotation_y &&
               flags == other.flags;
    }

    [[nodiscard]] static GpuInstanceRecord fromRender(
        const RenderInstance& instance) noexcept {
        return {instance.object_id, instance.mesh_id, instance.material_id,
                instance.position, instance.scale, instance.rotation_y,
                ActiveFlag | instance.flags, {}};
    }
};

static_assert(std::is_standard_layout_v<GpuInstanceRecord>);
static_assert(alignof(GpuInstanceRecord) == 16);
static_assert(sizeof(GpuInstanceRecord) == 64);

struct GpuUploadRange final {
    std::uint32_t first_slot{0};
    std::uint32_t count{0};
    std::size_t payload_offset{0};
};

struct GpuUploadBatch final {
    std::vector<GpuUploadRange> ranges;
    std::vector<GpuInstanceRecord> payload;
    std::vector<GpuInstanceHandle> removed;

    [[nodiscard]] bool empty() const noexcept {
        return ranges.empty() && removed.empty();
    }
};

} // namespace genomes::render::gpu_scene
