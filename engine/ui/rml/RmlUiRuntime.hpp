#pragma once
#include <genomes/ui/UiRuntime.hpp>
#include <RmlUi/Core.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::ui::rml {
class RenderAdapter final : public Rml::RenderInterface {
public:
    explicit RenderAdapter(UiRenderFrame& frame) noexcept : frame_{frame} {}
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>) override;
    void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i&,const Rml::String&) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i) override;
    void ReleaseTexture(Rml::TextureHandle) override;
    void EnableScissorRegion(bool enabled) override { scissor_enabled_=enabled; }
    void SetScissorRegion(Rml::Rectanglei region) override { scissor_=region; }
private:
    struct Geometry final {std::vector<Rml::Vertex> vertices;std::vector<int> indices;};
    UiRenderFrame& frame_;
    std::unordered_map<Rml::CompiledGeometryHandle,Geometry> geometries_;
    Rml::Rectanglei scissor_{};
    Rml::CompiledGeometryHandle next_geometry_{1};
    Rml::TextureHandle next_texture_{1};
    bool scissor_enabled_{false};
};
class FileAdapter final : public Rml::FileInterface {
public:
    explicit FileAdapter(std::filesystem::path root):root_(std::move(root)) {}
    Rml::FileHandle Open(const Rml::String&) override;
    void Close(Rml::FileHandle) override;
    size_t Read(void*,size_t,Rml::FileHandle) override;
    bool Seek(Rml::FileHandle,long,int) override;
    size_t Tell(Rml::FileHandle) override;
private:
    std::filesystem::path root_;
};
class SystemAdapter final : public Rml::SystemInterface {
public:
    SystemAdapter():start_(std::chrono::steady_clock::now()) {}
    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type,const Rml::String&) override;
private:
    std::chrono::steady_clock::time_point start_;
};
class ActionListener final : public Rml::EventListener {
public:
    explicit ActionListener(IUiActionRouter* router=nullptr) noexcept:router_(router) {}
    void set_router(IUiActionRouter* router) noexcept {router_=router;}
    [[nodiscard]] UiActionResult dispatch(UiActionId,const UiActionArguments& arguments={}) const;
    void ProcessEvent(Rml::Event&) override;
private:
    IUiActionRouter* router_{nullptr};
};
class Runtime final {
public:
    Runtime(std::filesystem::path,std::uint32_t,std::uint32_t);
    ~Runtime();
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    [[nodiscard]] bool valid() const noexcept {return context_!=nullptr;}
    [[nodiscard]] Rml::Context* context() noexcept {return context_;}
    [[nodiscard]] const UiRenderFrame& update(double);
    [[nodiscard]] UiRenderFrame& frame() noexcept {return frame_;}
    [[nodiscard]] input::InputFrame filter_input(const input::InputFrame&);
    bool process_input(const input::InputFrame&);
    void set_action_router(IUiActionRouter*) noexcept;
    bool bind_text(std::string,std::string*);
    // Update the already-bound owned strings. Do not rebind temporary pointers
    // or repeatedly call Bind() for a variable that already exists.
    bool set_builtin_text(std::string_view name,std::string value) {
        if (!context_ || !static_cast<bool>(data_model_)) return false;
        std::string* target=name=="status"?&status_:name=="selected"?&selected_:name=="seed"?&seed_:nullptr;
        if (!target) return false;
        if (*target!=value) {
            *target=std::move(value);
            data_model_.GetModelHandle().DirtyVariable(std::string{name});
        }
        return true;
    }
    bool load_document(const std::filesystem::path&);
    bool push_document(const std::filesystem::path&);
    void unload_documents();
    void resize(std::uint32_t,std::uint32_t);
private:
    [[nodiscard]] bool process_event(const input::Event&);
    static void accumulate_event(input::InputFrame&,const input::Event&);
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
