#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace genomes::infantry {

enum class BoneCategory : std::uint8_t {
    Body,
    FaceController,
    Finger,
};

// BoneId values are the canonical palette order. Do not insert an item in the
// middle of this enum without bumping the rig schema version.
enum class BoneId : std::uint16_t {
    Hips,
    SpineLower,
    SpineUpper,
    Chest,
    Neck,
    Head,
    ClavicleL,
    UpperArmL,
    ForeArmL,
    HandL,
    ThighL,
    ShinL,
    FootL,
    ToesL,
    ClavicleR,
    UpperArmR,
    ForeArmR,
    HandR,
    ThighR,
    ShinR,
    FootR,
    ToesR,
    Jaw,
    MouthUpper,
    MouthLower,
    EyeL,
    LidUpperL,
    LidLowerL,
    BrowInnerL,
    BrowOuterL,
    MouthCornerL,
    CheekL,
    EyeR,
    LidUpperR,
    LidLowerR,
    BrowInnerR,
    BrowOuterR,
    MouthCornerR,
    CheekR,
    FingerLLittle0,
    FingerLLittle1,
    FingerLLittle2,
    FingerLRing0,
    FingerLRing1,
    FingerLRing2,
    FingerLMiddle0,
    FingerLMiddle1,
    FingerLMiddle2,
    FingerLIndex0,
    FingerLIndex1,
    FingerLIndex2,
    FingerLThumb0,
    FingerLThumb1,
    FingerLThumb2,
    FingerRIndex0,
    FingerRIndex1,
    FingerRIndex2,
    FingerRMiddle0,
    FingerRMiddle1,
    FingerRMiddle2,
    FingerRRing0,
    FingerRRing1,
    FingerRRing2,
    FingerRLittle0,
    FingerRLittle1,
    FingerRLittle2,
    FingerRThumb0,
    FingerRThumb1,
    FingerRThumb2,
};

inline constexpr std::size_t kRigBoneCount = 69U;
inline constexpr std::size_t kBodyBoneCount = 22U;
inline constexpr std::size_t kFaceBoneCount = 17U;
inline constexpr std::size_t kFingerBoneCount = 30U;
inline constexpr std::uint32_t RigSchemaVersion = 1U;
inline constexpr std::uint16_t kInvalidBoneIndex = std::numeric_limits<std::uint16_t>::max();

struct BoneSchema final {
    BoneId id{};
    std::string_view name{};
    std::uint16_t parent{kInvalidBoneIndex};
    BoneCategory category{BoneCategory::Body};
};

