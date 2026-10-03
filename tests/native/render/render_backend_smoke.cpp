#include <genomes/render/NullRenderBackend.hpp>
#include <genomes/render/RenderLane.hpp>
#include <genomes/render/RenderFrameTransaction.hpp>

#include <cassert>
#include <atomic>
#include <thread>

int main() {
    genomes::render::RenderConfig config{};
    genomes::render::NullRenderBackend backend{config};

    assert(backend.capabilities().initialized);
    assert(backend.healthState() == genomes::render::RendererHealthState::Healthy);
    assert(backend.begin_frame());
    assert(!backend.begin_frame());
    assert(backend.healthState() == genomes::render::RendererHealthState::FrameAborted);
    assert(backend.healthDiagnostics().error.code == genomes::foundation::ErrorCode::InvalidState);
    assert(backend.end_frame());
    assert(backend.healthState() == genomes::render::RendererHealthState::Healthy);
    assert(backend.healthDiagnostics().last_successful_frame == 1);
    assert(backend.healthDiagnostics().last_present_frame == 1);
    assert(!backend.end_frame());
    assert(backend.wait_idle());

    backend.shutdown();
    assert(!backend.capabilities().initialized);
    assert(backend.healthState() == genomes::render::RendererHealthState::Stopped);
    assert(!backend.begin_frame());
    assert(backend.healthDiagnostics().error.code == genomes::foundation::ErrorCode::InvalidState);
    assert(!backend.wait_idle());
    assert(backend.healthDiagnostics().error.code == genomes::foundation::ErrorCode::InvalidState);

    genomes::render::RenderLane lane;
    assert(lane.requireOwner());
    std::atomic_bool rejected{false};
    std::thread foreign([&] { rejected.store(!lane.requireOwner(), std::memory_order_release); });
    foreign.join();
    assert(rejected.load(std::memory_order_acquire));

    genomes::render::RenderFrameTransaction transaction;
    int presents = 0;
    int aborts = 0;
    transaction.begin([] {
        return genomes::render::RenderResult::success();
    });
    transaction.submit([] {
        return genomes::render::RenderResult::failure(
            {genomes::foundation::ErrorCode::Internal, "synthetic frame failure"});
    });
    transaction.end([&] {
        ++presents;
        return genomes::render::RenderResult::success();
    }, [&] {
        ++aborts;
        return genomes::render::RenderResult::success();
    });
    assert(!transaction.healthy());
    assert(!transaction.open());
    assert(presents == 0);
    assert(aborts == 1);

    return 0;
}
