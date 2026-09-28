#include <genomes/combat/AIJobPipeline.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void run(std::size_t count, std::uint32_t workers) {
    using namespace genomes;
    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    if (rifle == nullptr) {
        return;
    }
    std::vector<combat::VisibleTarget> visible{
        {{0xFFFFU, 1U}, {0.0F, 1.0F, 20.0F}, 400.0F, true}};
    std::vector<combat::AIState> states(count);
    std::vector<combat::TacticalAIEntity> entities;
    entities.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        entities.push_back({{static_cast<std::uint32_t>(index + 1U), 1U},
                            {static_cast<float>(index % 256U), 1.0F,
                             static_cast<float>(index / 256U)},
                            0.0F, rifle->id, visible, &states[index]});
    }
    jobs::JobSystem job_system(workers);
    combat::TacticalAISystem model;
    (void)model.registerDefaults();
    combat::AIJobPipeline pipeline;
    const auto begin = std::chrono::steady_clock::now();
    const auto result = pipeline.evaluate(job_system, model, entities, {7U}, {128U, true});
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "ai_pipeline count=" << count << " workers=" << workers
              << " ms=" << elapsed.count() << " intents="
              << (result ? result.value().size() : 0U) << std::endl;
}

} // namespace

int main() {
    for (const std::uint32_t workers : {1U, 2U, 4U, 8U}) {
        run(1000U, workers);
        run(10000U, workers);
        run(50000U, workers);
    }
    return 0;
}
