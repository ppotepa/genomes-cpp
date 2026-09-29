#pragma once
#include <genomes/foundation/Types.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::render {

inline constexpr std::uint16_t kInvalidRenderBoneIndex = 0xffffU;

struct BoneQuaternion final {
    float x{0.0F}, y{0.0F}, z{0.0F}, w{1.0F};
    [[nodiscard]] bool finite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }
};

struct BoneLocalTransform final {
    foundation::Vec3 translation{};
    BoneQuaternion rotation{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};

    [[nodiscard]] bool valid() const noexcept {
        const auto finite3 = [](foundation::Vec3 v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        return finite3(translation) && rotation.finite() && finite3(scale) &&
               std::abs(scale.x) > 1.0e-7F && std::abs(scale.y) > 1.0e-7F &&
               std::abs(scale.z) > 1.0e-7F;
    }
};

struct RenderSkeletonBone final {
    std::uint16_t parent{kInvalidRenderBoneIndex};
    foundation::StableId name_id{0};
    BoneLocalTransform local_bind{};
    std::array<float, 16U> inverse_bind{};
};

struct RenderSkeletonPrototype final {
    foundation::StableId skeleton_id{0};
    std::uint64_t revision{0};
    std::vector<RenderSkeletonBone> bones;

    [[nodiscard]] bool valid() const noexcept {
        if (skeleton_id == 0 || bones.empty()) return false;
        for (std::size_t index = 0; index < bones.size(); ++index) {
            const auto& bone = bones[index];
            if (!bone.local_bind.valid()) return false;
            if (bone.parent != kInvalidRenderBoneIndex && bone.parent >= index) return false;
            for (const float value : bone.inverse_bind)
                if (!std::isfinite(value)) return false;
        }
        return true;
    }
};

} // namespace genomes::render
