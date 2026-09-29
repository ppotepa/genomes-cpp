#pragma once

#include <genomes/ui/UiRuntime.hpp>

#include <RmlUi/Core.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace genomes::ui::rml {

class RenderAdapter final : public Rml::RenderInterface {
public:
    explicit RenderAdapter(UiRenderFrame& frame) noexcept : frame_{frame} {}

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
                                   const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                       Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override { scissor_enabled_ = enable; }
    void SetScissorRegion(Rml::Rectanglei region) override { scissor_ = region; }

private:
    struct Geometry final {
        std::vector<Rml::Vertex> vertices;
        std::vector<int> indices;
    };

    UiRenderFrame& frame_;
    std::unordered_map<Rml::CompiledGeometryHandle, Geometry> geometries_;
    Rml::Rectanglei scissor_{};
    Rml::CompiledGeometryHandle next_geometry_{1};
    Rml::TextureHandle next_texture_{1};
    bool scissor_enabled_{false};
};

class FileAdapter final : public Rml::FileInterface {
public:
    explicit FileAdapter(std::filesystem::path root) : root_{std::move(root)} {}

    Rml::FileHandle Open(const Rml::String& path) override;
    void Close(Rml::FileHandle file) override;
    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
    bool Seek(Rml::FileHandle file, long offset, int origin) override;
    size_t Tell(Rml::FileHandle file) override;

private:
    std::filesystem::path root_;
};

class SystemAdapter final : public Rml::SystemInterface {
public:
    SystemAdapter() : start_{std::chrono::steady_clock::now()} {}
    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

private:
    std::chrono::steady_clock::time_point start_;
};

class ActionListener final : public Rml::EventListener {
public:
    explicit ActionListener(IUiActionRouter* router = nullptr) noexcept : router_{router} {}
    void set_router(IUiActionRouter* router) noexcept { router_ = router; }
    [[nodiscard]] UiActionResult dispatch(UiActionId action,
                                          const UiActionArguments& arguments = {}) const;
    void ProcessEvent(Rml::Event& event) override;

private:
    IUiActionRouter* router_{nullptr};
};

class Runtime final {
public:
    Runtime(std::filesystem::path asset_root, std::uint32_t width, std::uint32_t height);
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    [[nodiscard]] bool valid() const noexcept { return context_ != nullptr; }
    [[nodiscard]] Rml::Context* context() noexcept { return context_; }
    [[nodiscard]] const UiRenderFrame& update(double delta_seconds);
    [[nodiscard]] UiRenderFrame& frame() noexcept { return frame_; }
    // Dispatch each raw event to RmlUi and return only events that still belong
    // to the scene/presentation layer. Release events are always forwarded so
    // interrupted drags cannot leave controls stuck.
    [[nodiscard]] input::InputFrame filter_input(const input::InputFrame& input);
    bool process_input(const input::InputFrame& input);
    void set_action_router(IUiActionRouter* router) noexcept;
    bool bind_text(std::string name, std::string* value);
    bool load_document(const std::filesystem::path& relative_path);
    bool push_document(const std::filesystem::path& relative_path);
    void unload_documents();
    void resize(std::uint32_t width, std::uint32_t height);

private:
    [[nodiscard]] bool process_event(const input::Event& event);
    static void accumulate_event(input::InputFrame& frame, const input::Event& event);

    RenderAdapter renderer_;
    FileAdapter files_;
    SystemAdapter system_;
    ActionListener action_listener_;
    Rml::Context* context_{nullptr};
    Rml::DataModelConstructor data_model_{};
    std::string status_{"Preparing presentation..."};
    std::string selected_{"No selection"};
    std::string seed_{"Seed: 0"};
    UiRenderFrame frame_{};
    bool ui_left_capture_{false};
};

} // namespace genomes::ui::rml
