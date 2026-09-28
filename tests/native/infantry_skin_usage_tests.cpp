#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    const std::array<genomes::infantry::SkinInfluence, 6U> candidates{{
        {3U, 0.25F}, {2U, 0.10F}, {3U, 0.75F}, {9U, 0.40F},
        {11U, -1.0F}, {12U, 0.0F}}};
    const auto normalized = genomes::infantry::normalizeTopFour(candidates);
    assert(normalized.count == 3U);
    assert(normalized.values[0].bone_index == 3U);
    float normalized_sum = 0.0F;
    for (std::size_t index = 0U; index < normalized.count; ++index) {
        assert(normalized.values[index].weight > 0.0F);
        normalized_sum += normalized.values[index].weight;
    }
    assert(std::abs(normalized_sum - 1.0F) < 1.0e-5F);

    genomes::infantry::InfantryModelCompiler compiler;
    genomes::infantry::InfantryModelRequest request{};
    request.seed = 0x5EED2026U;
    const auto model = compiler.compile(request);
    assert(model);
    std::array<std::uint32_t, genomes::infantry::kRigBoneCount> usage{};
    for (const auto* mesh : {&model.value().appearance.body, &model.value().appearance.hair}) {
        for (const auto& vertex : mesh->vertices) {
            for (std::size_t index = 0U; index < vertex.influence_count; ++index) {
                if (vertex.influences[index].weight > 0.0F &&
                    vertex.influences[index].bone_index < usage.size()) {
                    ++usage[vertex.influences[index].bone_index];
                }
            }
        }
    }
    const auto require_used = [&usage](genomes::infantry::BoneId bone) {
        assert(usage[genomes::infantry::boneIndex(bone)] > 0U);
    };
    for (const auto bone : {genomes::infantry::BoneId::Hips,
                            genomes::infantry::BoneId::SpineLower,
                            genomes::infantry::BoneId::SpineUpper,
                            genomes::infantry::BoneId::Chest,
                            genomes::infantry::BoneId::Neck,
                            genomes::infantry::BoneId::Head,
                            genomes::infantry::BoneId::Jaw,
                            genomes::infantry::BoneId::ClavicleL,
                            genomes::infantry::BoneId::ClavicleR,
                            genomes::infantry::BoneId::EyeL,
                            genomes::infantry::BoneId::EyeR,
                            genomes::infantry::BoneId::UpperArmL,
                            genomes::infantry::BoneId::UpperArmR,
                            genomes::infantry::BoneId::ForeArmL,
                            genomes::infantry::BoneId::ForeArmR,
                            genomes::infantry::BoneId::HandL,
                            genomes::infantry::BoneId::HandR,
                            genomes::infantry::BoneId::ThighL,
                            genomes::infantry::BoneId::ThighR,
                            genomes::infantry::BoneId::ShinL,
                            genomes::infantry::BoneId::ShinR,
                            genomes::infantry::BoneId::FootL,
                            genomes::infantry::BoneId::FootR}) {
        require_used(bone);
    }
    const std::array finger_roots{
        genomes::infantry::BoneId::FingerLLittle0,
        genomes::infantry::BoneId::FingerLRing0,
        genomes::infantry::BoneId::FingerLMiddle0,
        genomes::infantry::BoneId::FingerLIndex0,
        genomes::infantry::BoneId::FingerLThumb0,
        genomes::infantry::BoneId::FingerRIndex0,
        genomes::infantry::BoneId::FingerRMiddle0,
        genomes::infantry::BoneId::FingerRRing0,
        genomes::infantry::BoneId::FingerRLittle0,
        genomes::infantry::BoneId::FingerRThumb0};
    for (const auto root : finger_roots) {
        for (std::size_t segment = 0U; segment < 3U; ++segment) {
            require_used(static_cast<genomes::infantry::BoneId>(
                genomes::infantry::boneIndex(root) + segment));
        }
    }
    return 0;
}
