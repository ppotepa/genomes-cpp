#pragma once
#include <genomes/ui/UiRuntime.hpp>
#include <genomes/ui/UiDataModel.hpp>
#include <genomes/ui/UiEvent.hpp>
#include <genomes/ui/UiServices.hpp>
#include <RmlUi/Core.h>
#include <chrono>
#include <cstdio>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::ui::rml {
struct RmlRow final {
    std::string id;
    std::string group;
    std::string label;
    std::string value;
    double number_value{0.0};
    bool enabled{true};
    bool selected{false};
    bool overridden{false};
    bool operator==(const RmlRow&) const = default;
};
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
    bool LogMessage(Rml::Log::Type, const Rml::String&) override;
    [[nodiscard]] const std::vector<std::string>& messages() const noexcept { return messages_; }
    [[nodiscard]] bool has_errors() const noexcept { return errors_ != 0; }
private:
    std::chrono::steady_clock::time_point start_;
    std::vector<std::string> messages_;
    std::size_t errors_{0};
};
class ActionListener final : public Rml::EventListener {
public:
    explicit ActionListener(IUiActionRouter* router=nullptr) noexcept:router_(router) {}
    void set_router(IUiActionRouter* router) noexcept {router_=router;}
    void set_file_dialog_service(IFileDialogService* service) noexcept {file_dialog_service_=service;}
    void set_event_router(std::function<UiActionResult(const UiEvent&)> router) {event_router_=std::move(router);}
    void set_route_id(std::uint64_t id) noexcept {route_id_=id;}
    void set_route_revision(std::uint64_t revision) noexcept {route_revision_=revision;}
    [[nodiscard]] UiActionResult dispatch(UiActionId,const UiActionArguments& arguments={}) const;
    void ProcessEvent(Rml::Event&) override;
private:
    IUiActionRouter* router_{nullptr};
    IFileDialogService* file_dialog_service_{nullptr};
    std::function<UiActionResult(const UiEvent&)> event_router_{};
    std::uint64_t route_revision_{0};
    std::uint64_t route_id_{0};
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
    [[nodiscard]] std::optional<UiViewportMetrics> element_viewport_metrics(
        std::string_view element_id) const noexcept;
    [[nodiscard]] input::InputFrame filter_input(const input::InputFrame&);
    bool process_input(const input::InputFrame&);
    void set_action_router(IUiActionRouter*) noexcept;
    void set_event_router(std::function<UiActionResult(const UiEvent&)>);
    void set_file_dialog_service(IFileDialogService*) noexcept;
    bool bind_text(std::string,std::string*);
    // Update the already-bound owned strings. Do not rebind temporary pointers
    // or repeatedly call Bind() for a variable that already exists.
    bool set_text(std::string_view name,std::string value) {
        if (!context_ || !static_cast<bool>(data_model_)) return false;
        auto [it, inserted] = text_values_.try_emplace(std::string{name});
        if (inserted && !data_model_.Bind(it->first, &it->second)) return false;
        if (it->second!=value) {
            it->second=std::move(value);
            data_model_.GetModelHandle().DirtyVariable(std::string{name});
        }
        return true;
    }
    bool load_document(const std::filesystem::path&);
    bool push_document(const std::filesystem::path&);
    bool mount_routes(const std::vector<UiRoute>&, const UiRuntime&);
    void unload_documents();
    void resize(std::uint32_t,std::uint32_t);
    void set_density_ratio(float);
    void set_model(const UiDataModel&);
    void set_route_models(const UiRuntime&);
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept { return system_.messages(); }
    [[nodiscard]] bool has_diagnostics_errors() const noexcept { return system_.has_errors(); }
private:
    [[nodiscard]] bool process_event(const input::Event&);
    static void accumulate_event(input::InputFrame&,const input::Event&);
    RenderAdapter renderer_;
    FileAdapter files_;
    SystemAdapter system_;
    ActionListener action_listener_;
    Rml::Context* context_{nullptr};
    Rml::DataModelConstructor data_model_{};
    // Node addresses remain stable as additional bindings are introduced.
    std::map<std::string, std::string, std::less<>> text_values_;
    struct RouteModel final {
        std::string name;
        Rml::DataModelConstructor model;
        std::map<std::string, bool, std::less<>> bool_values;
        std::map<std::string, std::int64_t, std::less<>> integer_values;
        std::map<std::string, double, std::less<>> number_values;
        std::map<std::string, std::string, std::less<>> string_values;
        std::map<std::string, std::vector<RmlRow>, std::less<>> lists;
        std::vector<std::string> bound_lists;
    };
    std::vector<std::unique_ptr<RouteModel>> route_models_;
    Rml::ElementDocument* modal_document_{nullptr};
    bool rml_types_registered_{false};
    void set_route_scalar(RouteModel&, std::string_view, const UiScalar&);
    UiRenderFrame frame_{};
    std::array<bool, 3> ui_mouse_capture_{};
    std::array<bool, 3> passthrough_mouse_capture_{};
    std::function<UiActionResult(const UiEvent&)> event_router_{};
};
} // namespace genomes::ui::rml
