#include "ProcViewerApp.hpp"

#include <algorithm>
#include <array>

namespace genomes::proc_viewer {

namespace {

constexpr std::array<ViewerMode, 7> kModes{
    ViewerMode::TerrainHydrology, ViewerMode::RoadsCity, ViewerMode::Buildings,
    ViewerMode::Vegetation, ViewerMode::DestructionBallistics,
    ViewerMode::InfantryAnimation, ViewerMode::WorldDiagnostics};

} // namespace

ProcViewerApp::ProcViewerApp(std::uint32_t workers) : jobs_(workers), scenario_(jobs_) {}

std::span<const ViewerMode> ProcViewerApp::modes() noexcept { return kModes; }

foundation::Result<void, foundation::Error> ProcViewerApp::selectMode(ViewerMode mode) noexcept {
    if (std::find(kModes.begin(), kModes.end(), mode) == kModes.end()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "unknown procedural viewer mode"});
    }
    mode_ = mode;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ProcViewerApp::regenerate(
    const world::WorldGenerationRequest& request) {
    const auto result = scenario_.requestNew(request);
    if (!result) {
        last_error_ = result.error();
        return result;
    }
    generation_started_ = std::chrono::steady_clock::now();
    last_error_ = {};
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<bool, foundation::Error> ProcViewerApp::poll() {
    const auto result = scenario_.poll();
    if (!result) {
        last_error_ = result.error();
        return foundation::Result<bool, foundation::Error>::failure(last_error_);
    }
    if (!result.value()) {
        return foundation::Result<bool, foundation::Error>::success(false);
    }
    const gameplay::WorldSemanticSnapshot semantic = scenario_.semanticSnapshot();
    if (!semantic.valid()) {
        last_error_ = {foundation::ErrorCode::Internal, "viewer candidate validation failed"};
        return foundation::Result<bool, foundation::Error>::failure(last_error_);
    }
    ++generation_;
    artifact_ = ViewerArtifact{semantic, mode_, generation_};
    trace_.push_back({mode_, semantic.content_hash,
                      std::chrono::steady_clock::now() - generation_started_, true});
    last_error_ = {};
    return foundation::Result<bool, foundation::Error>::success(true);
}

void ProcViewerApp::cancel() noexcept { scenario_.cancelPending(); }

} // namespace genomes::proc_viewer
