#pragma once

#include <genomes/input/InputFrame.hpp>

#include <threepp/input/PeripheralsEventSource.hpp>

#include <array>
#include <cstdint>

namespace genomes::render {

class ThreeppInputAdapter final : public threepp::PeripheralsEventSource {
public:
    void setViewport(int left, int top, int width, int height) noexcept;
    void feed(const input::InputFrame& frame);

    [[nodiscard]] threepp::WindowSize size() const override;

private:
    [[nodiscard]] bool contains(float x, float y) const noexcept;
    [[nodiscard]] threepp::Vector2 local(float x, float y) const noexcept;

    int left_{0};
    int top_{0};
    int width_{1};
    int height_{1};
    std::array<bool, 8U> buttons_down_{};
};

} // namespace genomes::render
