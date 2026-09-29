#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/Equipment.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/ResolvedAnatomy.hpp>
#include <genomes/infantry/SkeletonData.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace genomes::infantry {

enum class EquipmentSocketId : std::uint8_t {
    Head, HeadFront, Neck, ChestCenter, ChestLeft, BackCenter, Waist,
    WaistFront, WaistBack, HipL, HipR, ThighL, ThighR, HandL, HandR,
    FootL, FootR, WeaponBack, WeaponHip, Count
};

struct EquipmentSocket final {
    BoneId bone{BoneId::Hips};
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 0.0F, 1.0F};
};

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
    float height{1.0F};
    double reference_height{1.0};
    float hip_y{0.0F};
    double reference_hip_y{0.0};
    float armor_thickness{0.0F};
    float hair_floor{0.0F};
    float pants_ease{1.0F};
    float boot_width{1.0F};
    float boot_shaft{1.0F};
    bool gloves_present{false};
    bool gloves_fingerless{false};
    BodyPhenotype body{};
    FacePhenotype face{};
    ResolvedAnatomy face_anatomy{};
    std::array<BodyCrossSection, 14U> jacket{};
    foundation::Vec3 pack_dimensions{};
    std::optional<EquipmentKind> headgear_kind{};
    EquipmentVisualDefinition headgear_visual{};
    std::array<EquipmentSocket, static_cast<std::size_t>(EquipmentSocketId::Count)> sockets{};
    std::array<foundation::Vec3, kRigBoneCount> bind_points{};
    std::array<std::array<double,3U>, kRigBoneCount> reference_bind_points{};

    [[nodiscard]] bool valid(const SkeletonData&) const noexcept;
    [[nodiscard]] const EquipmentFitSlot& at(EquipmentSlot slot) const noexcept {
        return slots[equipmentSlotIndex(slot)];
    }
    [[nodiscard]] float mapTorsoY(float reference_y) const noexcept;
    // Authoring-number variant used by the reference surface port. The JS
    // implementation maps before any Float32BufferAttribute conversion.
    [[nodiscard]] double mapTorsoYExact(double reference_y) const noexcept;
    [[nodiscard]] foundation::Vec2 profile(float y) const noexcept;
    [[nodiscard]] foundation::Vec3 front(float x, float y, float gap = 0.0F) const noexcept;
    [[nodiscard]] const EquipmentSocket& socket(EquipmentSocketId id) const noexcept {
        return sockets[static_cast<std::size_t>(id)];
    }
    [[nodiscard]] float headBottom(float theta) const noexcept;
    [[nodiscard]] foundation::Vec3 facePoint(float y, float theta) const noexcept;
    [[nodiscard]] float faceFrontZ(float x, float y) const noexcept;
};

class EquipmentFitter final {
public:
    [[nodiscard]] static foundation::Result<EquipmentFit, foundation::Error> build(
        const EquipmentState&, const PhenotypeArtifact&, const SkeletonData&);
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
    EquipmentState equipment{};
    EquipmentFit fit{};
    foundation::Color palette{};
    std::uint32_t detail_level{2U};
    float wear{0.0F};
    std::vector<GearPiece> pieces;

    [[nodiscard]] bool valid(const SkeletonData&) const noexcept;
};

class GearGenerator final {
public:
    [[nodiscard]] static foundation::Result<GearArtifact, foundation::Error> build(
        const EquipmentState&, const EquipmentFit&, const SkeletonData&,
        foundation::Color palette = {}, std::uint32_t detail_level = 2U,
        float wear = 0.0F);
};

class GearCache final {
public:
    using Artifact = std::shared_ptr<const GearArtifact>;

    [[nodiscard]] Artifact find(foundation::StableId key) const;
    [[nodiscard]] Artifact acquire(const EquipmentState&, const EquipmentFit&,
                                   const SkeletonData&, foundation::Color palette = {},
                                   std::uint32_t detail_level = 2U, float wear = 0.0F);
    [[nodiscard]] std::size_t size() const noexcept;
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<foundation::StableId, Artifact> entries_;
};

} // namespace genomes::infantry
