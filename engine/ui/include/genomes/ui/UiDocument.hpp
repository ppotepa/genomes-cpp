#pragma once

#include <genomes/foundation/Types.hpp>

#include <string>
#include <utility>
#include <vector>

namespace genomes::ui {

enum class UiNodeType {
    Panel,
    Label,
    Button,
    Separator
};

struct UiNode {
    foundation::StableId id{0};
    UiNodeType type{UiNodeType::Label};
    std::string text;
    bool enabled{true};
    bool selected{false};
    float width{0.0F};
    float height{0.0F};
};

struct UiDocument {
    std::vector<UiNode> nodes;

    void clear() {
        nodes.clear();
    }

    UiNode& add(UiNode node) {
        nodes.push_back(std::move(node));
        return nodes.back();
    }
};

} // namespace genomes::ui
