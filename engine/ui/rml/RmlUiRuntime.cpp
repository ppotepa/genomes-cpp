#include "RmlUiRuntime.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

namespace genomes::ui::rml {

Rml::CompiledGeometryHandle RenderAdapter::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    if (vertices.empty()) return 0;
    Geometry geometry{};
    geometry.vertices.assign(vertices.begin(), vertices.end());
    geometry.indices.assign(indices.begin(), indices.end());
    const auto handle = next_geometry_++;
    geometries_.emplace(handle, geometry);
    return handle;
}

void RenderAdapter::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                   Rml::Vector2f translation, Rml::TextureHandle texture) {
    const auto it = geometries_.find(geometry);
    if (it == geometries_.end()) return;
    const auto& value = it->second;
    UiDrawCommand command{};
    command.primitive = UiDrawPrimitive::Quad;
    command.texture = static_cast<std::uint64_t>(texture);
    command.vertices.reserve(value.vertices.size());
    for (const auto& vertex : value.vertices) {
        command.vertices.push_back({vertex.position.x + translation.x,
                                    vertex.position.y + translation.y,
                                    vertex.tex_coord.x, vertex.tex_coord.y,
                                    {vertex.colour.red / 255.0F,
                                     vertex.colour.green / 255.0F,
                                     vertex.colour.blue / 255.0F,
                                     vertex.colour.alpha / 255.0F}});
    }
    command.indices.reserve(value.indices.size());
    for (const auto index : value.indices) {
        if (index >= 0 && static_cast<std::size_t>(index) < command.vertices.size()) {
            command.indices.push_back(static_cast<std::uint32_t>(index));
        }
    }
    if (!command.vertices.empty()) {
        command.rect = {command.vertices.front().x, command.vertices.front().y, 0.0F, 0.0F};
    }
    command.scissor_enabled = scissor_enabled_;
    command.scissor = {static_cast<float>(scissor_.Left()), static_cast<float>(scissor_.Top()),
                       static_cast<float>(scissor_.Width()), static_cast<float>(scissor_.Height())};
    frame_.commands.push_back(std::move(command));
}

void RenderAdapter::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    geometries_.erase(geometry);
}

Rml::TextureHandle RenderAdapter::LoadTexture(Rml::Vector2i&, const Rml::String&) { return 0; }
Rml::TextureHandle RenderAdapter::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                  Rml::Vector2i dimensions) {
    if (dimensions.x <= 0 || dimensions.y <= 0) return 0;
    const auto id = static_cast<Rml::TextureHandle>(next_texture_++);
    UiTexture texture{};
    texture.id = static_cast<std::uint64_t>(id);
    texture.content_revision = 1U;
    texture.width = static_cast<std::uint32_t>(dimensions.x);
    texture.height = static_cast<std::uint32_t>(dimensions.y);
    const auto* begin = reinterpret_cast<const std::uint8_t*>(source.data());
    texture.rgba.assign(begin, begin + source.size());
    frame_.textures.push_back(std::move(texture));
    return id;
}
void RenderAdapter::ReleaseTexture(Rml::TextureHandle texture) {
    frame_.textures.erase(std::remove_if(frame_.textures.begin(), frame_.textures.end(),
                                         [texture](const UiTexture& value) {
                                             return value.id == static_cast<std::uint64_t>(texture);
                                         }),
                           frame_.textures.end());
}

Rml::FileHandle FileAdapter::Open(const Rml::String& path) {
    const std::filesystem::path relative{path};
    const auto text = path.c_str();
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        (text != nullptr && std::string_view{text}.find(':') != std::string_view::npos)) return 0;
    for (const auto& part : relative) if (part == "..") return 0;
    static constexpr std::array<std::string_view, 8> extensions{
        ".rml", ".rcss", ".png", ".jpg", ".jpeg", ".ttf", ".otf", ".json"};
    const auto extension = relative.extension().string();
    if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) return 0;
    const auto full = (root_ / relative).lexically_normal();
    const auto root = root_.lexically_normal();
    if (full.lexically_relative(root).string().starts_with("..")) return 0;
    return reinterpret_cast<Rml::FileHandle>(std::fopen(full.string().c_str(), "rb"));
}

namespace {
[[nodiscard]] FILE* as_file(Rml::FileHandle file) noexcept {
    return reinterpret_cast<FILE*>(file);
}
}

