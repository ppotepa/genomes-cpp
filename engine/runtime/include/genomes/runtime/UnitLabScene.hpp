#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/runtime/Scene.hpp>

#include <memory>
#include <optional>
#include <cstdint>

namespace genomes::runtime {

class UnitLabScene final : public Scene {
public:
    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

private:
    double elapsed_seconds_{0.0};
    std::uint64_t fixed_tick_{0};
    float fixed_accumulator_{0.0F};
    std::shared_ptr<const render::RenderMesh> unit_prototype_;
    infantry::InfantryModelCompiler model_compiler_;
    std::optional<infantry::InfantryModelArtifact> model_artifact_;
    std::optional<infantry::LocomotionController> locomotion_;
    std::optional<infantry::LocomotionState> locomotion_state_;
    std::optional<infantry::FaceAnimator> face_animator_;
    std::optional<infantry::AnimationSystem> animation_system_;
    std::optional<infantry::AnimationPose> animation_pose_;
};

} // namespace genomes::runtime
