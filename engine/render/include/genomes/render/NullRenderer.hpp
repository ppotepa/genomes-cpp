#pragma once

#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderTypes.hpp>

#include <cstddef>

namespace genomes::render {

// A headless renderer used by smoke applications and scene tests. The real
// Diligent adapter can consume exactly the same PresentationSnapshot.
class NullRenderer final : public IRenderer {
public:
    void begin_frame() override {
        ++frames_started_;
    }

    void submit(const PresentationSnapshot& snapshot,
                const ui::UiDocument& document) override {
        submitted_instances_ += snapshot.instances.size();
        submitted_ui_nodes_ += document.nodes.size();
    }

    void end_frame() override {}

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override {
        return {RenderBackendKind::Null, true, true, false, false};
    }

    [[nodiscard]] std::size_t frames_started() const noexcept {
        return frames_started_;
    }

    [[nodiscard]] std::size_t submitted_instances() const noexcept {
        return submitted_instances_;
    }

    [[nodiscard]] std::size_t submitted_ui_nodes() const noexcept {
        return submitted_ui_nodes_;
    }

private:
    std::size_t frames_started_{0};
    std::size_t submitted_instances_{0};
    std::size_t submitted_ui_nodes_{0};
};

} // namespace genomes::render
