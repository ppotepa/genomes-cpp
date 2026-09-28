#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool finite(foundation::Color value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           std::isfinite(value.a);
}

[[nodiscard]] BoneId anchorBone(EquipmentSlot slot) noexcept {
    switch (slot) {
    case EquipmentSlot::Head:
    case EquipmentSlot::Face:
        return BoneId::Head;
    case EquipmentSlot::Neck:
        return BoneId::Neck;
    case EquipmentSlot::TorsoBase:
    case EquipmentSlot::TorsoArmor:
    case EquipmentSlot::ChestRig:
    case EquipmentSlot::Utility3:
        return BoneId::Chest;
    case EquipmentSlot::Legs:
    case EquipmentSlot::Belt:
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Utility2:
        return BoneId::Hips;
    case EquipmentSlot::Feet:
        return BoneId::FootL;
    case EquipmentSlot::Hands:
        return BoneId::HandL;
    case EquipmentSlot::Back:
        return BoneId::Chest;
    case EquipmentSlot::LeftHip:
        return BoneId::ThighL;
    case EquipmentSlot::RightHip:
        return BoneId::ThighR;
    case EquipmentSlot::LeftThigh:
        return BoneId::ThighL;
    case EquipmentSlot::RightThigh:
        return BoneId::ThighR;
    case EquipmentSlot::MeleeWeapon:
    case EquipmentSlot::Throwable:
    case EquipmentSlot::PrimaryWeapon:
    case EquipmentSlot::SecondaryWeapon:
        return BoneId::Chest;
    }
    return BoneId::Hips;
}

[[nodiscard]] foundation::Vec3 baseDimensions(EquipmentSlot slot,
                                               const BodyPhenotype& body) noexcept {
    switch (slot) {
    case EquipmentSlot::Head:
        return {body.shoulder_width * 0.30F, body.height * 0.14F, body.chest_depth * 0.55F};
    case EquipmentSlot::Face:
        return {body.shoulder_width * 0.18F, body.height * 0.08F, body.chest_depth * 0.18F};
    case EquipmentSlot::Neck:
        return {body.shoulder_width * 0.16F, body.height * 0.06F, body.chest_depth * 0.28F};
    case EquipmentSlot::TorsoBase:
    case EquipmentSlot::TorsoArmor:
    case EquipmentSlot::ChestRig:
        return {body.shoulder_width * 0.82F, body.height * 0.30F, body.chest_depth * 1.15F};
    case EquipmentSlot::Legs:
        return {body.hip_width * 0.95F, body.height * 0.26F, body.chest_depth * 0.85F};
    case EquipmentSlot::Feet:
        return {body.hip_width * 0.28F, body.height * 0.08F, body.height * 0.13F};
    case EquipmentSlot::Hands:
        return {body.arm_length * 0.10F, body.arm_length * 0.08F, body.arm_length * 0.14F};
    case EquipmentSlot::Back:
        return {body.shoulder_width * 0.52F, body.height * 0.28F, body.chest_depth * 0.75F};
    case EquipmentSlot::Belt:
        return {body.hip_width * 1.05F, body.height * 0.045F, body.chest_depth * 0.32F};
    case EquipmentSlot::LeftHip:
    case EquipmentSlot::RightHip:
    case EquipmentSlot::LeftThigh:
    case EquipmentSlot::RightThigh:
        return {body.hip_width * 0.24F, body.height * 0.12F, body.chest_depth * 0.32F};
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Utility2:
    case EquipmentSlot::Utility3:
        return {body.hip_width * 0.22F, body.height * 0.11F, body.chest_depth * 0.26F};
    case EquipmentSlot::MeleeWeapon:
    case EquipmentSlot::Throwable:
    case EquipmentSlot::SecondaryWeapon:
        return {body.arm_length * 0.12F, body.arm_length * 0.08F, body.arm_length * 0.30F};
    case EquipmentSlot::PrimaryWeapon:
        return {body.arm_length * 0.16F, body.arm_length * 0.11F, body.arm_length * 0.78F};
    }
    return {0.1F, 0.1F, 0.1F};
}

} // namespace