inline constexpr BoneSchema kCanonicalRigSchema[kRigBoneCount]{
    {BoneId::Hips, "hips", kInvalidBoneIndex, BoneCategory::Body},
    {BoneId::SpineLower, "spineLower", 0U, BoneCategory::Body},
    {BoneId::SpineUpper, "spineUpper", 1U, BoneCategory::Body},
    {BoneId::Chest, "chest", 2U, BoneCategory::Body},
    {BoneId::Neck, "neck", 3U, BoneCategory::Body},
    {BoneId::Head, "head", 4U, BoneCategory::Body},
    {BoneId::ClavicleL, "clavicle.L", 3U, BoneCategory::Body},
    {BoneId::UpperArmL, "upperArm.L", 6U, BoneCategory::Body},
    {BoneId::ForeArmL, "foreArm.L", 7U, BoneCategory::Body},
    {BoneId::HandL, "hand.L", 8U, BoneCategory::Body},
    {BoneId::ThighL, "thigh.L", 0U, BoneCategory::Body},
    {BoneId::ShinL, "shin.L", 10U, BoneCategory::Body},
    {BoneId::FootL, "foot.L", 11U, BoneCategory::Body},
    {BoneId::ToesL, "toes.L", 12U, BoneCategory::Body},
    {BoneId::ClavicleR, "clavicle.R", 3U, BoneCategory::Body},
    {BoneId::UpperArmR, "upperArm.R", 14U, BoneCategory::Body},
    {BoneId::ForeArmR, "foreArm.R", 15U, BoneCategory::Body},
    {BoneId::HandR, "hand.R", 16U, BoneCategory::Body},
    {BoneId::ThighR, "thigh.R", 0U, BoneCategory::Body},
    {BoneId::ShinR, "shin.R", 18U, BoneCategory::Body},
    {BoneId::FootR, "foot.R", 19U, BoneCategory::Body},
    {BoneId::ToesR, "toes.R", 20U, BoneCategory::Body},
    {BoneId::Jaw, "jaw", 5U, BoneCategory::FaceController},
    {BoneId::MouthUpper, "mouthUpper", 5U, BoneCategory::FaceController},
    {BoneId::MouthLower, "mouthLower", 22U, BoneCategory::FaceController},
    {BoneId::EyeL, "eye.L", 5U, BoneCategory::FaceController},
    {BoneId::LidUpperL, "lidUpper.L", 5U, BoneCategory::FaceController},
    {BoneId::LidLowerL, "lidLower.L", 5U, BoneCategory::FaceController},
    {BoneId::BrowInnerL, "browInner.L", 5U, BoneCategory::FaceController},
    {BoneId::BrowOuterL, "browOuter.L", 5U, BoneCategory::FaceController},
    {BoneId::MouthCornerL, "mouthCorner.L", 5U, BoneCategory::FaceController},
    {BoneId::CheekL, "cheek.L", 5U, BoneCategory::FaceController},
    {BoneId::EyeR, "eye.R", 5U, BoneCategory::FaceController},
    {BoneId::LidUpperR, "lidUpper.R", 5U, BoneCategory::FaceController},
    {BoneId::LidLowerR, "lidLower.R", 5U, BoneCategory::FaceController},
    {BoneId::BrowInnerR, "browInner.R", 5U, BoneCategory::FaceController},
    {BoneId::BrowOuterR, "browOuter.R", 5U, BoneCategory::FaceController},
    {BoneId::MouthCornerR, "mouthCorner.R", 5U, BoneCategory::FaceController},
    {BoneId::CheekR, "cheek.R", 5U, BoneCategory::FaceController},
    {BoneId::FingerLLittle0, "finger.L.little.0", 9U, BoneCategory::Finger},
    {BoneId::FingerLLittle1, "finger.L.little.1", 39U, BoneCategory::Finger},
    {BoneId::FingerLLittle2, "finger.L.little.2", 40U, BoneCategory::Finger},
    {BoneId::FingerLRing0, "finger.L.ring.0", 9U, BoneCategory::Finger},
    {BoneId::FingerLRing1, "finger.L.ring.1", 42U, BoneCategory::Finger},
    {BoneId::FingerLRing2, "finger.L.ring.2", 43U, BoneCategory::Finger},
    {BoneId::FingerLMiddle0, "finger.L.middle.0", 9U, BoneCategory::Finger},
    {BoneId::FingerLMiddle1, "finger.L.middle.1", 45U, BoneCategory::Finger},
    {BoneId::FingerLMiddle2, "finger.L.middle.2", 46U, BoneCategory::Finger},
    {BoneId::FingerLIndex0, "finger.L.index.0", 9U, BoneCategory::Finger},
    {BoneId::FingerLIndex1, "finger.L.index.1", 48U, BoneCategory::Finger},
    {BoneId::FingerLIndex2, "finger.L.index.2", 49U, BoneCategory::Finger},
    {BoneId::FingerLThumb0, "finger.L.thumb.0", 9U, BoneCategory::Finger},
    {BoneId::FingerLThumb1, "finger.L.thumb.1", 51U, BoneCategory::Finger},
    {BoneId::FingerLThumb2, "finger.L.thumb.2", 52U, BoneCategory::Finger},
    {BoneId::FingerRIndex0, "finger.R.index.0", 17U, BoneCategory::Finger},
    {BoneId::FingerRIndex1, "finger.R.index.1", 54U, BoneCategory::Finger},
    {BoneId::FingerRIndex2, "finger.R.index.2", 55U, BoneCategory::Finger},
    {BoneId::FingerRMiddle0, "finger.R.middle.0", 17U, BoneCategory::Finger},
    {BoneId::FingerRMiddle1, "finger.R.middle.1", 57U, BoneCategory::Finger},
    {BoneId::FingerRMiddle2, "finger.R.middle.2", 58U, BoneCategory::Finger},
    {BoneId::FingerRRing0, "finger.R.ring.0", 17U, BoneCategory::Finger},
    {BoneId::FingerRRing1, "finger.R.ring.1", 60U, BoneCategory::Finger},
    {BoneId::FingerRRing2, "finger.R.ring.2", 61U, BoneCategory::Finger},
    {BoneId::FingerRLittle0, "finger.R.little.0", 17U, BoneCategory::Finger},
    {BoneId::FingerRLittle1, "finger.R.little.1", 63U, BoneCategory::Finger},
    {BoneId::FingerRLittle2, "finger.R.little.2", 64U, BoneCategory::Finger},
    {BoneId::FingerRThumb0, "finger.R.thumb.0", 17U, BoneCategory::Finger},
    {BoneId::FingerRThumb1, "finger.R.thumb.1", 66U, BoneCategory::Finger},
    {BoneId::FingerRThumb2, "finger.R.thumb.2", 67U, BoneCategory::Finger},
};

static_assert(static_cast<std::size_t>(BoneId::FingerRThumb2) + 1U == kRigBoneCount);

[[nodiscard]] constexpr std::size_t boneIndex(BoneId id) noexcept {
    return static_cast<std::size_t>(id);
}

[[nodiscard]] constexpr std::span<const BoneSchema> rigSchema() noexcept {
    return {kCanonicalRigSchema, kRigBoneCount};
}

} // namespace genomes::infantry