void FileAdapter::Close(Rml::FileHandle file) { if (file != 0) std::fclose(as_file(file)); }
size_t FileAdapter::Read(void* buffer, size_t size, Rml::FileHandle file) {
    return file == 0 ? 0 : std::fread(buffer, 1, size, as_file(file));
}
bool FileAdapter::Seek(Rml::FileHandle file, long offset, int origin) {
    return file != 0 && std::fseek(as_file(file), offset, origin) == 0;
}
size_t FileAdapter::Tell(Rml::FileHandle file) {
    const long position = file == 0 ? -1 : std::ftell(as_file(file));
    return position < 0 ? 0 : static_cast<size_t>(position);
}

double SystemAdapter::GetElapsedTime() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
}

bool SystemAdapter::LogMessage(Rml::Log::Type, const Rml::String& message) {
    std::cerr << "[RmlUi] " << message << '\n';
    return true;
}

void ActionListener::ProcessEvent(Rml::Event& event) {
    if (router_ == nullptr || event.GetType() != "click") return;
    auto* element = event.GetCurrentElement();
    if (element == nullptr) element = event.GetTargetElement();
    if (element == nullptr) return;
    const auto* value = element->GetAttribute("data-action");
    if (value == nullptr) return;
    const auto action = value->Get<Rml::String>();
    if (action.empty()) return;
    if (router_->dispatch(foundation::stable_id(action), {}) == UiActionResult::Handled) {
        event.StopImmediatePropagation();
    }
}

UiActionResult ActionListener::dispatch(UiActionId action,
                                        const UiActionArguments& arguments) const {
    return router_ == nullptr ? UiActionResult::Unknown : router_->dispatch(action, arguments);
}

Runtime::Runtime(std::filesystem::path asset_root, std::uint32_t width, std::uint32_t height)
    : renderer_{frame_}, files_{std::move(asset_root)}, action_listener_{} {
    Rml::SetSystemInterface(&system_);
    Rml::SetRenderInterface(&renderer_);
    Rml::SetFileInterface(&files_);
    if (Rml::Initialise()) {
        const bool regular_font = Rml::LoadFontFace(
            "ui/fonts/LatoLatin-Regular.ttf", true, Rml::Style::FontWeight::Normal);
        const bool bold_font = Rml::LoadFontFace(
            "ui/fonts/LatoLatin-Bold.ttf", false, Rml::Style::FontWeight::Bold);
        if (!regular_font || !bold_font) {
            std::cerr << "[RmlUi] Core UI fonts could not be loaded\n";
        }
        context_ = Rml::CreateContext("genomes", {static_cast<int>(width), static_cast<int>(height)},
                                      &renderer_);
        if (context_ != nullptr) {
            data_model_ = context_->CreateDataModel("ui");
            data_model_.Bind("status", &status_);
            data_model_.Bind("selected", &selected_);
            data_model_.Bind("seed", &seed_);
        }
    }
}

Runtime::~Runtime() {
    if (context_ != nullptr) Rml::RemoveContext(context_->GetName());
    Rml::Shutdown();
}

const UiRenderFrame& Runtime::update(double delta_seconds) {
    frame_.commands.clear();
    if (context_ != nullptr) {
        context_->Update();
        context_->Render();
    }
    frame_.revision += static_cast<std::uint64_t>(std::max(0.0, delta_seconds) > 0.0 ? 1 : 0);
    return frame_;
}

bool Runtime::process_event(const input::Event& event) {
    if (context_ == nullptr) return false;
    switch (event.type) {
    case input::EventType::KeyDown:
    case input::EventType::KeyUp: {
        Rml::Input::KeyIdentifier key = Rml::Input::KI_UNKNOWN;
        switch (event.scancode) {
        case 79: key = Rml::Input::KI_RIGHT; break;
        case 80: key = Rml::Input::KI_LEFT; break;
        case 81: key = Rml::Input::KI_DOWN; break;
        case 82: key = Rml::Input::KI_UP; break;
        case 40: key = Rml::Input::KI_RETURN; break;
        case 41:
            if (event.type == input::EventType::KeyDown &&
                action_listener_.dispatch(foundation::stable_id("scene.close-overlay")) ==
                    UiActionResult::Handled) {
                return true;
            }
            key = Rml::Input::KI_ESCAPE;
            break;
        default: break;
        }
        if (key == Rml::Input::KI_UNKNOWN) return false;
        return event.type == input::EventType::KeyDown
            ? !context_->ProcessKeyDown(key, 0)
            : !context_->ProcessKeyUp(key, 0);
    }
    case input::EventType::TextInput:
        return !context_->ProcessTextInput(event.text);
    case input::EventType::MouseMove:
        return !context_->ProcessMouseMove(static_cast<int>(event.x),
                                           static_cast<int>(event.y), 0);
    case input::EventType::MouseButtonDown:
        return !context_->ProcessMouseButtonDown(event.mouse_button - 1, 0);
    case input::EventType::MouseButtonUp:
        return !context_->ProcessMouseButtonUp(event.mouse_button - 1, 0);
    case input::EventType::MouseWheel:
        return !context_->ProcessMouseWheel({event.wheel_x, event.wheel_y}, 0);
    case input::EventType::WindowResize:
    case input::EventType::TextInputStart:
    case input::EventType::TextInputStop:
        return false;
    }
    return false;
}