bool EquipmentFit::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || identity == 0) {
        return false;
    }
    for (const EquipmentFitSlot& slot : slots) {
        if (!slot.occupied) {
            continue;
        }
        if (skeleton.find(slot.bone) == nullptr || !finite(slot.anchor) ||
            !finite(slot.dimensions) || !(slot.scale > 0.0F) ||
            !std::isfinite(slot.scale) || !std::isfinite(slot.thickness) ||
            slot.dimensions.x <= 0.0F || slot.dimensions.y <= 0.0F || slot.dimensions.z <= 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<EquipmentFit, foundation::Error> EquipmentFitter::build(
    const EquipmentState& state, const BodyPhenotype& body, const SkeletonData& skeleton) {
    if (!state.valid() || !body.valid() || !skeleton.valid()) {
        return foundation::Result<EquipmentFit, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid equipment fit input"});
    }
    EquipmentFit result{};
    result.identity = foundation::stableHashCombine(foundation::stable_id("infantry.equipment.fit.v1"),
                                                    state.identity);
    for (std::size_t index = 0U; index < kEquipmentSlotCount; ++index) {
        const EquipmentSlot slot = static_cast<EquipmentSlot>(index);
        EquipmentFitSlot& output = result.slots[index];
        output.slot = slot;
        const EquipmentItem* item = state.item(slot);
        if (item == nullptr) {
            continue;
        }
        const EquipmentItemDefinition* definition = EquipmentCatalog::findItem(item->definition_id);
        const BoneId bone = anchorBone(slot);
        const BoneRecord* anchor = skeleton.find(bone);
        if (definition == nullptr || anchor == nullptr) {
            return foundation::Result<EquipmentFit, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "equipment fit anchor is missing"});
        }
        output.occupied = true;
        output.bone = bone;
        output.anchor = anchor->world_bind.translation;
        output.scale = definition->fit_scale * item->variant.size;
        output.thickness = definition->fit_thickness;
        output.dimensions = baseDimensions(slot, body);
        output.dimensions = {output.dimensions.x * output.scale,
                             output.dimensions.y * output.scale,
                             output.dimensions.z * output.scale};
        if (slot == EquipmentSlot::LeftHip || slot == EquipmentSlot::LeftThigh) {
            output.anchor.x -= body.hip_width * 0.26F;
        } else if (slot == EquipmentSlot::RightHip || slot == EquipmentSlot::RightThigh) {
            output.anchor.x += body.hip_width * 0.26F;
        }
    }
    return result.valid(skeleton)
               ? foundation::Result<EquipmentFit, foundation::Error>::success(std::move(result))
               : foundation::Result<EquipmentFit, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "equipment fit constraints are infeasible"});
}

bool GearArtifact::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || cache_key == 0) {
        return false;
    }
    for (const GearPiece& piece : pieces) {
        if (piece.definition_id == 0 || skeleton.find(piece.bone) == nullptr ||
            !finite(piece.center) || !finite(piece.dimensions) || !finite(piece.color) ||
            piece.dimensions.x <= 0.0F || piece.dimensions.y <= 0.0F || piece.dimensions.z <= 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<GearArtifact, foundation::Error> GearGenerator::build(
    const EquipmentState& state,
    const EquipmentFit& fit,
    const SkeletonData& skeleton,
    foundation::Color palette) {
    if (!state.valid() || !fit.valid(skeleton) || !finite(palette)) {
        return foundation::Result<GearArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid gear generation input"});
    }
    GearArtifact result{};
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("infantry.gear.v1"), state.identity),
        fit.identity);
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(result.cache_key, foundation::stableHashFloat(palette.r)),
        foundation::stableHashCombine(foundation::stableHashFloat(palette.g),
                                       foundation::stableHashFloat(palette.b)));
    for (std::size_t index = 0U; index < kEquipmentSlotCount; ++index) {
        const EquipmentSlot slot = static_cast<EquipmentSlot>(index);
        const EquipmentItem* item = state.item(slot);
        const EquipmentFitSlot& fitted = fit.slots[index];
        if (item == nullptr || !fitted.occupied) {
            continue;
        }
        GearPiece piece{};
        piece.slot = slot;
        piece.definition_id = item->definition_id;
        piece.bone = fitted.bone;
        piece.center = fitted.anchor;
        piece.dimensions = fitted.dimensions;
        piece.color = {std::clamp(palette.r * item->variant.shade, 0.0F, 1.0F),
                       std::clamp(palette.g * item->variant.shade, 0.0F, 1.0F),
                       std::clamp(palette.b * item->variant.shade, 0.0F, 1.0F), palette.a};
        piece.material_region = static_cast<std::uint32_t>(index);
        result.pieces.push_back(piece);
    }
    return result.valid(skeleton)
               ? foundation::Result<GearArtifact, foundation::Error>::success(std::move(result))
               : foundation::Result<GearArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "gear artifact failed validation"});
}

GearCache::Artifact GearCache::find(foundation::StableId key) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(key);
    return iterator == entries_.end() ? Artifact{} : iterator->second;
}

GearCache::Artifact GearCache::acquire(const EquipmentState& state,
                                       const EquipmentFit& fit,
                                       const SkeletonData& skeleton,
                                       foundation::Color palette) {
    const foundation::StableId key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("infantry.gear.cache.v1"),
                                      state.identity),
        fit.identity);
    const foundation::StableId palette_key = foundation::stableHashCombine(
        foundation::stableHashCombine(key, foundation::stableHashFloat(palette.r)),
        foundation::stableHashCombine(foundation::stableHashFloat(palette.g),
                                      foundation::stableHashFloat(palette.b)));
    if (Artifact cached = find(palette_key)) {
        return cached;
    }
    const auto built = GearGenerator::build(state, fit, skeleton, palette);
    if (!built) {
        return {};
    }
    Artifact artifact = std::make_shared<GearArtifact>(built.value());
    std::scoped_lock lock(mutex_);
    const auto [iterator, inserted] = entries_.emplace(palette_key, artifact);
    return inserted ? artifact : iterator->second;
}

std::size_t GearCache::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

void GearCache::clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

} // namespace genomes::infantry
