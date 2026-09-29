#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/ui/UiTypes.hpp>

#include <cstdint>
#include <memory>

namespace genomes::render {

struct ThreeppUiStats final {
    std::uint64_t texture_uploads{0};
    std::uint64_t texture_upload_bytes{0};
    std::uint64_t buffer_grows{0};
    std::uint64_t draw_calls{0};
};

class ThreeppUiPass final {
public:
    ThreeppUiPass();
    ~ThreeppUiPass();
    ThreeppUiPass(const ThreeppUiPass&) = delete;
    ThreeppUiPass& operator=(const ThreeppUiPass&) = delete;

    [[nodiscard]] foundation::Result<void, foundation::Error> initialize() noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> draw(
        const ui::UiRenderFrame&, std::uint32_t width, std::uint32_t height) noexcept;
    [[nodiscard]] ThreeppUiStats lastStats() const noexcept;
    void shutdown() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace genomes::render
