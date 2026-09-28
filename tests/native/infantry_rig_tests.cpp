#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cassert>
#include <cmath>

namespace {

[[nodiscard]] bool near(float left, float right, float epsilon = 1.0e-5F) {
    return std::abs(left - right) <= epsilon;
}

} // namespace

int main() {
    const auto genome = genomes::infantry::InfantryGenome::generate(0x12345678U, 1.0F);
    assert(genome);
    const auto phenotype = genomes::infantry::PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto rig = genomes::infantry::RigBuilder::build(phenotype.value().body,
                                                           phenotype.value().face);
    assert(rig);
    assert(rig.value().valid());
    assert(rig.value().bones().size() == genomes::infantry::kRigBoneCount);

    std::size_t body_count = 0U;
    std::size_t face_count = 0U;
    std::size_t finger_count = 0U;
    for (const auto& bone : rig.value().bones()) {
        switch (bone.category) {
        case genomes::infantry::BoneCategory::Body:
            ++body_count;
            break;
        case genomes::infantry::BoneCategory::FaceController:
            ++face_count;
            break;
        case genomes::infantry::BoneCategory::Finger:
            ++finger_count;
            break;
        }
        if (bone.parent != genomes::infantry::kInvalidBoneIndex) {
            assert(bone.parent < genomes::infantry::boneIndex(bone.id));
            assert(bone.reference_length > 0.0F);
        }
        assert(near(bone.world_bind.rotation.w, 1.0F));
        assert(near(bone.inverse_bind.translation.x, -bone.world_bind.translation.x));
        assert(near(bone.inverse_bind.translation.y, -bone.world_bind.translation.y));
        assert(near(bone.inverse_bind.translation.z, -bone.world_bind.translation.z));
    }
    assert(body_count == genomes::infantry::kBodyBoneCount);
    assert(face_count == genomes::infantry::kFaceBoneCount);
    assert(finger_count == genomes::infantry::kFingerBoneCount);

    const auto* left_index = rig.value().find(genomes::infantry::BoneId::FingerLIndex0);
    const auto* right_index = rig.value().find(genomes::infantry::BoneId::FingerRIndex0);
    assert(left_index != nullptr && right_index != nullptr);
    assert(left_index->parent == genomes::infantry::boneIndex(genomes::infantry::BoneId::HandL));
    assert(right_index->parent == genomes::infantry::boneIndex(genomes::infantry::BoneId::HandR));
    assert(rig.value().findAttachment(genomes::infantry::AttachmentPointId::LeftHand) != nullptr);
    assert(rig.value().findAttachment(genomes::infantry::AttachmentPointId::Head) != nullptr);

    const auto second = genomes::infantry::RigBuilder::build(phenotype.value().body,
                                                              phenotype.value().face);
    assert(second && second.value().cacheKey() == rig.value().cacheKey());
    return 0;
}
