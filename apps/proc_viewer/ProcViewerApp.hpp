#pragma once

#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace genomes::proc_viewer {

enum class ViewerMode : std::uint8_t {
    TerrainHydrology,
    RoadsCity,
    Buildings,
    Vegetation,
    DestructionBallistics,
    InfantryAnimation,
    WorldDiagnostics
};

struct ViewerTraceEntry final {
    ViewerMode mode{ViewerMode::WorldDiagnostics};
    std::uint64_t content_hash{0U};
    std::chrono::nanoseconds duration{};
    bool validation_passed{false};
};

struct ViewerArtifact final {
    gameplay::WorldSemanticSnapshot semantic{};
    ViewerMode mode{ViewerMode::WorldDiagnostics};
    std::uint64_t generation{0U};
};

class ProcViewerApp final {
public:
    explicit ProcViewerApp(std::uint32_t workers = 2U);

    [[nodiscard]] static std::span<const ViewerMode> modes() noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> selectMode(ViewerMode mode) noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> regenerate(
        const world::WorldGenerationRequest& request);
    [[nodiscard]] foundation::Result<bool, foundation::Error> poll();
    void cancel() noexcept;

    [[nodiscard]] const ViewerArtifact* artifact() const noexcept {
        return artifact_ ? &*artifact_ : nullptr;
    }
    [[nodiscard]] ViewerMode mode() const noexcept { return mode_; }
    [[nodiscard]] const foundation::Error& lastError() const noexcept { return last_error_; }
    [[nodiscard]] std::span<const ViewerTraceEntry> trace() const noexcept { return trace_; }

private:
    jobs::JobSystem jobs_;
    gameplay::WorldScenario scenario_;
    ViewerMode mode_{ViewerMode::WorldDiagnostics};
    std::optional<ViewerArtifact> artifact_;
    std::vector<ViewerTraceEntry> trace_;
    std::chrono::steady_clock::time_point generation_started_{};
    std::uint64_t generation_{0U};
    foundation::Error last_error_{};
};

} // namespace genomes::proc_viewer
