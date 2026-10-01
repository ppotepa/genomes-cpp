#pragma once

#include <genomes/ui/UiServices.hpp>

#include <string_view>

struct SDL_Window;

namespace genomes::platform {

// Synchronous facade over SDL3's asynchronous native file dialogs. The UI
// contract deliberately stays SDL-free, while the application-owned service
// translates the callback into the optional path returned to a FilePicker.
// Calls must be made on the SDL owner thread; SDL itself remains responsible
// for the native dialog and its lifetime.
class SdlFileDialogService final : public ui::IFileDialogService {
public:
    explicit SdlFileDialogService(SDL_Window* window = nullptr) noexcept : window_{window} {}

    [[nodiscard]] std::optional<std::string> open_file(
        std::string_view title, std::string_view filter) override;
    [[nodiscard]] std::optional<std::string> save_file(
        std::string_view title, std::string_view filter) override;

    void set_window(SDL_Window* window) noexcept { window_ = window; }

private:
    SDL_Window* window_{nullptr};
};

} // namespace genomes::platform