void Runtime::accumulate_event(input::InputFrame& frame, const input::Event& event) {
    frame.events.push_back(event);
    switch (event.type) {
    case input::EventType::KeyDown:
        switch (event.scancode) {
        case 79: frame.right_pressed = true; break;
        case 80: frame.left_pressed = true; break;
        case 81: frame.down_pressed = true; break;
        case 82: frame.up_pressed = true; break;
        case 40: frame.confirm_pressed = true; break;
        case 41: frame.cancel_pressed = true; break;
        default: break;
        }
        break;
    case input::EventType::MouseButtonDown:
        if (event.mouse_button == 1) frame.mouse_left_pressed = true;
        break;
    case input::EventType::MouseMove:
        frame.mouse_delta_x += event.wheel_x;
        frame.mouse_delta_y += event.wheel_y;
        break;
    case input::EventType::MouseWheel:
        frame.mouse_wheel_y += event.wheel_y;
        break;
    case input::EventType::KeyUp:
    case input::EventType::TextInput:
    case input::EventType::MouseButtonUp:
    case input::EventType::WindowResize:
    case input::EventType::TextInputStart:
    case input::EventType::TextInputStop:
        break;
    }
}

input::InputFrame Runtime::filter_input(const input::InputFrame& input) {
    input::InputFrame filtered = input;
    filtered.events.clear();
    filtered.up_pressed = false;
    filtered.down_pressed = false;
    filtered.left_pressed = false;
    filtered.right_pressed = false;
    filtered.confirm_pressed = false;
    filtered.cancel_pressed = false;
    filtered.mouse_left_pressed = false;
    filtered.mouse_delta_x = 0.0F;
    filtered.mouse_delta_y = 0.0F;
    filtered.mouse_wheel_y = 0.0F;

    if (context_ == nullptr) return input;

    for (const auto& event : input.events) {
        bool consumed = process_event(event);
        if (event.type == input::EventType::MouseButtonDown &&
            event.mouse_button == 1 && consumed) {
            ui_left_capture_ = true;
        }
        if (event.type == input::EventType::MouseMove && ui_left_capture_) {
            consumed = true;
        }

        // Release must reach downstream controls even when RmlUi owned the
        // press/drag, otherwise a control that began outside the UI can stick.
        const bool release = event.type == input::EventType::MouseButtonUp ||
                             event.type == input::EventType::KeyUp;
        if (!consumed || release) accumulate_event(filtered, event);

        if (event.type == input::EventType::MouseButtonUp && event.mouse_button == 1) {
            ui_left_capture_ = false;
        }
    }
    filtered.mouse_left_down = input.mouse_left_down && !ui_left_capture_;
    return filtered;
}

bool Runtime::process_input(const input::InputFrame& input) {
    const auto filtered = filter_input(input);
    return filtered.events.size() != input.events.size() ||
           filtered.mouse_left_pressed != input.mouse_left_pressed ||
           filtered.mouse_wheel_y != input.mouse_wheel_y;
}

bool Runtime::load_document(const std::filesystem::path& relative_path) {
    if (context_ == nullptr) return false;
    context_->UnloadAllDocuments();
    return push_document(relative_path);
}

bool Runtime::push_document(const std::filesystem::path& relative_path) {
    if (context_ == nullptr) return false;
    auto* document = context_->LoadDocument(relative_path.generic_string());
    if (document == nullptr) return false;
    document->Show();
    Rml::ElementList action_elements;
    document->QuerySelectorAll(action_elements, "[data-action]");
    for (auto* element : action_elements) {
        element->AddEventListener("click", &action_listener_);
    }
    return true;
}

void Runtime::unload_documents() {
    if (context_ != nullptr) context_->UnloadAllDocuments();
}

void Runtime::set_action_router(IUiActionRouter* router) noexcept {
    action_listener_.set_router(router);
}

bool Runtime::bind_text(std::string name, std::string* value) {
    return static_cast<bool>(data_model_) && value != nullptr && data_model_.Bind(name, value);
}

void Runtime::resize(std::uint32_t width, std::uint32_t height) {
    frame_.viewport_width = width;
    frame_.viewport_height = height;
    if (context_ != nullptr) context_->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
}

} // namespace genomes::ui::rml
