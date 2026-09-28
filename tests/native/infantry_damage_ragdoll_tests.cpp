#include <genomes/infantry/InfantryDamage.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RagdollSchema.hpp>
#include <genomes/infantry/RigBuilder.hpp>
#include <genomes/physics/PhysicsWorld.hpp>

#include <cassert>
#include <cmath>
#include <vector>

int main() {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(0xDA6E5U, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(rig);

    const genomes::simulation::EntityId entity{7U, 1U};
    auto volumes_result = InfantryDamageModel::buildVolumes(entity, rig.value());
    assert(volumes_result && volumes_result.value().size() == InfantryDamageModel::recipes().size());
    std::vector<HitVolume> volumes = std::move(volumes_result.value());

    AnimationPose previous{};
    previous.semantic_id = 7U;
    previous.evaluated = true;
    previous.locomotion_phase = 0.2F;
    AnimationPose current = previous;
    current.root_position = {0.1F, 0.0F, 0.0F};
    for (std::size_t index = 0U; index < rig.value().bones().size(); ++index) {
        previous.bones[index] = rig.value().bones()[index].local_bind;
        current.bones[index] = rig.value().bones()[index].local_bind;
    }
    assert(previous.valid() && current.valid());
    assert(InfantryDamageModel::updateVolumes(volumes, current, rig.value(), 4U));
    for (const HitVolume& volume : volumes) {
        assert(volume.valid() && volume.pose_revision == 4U);
    }

    genomes::spatial::UniformGrid grid(2.0F);
    HitVolumeBatcher batcher;
    DamageBatchItem item{entity, volumes, &current, &rig.value(), 4U};
    const auto report = batcher.update(std::span<DamageBatchItem>(&item, 1U), grid, 4U);
    assert(report && report.value().volumes_updated == volumes.size());
    std::vector<genomes::simulation::EntityId> candidates;
    grid.queryRadius(current.root_position, 2.0F, candidates);
    assert(candidates.size() == 1U && candidates.front() == entity);

    float health = 100.0F;
    const auto damage = InfantryDamageModel::apply(
        health, 100.0F, {DamageRegion::Head, 50.0F, 0.0F, 4U});
    assert(damage && damage.value().applied > 0.0F && health < 100.0F);

    const auto schema = RagdollSchema::build(phenotype.value().body, rig.value());
    assert(schema && schema.value().valid() && !schema.value().bodies().empty());
    RagdollCache cache;
    cache.store(schema.value());
    assert(cache.size() == 1U && cache.find(schema.value().cacheKey()) != nullptr);

    genomes::physics::SimplePhysicsWorld physics;
    RagdollController ragdoll;
    const auto entered = ragdoll.enter(physics, schema.value(), previous, current, 1.0F / 60.0F, 4U);
    assert(entered && ragdoll.active());
    assert(physics.bodyCount() == schema.value().bodies().size());
    assert(std::isfinite(ragdoll.state().linear_velocities.front().x));
    ragdoll.dispose(physics);
    assert(!ragdoll.active() && physics.bodyCount() == 0U);
    return 0;
}
