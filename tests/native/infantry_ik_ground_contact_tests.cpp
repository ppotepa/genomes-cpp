#include <genomes/infantry/GroundContact.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <cassert>
#include <cmath>

namespace {

bool flat_surface(void*, genomes::foundation::Vec3 position,
                  genomes::infantry::GroundSample& output) noexcept {
    output.height = 0.05F + position.x * 0.02F;
    output.normal = {-0.02F, 1.0F, 0.0F};
    return true;
}

} // namespace

int main() {
    using namespace genomes::infantry;
    const auto reachable = TwoBoneIK::solve({0.0F, 1.0F, 0.0F}, {0.0F, 0.1F, 0.6F},
                                             {1.0F, 0.0F, 0.0F}, 0.6F, 0.6F);
    assert(reachable);
    assert(reachable.value().reachable);
    assert(reachable.value().residual_error < 1.0e-5F);
    const auto unreachable = TwoBoneIK::solve({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 2.0F},
                                               {1.0F, 0.0F, 0.0F}, 0.5F, 0.5F);
    assert(unreachable && !unreachable.value().reachable);
    assert(unreachable.value().residual_error > 0.0F);

    GroundContactInput input{};
    input.hips = {0.0F, 1.0F, 0.0F};
    input.left_foot = {-0.2F, 0.0F, 0.1F};
    input.right_foot = {0.2F, 0.0F, 0.1F};
    input.upper_leg_length = 0.55F;
    input.lower_leg_length = 0.55F;
    input.morphology_key = 17U;
    GroundSurfaceQuery surface{9U, nullptr, flat_surface};
    const auto contact = GroundContactSolver::solve(input, surface);
    assert(contact && contact.value().valid());
    assert(contact.value().surface_revision == 9U);
    assert(contact.value().feet[0].target_position.y > 0.0F);
    assert(contact.value().feet[0].normal.y > 0.0F);

    GroundContactCache cache;
    const auto cache_key = groundContactCacheKey(input, surface);
    cache.store(cache_key, contact.value());
    assert(cache.size() == 1U && cache.find(cache_key) != nullptr);
    surface.revision = 10U;
    assert(groundContactCacheKey(input, surface) != cache_key);
    const auto changed = GroundContactSolver::solve(input, surface);
    assert(changed && changed.value().surface_revision == 10U);
    return 0;
}
