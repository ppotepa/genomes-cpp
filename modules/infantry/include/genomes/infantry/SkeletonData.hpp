#pragma once

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/RigSchema.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace genomes::infantry {

struct RigQuaternion final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};

    [[nodiscard]] static constexpr RigQuaternion identity() noexcept { return {}; }
};

struct RigTransform final {
    foundation::Vec3 translation{};
    RigQuaternion rotation{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct BoneRecord final {
    BoneId id{};
    std::string_view name{};
    std::uint16_t parent{kInvalidBoneIndex};
    BoneCategory category{BoneCategory::Body};
    RigTransform local_bind{};
    RigTransform world_bind{};
    RigTransform inverse_bind{};
    float reference_length{0.0F};
};

enum class AttachmentPointId : std::uint8_t {
    Pelvis,
    Chest,
    Head,
    Jaw,
    LeftHand,
    RightHand,
    LeftFoot,
    RightFoot,
    LeftEye,
    RightEye,
    Mouth,
};

struct AttachmentPoint final {
    AttachmentPointId id{};
    BoneId bone{};
    foundation::Vec3 local{};
    foundation::Vec3 world{};
};

class SkeletonData final {
public:
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::uint32_t schemaVersion() const noexcept { return schema_version_; }
    [[nodiscard]] foundation::StableId cacheKey() const noexcept { return cache_key_; }
    [[nodiscard]] std::span<const BoneRecord> bones() const noexcept { return bones_; }
    [[nodiscard]] std::span<const AttachmentPoint> attachments() const noexcept {
        return attachments_;
    }
    [[nodiscard]] const BoneRecord* find(BoneId id) const noexcept;
    [[nodiscard]] const AttachmentPoint* findAttachment(AttachmentPointId id) const noexcept;

private:
    friend class RigBuilder;

    std::uint32_t schema_version_{RigSchemaVersion};
    foundation::StableId cache_key_{0};
    std::vector<BoneRecord> bones_;
    std::vector<AttachmentPoint> attachments_;
};

} // namespace genomes::infantry
