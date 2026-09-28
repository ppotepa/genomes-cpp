#pragma once

#include <genomes/foundation/Types.hpp>

#include <string>
#include <utility>
#include <vector>

namespace genomes::ui {

enum class UiNodeType { Panel, Label, Button, Separator };

struct UiNode {
    foundation::StableId id{0};
    UiNodeType type{UiNodeType::Label};
    std::string text;
    bool enabled{true};
    bool selected{false};
    float width{0.0F};
    float height{0.0F};
    // Optional layout in framebuffer pixels. Rendering and hit testing consume
    // exactly the same rectangle; legacy menu documents retain automatic layout.
    bool explicit_layout{false};
    float left{0.0F};
    float top{0.0F};
    float font_scale{1.5F};
};

struct UiDocument {
    std::vector<UiNode> nodes;

    void clear() { nodes.clear(); }

    UiNode& add(UiNode node) {
        nodes.push_back(std::move(node));
        return nodes.back();
    }

    [[nodiscard]] foundation::StableId hitTest(float x, float y) const noexcept {
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            const auto& node = *it;
            if (node.explicit_layout && node.enabled && node.type == UiNodeType::Button &&
                x >= node.left && x < node.left + node.width &&
                y >= node.top && y < node.top + node.height) {
                return node.id;
            }
        }
        return 0;
    }
};

} // namespace genomes::ui
