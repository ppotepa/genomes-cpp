#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/Equipment.hpp>
#include <genomes/infantry/SkeletonData.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace genomes::infantry {

struct EquipmentFitSlot final {
    bool occupied{false};
    EquipmentSlot slot{};
    BoneId bone{BoneId::Hips};
    foundation::Vec3 anchor{};
    foundation::Vec3 dimensions{};
    float scale{1.0F};
    float thickness{0.0F};
};

struct EquipmentFit final {
    std::uint32_t version{1};
    foundation::StableId identity{0};
    std::array<EquipmentFitSlot, kEquipmentSlotCount> slots{};

    [[nodiscard]] bool valid(const SkeletonData&) const noexcept;
    [[nodiscard]] const EquipmentFitSlot& at(EquipmentSlot slot) const noexcept {
        return slots[equipmentSlotIndex(slot)];
    }
};

class EquipmentFitter final {
public:
    [[nodiscard]] static foundation::Result<EquipmentFit, foundation::Error> build(
        const EquipmentState&, const BodyPhenotype&, const SkeletonData&);
};

struct GearPiece final {
    EquipmentSlot slot{};
    foundation::StableId definition_id{0};
    BoneId bone{BoneId::Hips};
    foundation::Vec3 center{};
    foundation::Vec3 dimensions{};
    foundation::Color color{};
    std::uint32_t material_region{0};
};

struct GearArtifact final {
    std::uint32_t version{1};
    foundation::StableId cache_key{0};
    std::vector<GearPiece> pieces;

    [[nodiscard]] bool valid(const SkeletonData&) const noexcept;
};

class GearGenerator final {
public:
    [[nodiscard]] static foundation::Result<GearArtifact, foundation::Error> build(
        const EquipmentState&, const EquipmentFit&, const SkeletonData&,
        foundation::Color palette = {});
};

class GearCache final {
public:
    using Artifact = std::shared_ptr<const GearArtifact>;

    [[nodiscard]] Artifact find(foundation::StableId key) const;
    [[nodiscard]] Artifact acquire(const EquipmentState&, const EquipmentFit&,
                                   const SkeletonData&, foundation::Color palette = {});
    [[nodiscard]] std::size_t size() const noexcept;
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<foundation::StableId, Artifact> entries_;
};

} // namespace genomes::infantry
