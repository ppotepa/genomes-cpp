#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/WorldConfig.hpp>
#include <genomes/runtime/Scene.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace genomes::runtime {

enum class WorldConfigEntry {
    Seed,
    MapSize,
    Preset,
    Hydrology,
    Vegetation,
    Buildings,
    FencedParcels,
    Start,
    Back,
    Count
};

struct WorldConfigState final {
    WorldConfigEntry selected{WorldConfigEntry::Start};
    WorldGenerationConfig config{};
    double preview_time{0.0};
};

class WorldConfigScene final : public Scene {
public:
    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

    [[nodiscard]] const WorldConfigState& state() const noexcept {
        return state_;
    }

private:
    void adjust(int direction) noexcept;
    void activate(SceneContext&);

    WorldConfigState state_{};
    std::vector<std::shared_ptr<const render::RenderMesh>> preview_prototypes_;
};

} // namespace genomes::runtime
