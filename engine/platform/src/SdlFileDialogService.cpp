#include <genomes/platform/SdlFileDialogService.hpp>

#include <SDL3/SDL.h>

#include <condition_variable>
#include <chrono>
#include <mutex>
#include <string>

namespace genomes::platform {
namespace {
struct DialogState final {
    std::mutex mutex;
    std::condition_variable completed;
    bool done{false};
    std::optional<std::string> selected;
    std::string title;
    std::string pattern;
    SDL_DialogFileFilter filter{};
};

void SDLCALL complete_dialog(void* userdata, const char* const* filelist, int) {
    auto* state = static_cast<DialogState*>(userdata);
    if (state == nullptr) return;
    {
        std::lock_guard lock{state->mutex};
        if (filelist != nullptr && filelist[0] != nullptr && filelist[0][0] != '\0')
            state->selected = std::string{filelist[0]};
        state->done = true;
    }
    state->completed.notify_one();
}

std::optional<std::string> wait_for_dialog(DialogState& state, SDL_Window* window,
                                           bool save) {
    if (!SDL_IsMainThread() || (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0U) return {};
    if (save) {
        SDL_ShowSaveFileDialog(&complete_dialog, &state, window, &state.filter, 1, nullptr);
    } else {
        SDL_ShowOpenFileDialog(&complete_dialog, &state, window, &state.filter, 1, nullptr, false);
    }

    std::unique_lock lock{state.mutex};
    while (!state.done) {
        // SDL's portal backends require event pumping while the native dialog
        // is open. Do this outside the state mutex because the callback may be
        // delivered from another thread.
        lock.unlock();
        SDL_PumpEvents();
        lock.lock();
        state.completed.wait_for(lock, std::chrono::milliseconds{2});
    }
    return state.selected;
}

std::optional<std::string> show_dialog(SDL_Window* window, std::string_view title,
                                       std::string_view filter, bool save) {
    DialogState state{};
    state.title = std::string{title.empty() ? "Genomes" : title};
    state.pattern = std::string{filter.empty() ? "*" : filter};
    state.filter = {state.title.c_str(), state.pattern.c_str()};
    return wait_for_dialog(state, window, save);
}
} // namespace

std::optional<std::string> SdlFileDialogService::open_file(std::string_view title,
                                                            std::string_view filter) {
    return show_dialog(window_, title, filter, false);
}

std::optional<std::string> SdlFileDialogService::save_file(std::string_view title,
                                                            std::string_view filter) {
    return show_dialog(window_, title, filter, true);
}

} // namespace genomes::platform
