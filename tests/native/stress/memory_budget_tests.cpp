#include <genomes/foundation/MemoryTelemetry.hpp>

#include <cassert>

using namespace genomes::foundation;

int main() {
    MemoryBudgetProfile profile = MemoryBudgetProfile::defaults();
    profile.profileId = "memory-budget-test";
    profile.cpu = MemoryBudget::limits(128U, 192U, 512U);
    profile.vram = MemoryBudget::limits(256U, 384U, 768U);
    profile.transient = MemoryBudget::limits(32U, 64U, 128U);
    profile.cache = MemoryBudget::limits(128U, 192U, 256U);
    profile.setBudget(MemoryCategory::GenerationScratch,
                      MemoryBudget::limits(32U, 64U, 128U));

    MemoryTelemetry telemetry(profile);

    assert(telemetry.acquire(MemoryCategory::PersistentSemantic, 64U));
    MemorySnapshot first = telemetry.snapshot();
    assert(first.category(MemoryCategory::PersistentSemantic).currentBytes ==
           64U);
    assert(first.cpu.currentBytes == 64U);
    assert(first.cpu.highWaterBytes == 64U);

    assert(telemetry.release(MemoryCategory::PersistentSemantic, 64U));
    MemorySnapshot reclaimed = telemetry.snapshot();
    assert(reclaimed.cpu.currentBytes == 0U);
    assert(reclaimed.cpu.highWaterBytes == 64U);

    // Scratch is transient and has an independently checked high-water mark.
    assert(telemetry.acquire(MemoryCategory::GenerationScratch, 80U));
    MemoryBudgetReport warning = telemetry.checkBudgets();
    assert(warning.warningExceeded);
    assert(!warning.hardLimitExceeded);
    assert(warning.violationCount != 0U);

    // Driver-reported and estimated VRAM are kept distinct. CPU bytes never
    // enter either VRAM counter.
    assert(telemetry.acquire(MemoryCategory::GpuBuffers, 100U,
                             MemoryLocation::Vram, MemoryLifetime::Persistent,
                             MemorySource::Estimated));
    assert(telemetry.acquire(MemoryCategory::GpuTextures, 120U,
                             MemoryLocation::Vram, MemoryLifetime::Persistent,
                             MemorySource::DriverReported));
    const MemorySnapshot gpu = telemetry.snapshot();
    assert(gpu.vram.currentBytes == 220U);
    assert(gpu.vramEstimated.currentBytes == 100U);
    assert(gpu.vramDriverReported.currentBytes == 120U);
    assert(gpu.cpu.currentBytes == 80U);

    assert(telemetry.release(MemoryCategory::GenerationScratch, 80U));
    assert(telemetry.release(MemoryCategory::GpuBuffers, 100U,
                             MemoryLocation::Vram, MemoryLifetime::Persistent,
                             MemorySource::Estimated));
    assert(telemetry.release(MemoryCategory::GpuTextures, 120U,
                             MemoryLocation::Vram, MemoryLifetime::Persistent,
                             MemorySource::DriverReported));

    // A release without ownership is reported and never makes a counter
    // negative; this is the development underflow/double-release guard.
    assert(!telemetry.release(MemoryCategory::GpuTextures, 1U,
                              MemoryLocation::Vram,
                              MemoryLifetime::Persistent,
                              MemorySource::DriverReported));
    assert(telemetry.snapshot().underflowCount == 1U);

    const MemorySnapshot baseline = telemetry.snapshot();
    {
        MemoryLease scratch = telemetry.lease(MemoryCategory::GenerationScratch,
                                               16U);
        assert(scratch.active());
        assert(telemetry.snapshot().transient.currentBytes == 16U);
    }
    assert(telemetry.snapshot().transient.currentBytes == 0U);

    assert(telemetry.acquire(MemoryCategory::GenerationScratch, 8U));
    const MemoryRegressionReport transientLeak =
        telemetry.checkRegression(baseline, telemetry.snapshot());
    assert(!transientLeak.passed);
    assert(transientLeak.leakDetected());
    assert(telemetry.release(MemoryCategory::GenerationScratch, 8U));

    // Cache growth is an explicit allowance, not an invisible leak.
    const MemorySnapshot cacheBaseline = telemetry.snapshot();
    assert(telemetry.acquire(MemoryCategory::ArtifactCache, 24U));
    const MemoryRegressionReport cacheGrowth = telemetry.checkForLeaks(
        cacheBaseline, telemetry.snapshot(), {0U, 0U, 32U});
    assert(cacheGrowth.passed);
    assert(cacheGrowth.cacheWithinAllowance);
    assert(telemetry.release(MemoryCategory::ArtifactCache, 24U));

    telemetry.resetHighWater();
    assert(telemetry.snapshot().cpu.highWaterBytes == 0U);
    assert(telemetry.snapshot().transient.highWaterBytes == 0U);
    return 0;
}
