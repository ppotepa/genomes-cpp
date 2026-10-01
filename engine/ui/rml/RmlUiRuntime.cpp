#include "RmlUiRuntime.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <iostream>
#include <memory>
#include <string_view>
#include <type_traits>

namespace genomes::ui::rml {

namespace {
[[nodiscard]] std::string scalar_text(const UiScalar& value) {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::string>) return item;
        else if constexpr (std::is_same_v<T, bool>) return item ? "true" : "false";
        else {
            std::array<char, 64> buffer{};
            const auto result = [&] {
                if constexpr (std::is_floating_point_v<T>)
                    return std::to_chars(buffer.data(), buffer.data() + buffer.size(), item,
                                         std::chars_format::general);
                else
                    return std::to_chars(buffer.data(), buffer.data() + buffer.size(), item);
            }();
            return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr)
                                             : std::string{};
        }
    }, value);
}
}

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
    auto full = (root_ / relative).lexically_normal();
    if (!std::filesystem::is_regular_file(full) && relative.filename() == "theme.rcss") {
        full = (root_ / "ui" / "theme.rcss").lexically_normal();
    }
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

bool SystemAdapter::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    messages_.push_back(message);
    if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT) ++errors_;
    std::cerr << "[RmlUi] " << message << '\n';
    return true;
}

void ActionListener::ProcessEvent(Rml::Event& event) {
    if (event.GetType() != "click" && event.GetType() != "change" &&
        event.GetType() != "input" && event.GetType() != "submit") return;
    auto* element = event.GetTargetElement();
    if (element == nullptr) element = event.GetCurrentElement();
    if (element == nullptr) return;
    if (!element->IsVisible(true)) return;
    if (const auto* control = dynamic_cast<const Rml::ElementFormControl*>(element);
        control != nullptr && control->IsDisabled()) return;
    if (event.GetType() == "click" && file_dialog_service_ != nullptr) {
        if (const auto* mode_attribute = element->GetAttribute("data-file-dialog");
            mode_attribute != nullptr) {
            const auto mode = mode_attribute->Get<Rml::String>();
            const auto* title_attribute = element->GetAttribute("data-file-dialog-title");
            const auto* filter_attribute = element->GetAttribute("data-file-dialog-filter");
            const std::string title = title_attribute != nullptr
                ? title_attribute->Get<Rml::String>() : "Choose file";
            const std::string filter = filter_attribute != nullptr
                ? filter_attribute->Get<Rml::String>() : "*";
            const auto selected = mode == "save"
                ? file_dialog_service_->save_file(title, filter)
                : file_dialog_service_->open_file(title, filter);
            if (selected.has_value() && event_router_ != nullptr) {
                UiEvent typed{};
                typed.route_id = route_id_;
                typed.control = element->GetAttribute("data-control") != nullptr
                    ? element->GetAttribute("data-control")->Get<Rml::String>() : "";
                typed.field = element->GetAttribute("data-field") != nullptr
                    ? element->GetAttribute("data-field")->Get<Rml::String>() : "";
                typed.phase = UiEventPhase::Click;
                typed.route_revision = route_revision_;
                typed.value = *selected;
                if (!typed.control.empty() && event_router_(typed) == UiActionResult::Handled) {
                    event.StopImmediatePropagation();
                    return;
                }
            }
        }
    }
    if (event_router_) {
        UiEvent typed{};
        typed.route_id = route_id_;
        typed.route_revision = route_revision_;
        if (const auto* control = element->GetAttribute("data-control"); control != nullptr)
            typed.control = control->Get<Rml::String>();
        if (const auto* field = element->GetAttribute("data-field"); field != nullptr)
            typed.field = field->Get<Rml::String>();
        if (const auto* key = element->GetAttribute("data-key"); key != nullptr)
            typed.arguments.emplace_back("key", key->Get<Rml::String>());
        if (const auto* value = element->GetAttribute("value"); value != nullptr)
            typed.value = value->Get<Rml::String>();
        else if (const auto* value = element->GetAttribute("data-value"); value != nullptr)
            typed.value = value->Get<Rml::String>();
        if (const auto* input = dynamic_cast<const Rml::ElementFormControlInput*>(element);
            input != nullptr) {
            const auto type_name = element->GetAttribute("type") != nullptr
                ? element->GetAttribute("type")->Get<Rml::String>() : Rml::String{};
            if (type_name == "checkbox" || type_name == "radio") typed.value = element->HasAttribute("checked");
            else typed.value = input->GetValue();
        } else if (const auto* select = dynamic_cast<const Rml::ElementFormControlSelect*>(element);
                   select != nullptr) {
            typed.value = select->GetValue();
        }
        if (const auto* type = element->GetAttribute("type"); type != nullptr) {
            const auto type_name = type->Get<Rml::String>();
            if (type_name == "checkbox" || type_name == "radio")
                typed.value = element->HasAttribute("checked");
            // Number controls intentionally retain their text representation:
            // converting a uint64 seed through double loses precision.
            else if (type_name == "range") {
                if (const auto* text = std::get_if<std::string>(&typed.value); text != nullptr) {
                    double number = 0.0;
                    const auto converted = std::from_chars(text->data(), text->data() + text->size(), number,
                                                           std::chars_format::general);
                    if (converted.ec == std::errc{} && converted.ptr == text->data() + text->size())
                        typed.value = number;
                }
            }
        }
        if (event.GetType() == "input") typed.phase = UiEventPhase::Input;
        else if (event.GetType() == "change") typed.phase = UiEventPhase::Change;
        else if (event.GetType() == "submit") typed.phase = UiEventPhase::Submit;
        if (!typed.control.empty() && event_router_(typed) == UiActionResult::Handled) {
            event.StopImmediatePropagation();
            return;
        }
    }
    if (router_ == nullptr || event.GetType() != "click") return;
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
    frame_.viewport_width = width;
    frame_.viewport_height = height;
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
            (void)set_text("status", "Preparing presentation...");
            (void)set_text("selected", "No selection");
            (void)set_text("seed", "Seed: 0");
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
    if (modal_document_ != nullptr &&
        (event.type == input::EventType::KeyDown || event.type == input::EventType::KeyUp)) {
        if (auto* focused = context_->GetFocusElement();
            focused != nullptr && focused->GetOwnerDocument() != modal_document_) {
            (void)modal_document_->GetFocusLeafNode()->Focus();
        }
    }
    switch (event.type) {
    case input::EventType::KeyDown:
    case input::EventType::KeyUp: {
        Rml::Input::KeyIdentifier key = Rml::Input::KI_UNKNOWN;
        switch (event.scancode) {
        case 42: key = Rml::Input::KI_BACK; break;
        case 43: key = Rml::Input::KI_TAB; break;
        case 44: key = Rml::Input::KI_SPACE; break;
        case 74: key = Rml::Input::KI_HOME; break;
        case 76: key = Rml::Input::KI_DELETE; break;
        case 77: key = Rml::Input::KI_END; break;
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
        unsigned modifiers = 0U;
        if ((event.modifiers & 1U) != 0U) modifiers |= Rml::Input::KM_SHIFT;
        if ((event.modifiers & 2U) != 0U) modifiers |= Rml::Input::KM_CTRL;
        if ((event.modifiers & 4U) != 0U) modifiers |= Rml::Input::KM_ALT;
        return event.type == input::EventType::KeyDown
            ? !context_->ProcessKeyDown(key, modifiers)
            : !context_->ProcessKeyUp(key, modifiers);
    }
    case input::EventType::TextInput:
        return !context_->ProcessTextInput(event.text);
    case input::EventType::MouseMove:
        return !context_->ProcessMouseMove(static_cast<int>(event.x),
                                           static_cast<int>(event.y), 0);
    case input::EventType::MouseButtonDown:
        (void)context_->ProcessMouseMove(static_cast<int>(event.x),
                                         static_cast<int>(event.y), 0);
        return !context_->ProcessMouseButtonDown(event.mouse_button - 1, 0);
    case input::EventType::MouseButtonUp:
        (void)context_->ProcessMouseMove(static_cast<int>(event.x),
                                         static_cast<int>(event.y), 0);
        return !context_->ProcessMouseButtonUp(event.mouse_button - 1, 0);
    case input::EventType::MouseWheel:
        return !context_->ProcessMouseWheel({event.wheel_x, event.wheel_y}, 0);
    case input::EventType::WindowResize:
    case input::EventType::TextInputStart:
    case input::EventType::TextInputStop:
    case input::EventType::FocusLost:
    case input::EventType::PointerCancel:
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
        case 21: frame.reset_pressed = true; break;
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
    case input::EventType::FocusLost:
    case input::EventType::PointerCancel:
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
    filtered.reset_pressed = false;
    filtered.mouse_left_pressed = false;
    filtered.mouse_delta_x = 0.0F;
    filtered.mouse_delta_y = 0.0F;
    filtered.mouse_wheel_y = 0.0F;

    if (context_ == nullptr) return input;

    for (const auto& event : input.events) {
        if (event.type == input::EventType::FocusLost ||
            event.type == input::EventType::PointerCancel) {
            ui_mouse_capture_.fill(false);
            passthrough_mouse_capture_.fill(false);
        }
        bool consumed = process_event(event);
        const auto button_index = event.mouse_button - 1;
        if (event.type == input::EventType::MouseButtonDown &&
            button_index >= 0 && button_index < 3) {
            ui_mouse_capture_[static_cast<std::size_t>(button_index)] = consumed;
            passthrough_mouse_capture_[static_cast<std::size_t>(button_index)] = !consumed;
        }
        if (event.type == input::EventType::MouseMove &&
            (ui_mouse_capture_[0] || ui_mouse_capture_[1] || ui_mouse_capture_[2])) {
            consumed = true;
        } else if (event.type == input::EventType::MouseMove &&
                   (passthrough_mouse_capture_[0] || passthrough_mouse_capture_[1] ||
                    passthrough_mouse_capture_[2])) {
            consumed = false;
        }

        // Release must reach downstream controls even when RmlUi owned the
        // press/drag, otherwise a control that began outside the UI can stick.
        const bool release = event.type == input::EventType::MouseButtonUp ||
                             event.type == input::EventType::KeyUp;
        if (!consumed || release) accumulate_event(filtered, event);

        if (event.type == input::EventType::MouseButtonUp &&
            button_index >= 0 && button_index < 3) {
            ui_mouse_capture_[static_cast<std::size_t>(button_index)] = false;
            passthrough_mouse_capture_[static_cast<std::size_t>(button_index)] = false;
        }
    }
    filtered.mouse_left_down = input.mouse_left_down && !ui_mouse_capture_[0];
    filtered.mouse_middle_down = input.mouse_middle_down && !ui_mouse_capture_[2];
    filtered.mouse_right_down = input.mouse_right_down && !ui_mouse_capture_[1];
    return filtered;
}

bool Runtime::process_input(const input::InputFrame& input) {
    const auto filtered = filter_input(input);
    return filtered.events.size() != input.events.size() ||
           filtered.mouse_left_pressed != input.mouse_left_pressed ||
           filtered.reset_pressed != input.reset_pressed ||
           filtered.mouse_left_down != input.mouse_left_down ||
           filtered.mouse_middle_down != input.mouse_middle_down ||
           filtered.mouse_right_down != input.mouse_right_down ||
           filtered.mouse_delta_x != input.mouse_delta_x ||
           filtered.mouse_delta_y != input.mouse_delta_y ||
           filtered.mouse_wheel_y != input.mouse_wheel_y;
}

bool Runtime::load_document(const std::filesystem::path& relative_path) {
    if (context_ == nullptr) return false;
    unload_documents();
    return push_document(relative_path);
}

bool Runtime::mount_routes(const std::vector<UiRoute>& routes, const UiRuntime& runtime) {
    if (context_ == nullptr) return false;
    unload_documents();
    action_listener_.set_route_revision(routes.empty() ? 0U : routes.back().revision);
    action_listener_.set_route_id(routes.empty() ? 0U : routes.back().scene);
    for (const auto& route : routes) {
        auto slot = std::make_unique<RouteModel>();
        slot->name = "ui_" + (route.controller.empty() ? std::string{"route"} : route.controller);
        std::replace(slot->name.begin(), slot->name.end(), '.', '_');
        std::replace(slot->name.begin(), slot->name.end(), '-', '_');
        slot->model = context_->CreateDataModel(slot->name);
        if (!slot->model) return false;
        if (!rml_types_registered_) {
            auto row = slot->model.RegisterStruct<RmlRow>();
            if (!row || !row.RegisterMember("id", &RmlRow::id) ||
                !row.RegisterMember("group", &RmlRow::group) ||
                !row.RegisterMember("label", &RmlRow::label) ||
                !row.RegisterMember("value", &RmlRow::value) ||
                !row.RegisterMember("number_value", &RmlRow::number_value) ||
                !row.RegisterMember("enabled", &RmlRow::enabled) ||
                !row.RegisterMember("selected", &RmlRow::selected) ||
                !row.RegisterMember("overridden", &RmlRow::overridden) ||
                !slot->model.RegisterArray<std::vector<RmlRow>>()) return false;
            rml_types_registered_ = true;
        }
        slot->lists.emplace("entries", std::vector<RmlRow>{});
        if (!slot->model.Bind("entries", &slot->lists.at("entries"))) return false;
        slot->bound_lists.push_back("entries");
        route_models_.push_back(std::move(slot));
        set_model(runtime.model());
        set_route_models(runtime);
        if (!push_document(route.document)) return false;
    }
    return true;
}

bool Runtime::push_document(const std::filesystem::path& relative_path) {
    if (context_ == nullptr) return false;
    auto* document = context_->LoadDocument(relative_path.generic_string());
    if (document == nullptr) return false;
    document->Show();
    modal_document_ = document;
    document->AddEventListener("click", &action_listener_);
    document->AddEventListener("input", &action_listener_);
    document->AddEventListener("change", &action_listener_);
    document->AddEventListener("submit", &action_listener_);
    return true;
}

void Runtime::unload_documents() {
    if (context_ != nullptr) context_->UnloadAllDocuments();
    if (context_ != nullptr) {
        for (const auto& route : route_models_) (void)context_->RemoveDataModel(route->name);
    }
    route_models_.clear();
    modal_document_ = nullptr;
}

std::optional<UiViewportMetrics> Runtime::element_viewport_metrics(
    std::string_view element_id) const noexcept {
    if (modal_document_ == nullptr || element_id.empty()) return std::nullopt;
    auto* element = modal_document_->GetElementById(std::string{element_id});
    if (element == nullptr || !element->IsVisible(true)) return std::nullopt;
    const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
    UiViewportMetrics result{position.x, position.y, size.x, size.y};
    return result.valid() ? std::optional<UiViewportMetrics>{result} : std::nullopt;
}

void Runtime::set_model(const UiDataModel& model) {
    model.for_each_field([this](std::string_view key, const UiFieldState& field) {
        (void)set_text(key, scalar_text(field.value));
        (void)set_text(std::string{key} + "_error", field.error);
        for (auto& route : route_models_) {
            set_route_scalar(*route, key, field.value);
            set_route_scalar(*route, std::string{key} + "_error", UiScalar{field.error});
            if (!field.options.empty()) {
                const std::string options_key = std::string{key} + "_options";
                std::vector<RmlRow> next_options;
                next_options.reserve(field.options.size());
                for (const auto& option : field.options) {
                    next_options.push_back({option.value, {}, option.label, option.value, 0.0,
                                            option.enabled,
                                            scalar_text(field.value) == option.value, false});
                }
                auto& options = route->lists[options_key];
                if (std::find(route->bound_lists.begin(), route->bound_lists.end(), options_key) == route->bound_lists.end()) {
                    if (route->model.Bind(options_key, &options)) route->bound_lists.push_back(options_key);
                }
                if (options != next_options) {
                    options = std::move(next_options);
                    route->model.GetModelHandle().DirtyVariable(options_key);
                }
            }
        }
    });
    model.for_each_list([this](std::string_view key, const std::vector<UiTableRow>& source) {
        for (auto& route : route_models_) {
            std::vector<RmlRow> next;
            next.reserve(source.size());
            for (const auto& row : source) {
                RmlRow item{};
                if (const auto it = row.find("id"); it != row.end()) item.id = scalar_text(it->second);
                if (const auto it = row.find("group"); it != row.end()) item.group = scalar_text(it->second);
                if (const auto it = row.find("label"); it != row.end()) item.label = scalar_text(it->second);
                if (const auto it = row.find("value"); it != row.end()) item.value = scalar_text(it->second);
                if (const auto it = row.find("value"); it != row.end()) {
                    if (const auto* number = std::get_if<double>(&it->second)) item.number_value = *number;
                    else if (const auto* integer = std::get_if<std::int64_t>(&it->second))
                        item.number_value = static_cast<double>(*integer);
                }
                if (const auto it = row.find("enabled"); it != row.end())
                    if (const auto* enabled = std::get_if<bool>(&it->second); enabled != nullptr) item.enabled = *enabled;
                if (const auto it = row.find("selected"); it != row.end())
                    if (const auto* selected = std::get_if<bool>(&it->second); selected != nullptr) item.selected = *selected;
                if (const auto it = row.find("overridden"); it != row.end())
                    if (const auto* overridden = std::get_if<bool>(&it->second); overridden != nullptr) item.overridden = *overridden;
                next.push_back(std::move(item));
            }
            auto& target = route->lists[std::string{key}];
            if (std::find(route->bound_lists.begin(), route->bound_lists.end(), std::string{key}) == route->bound_lists.end()) {
                if (route->model.Bind(std::string{key}, &target)) route->bound_lists.push_back(std::string{key});
            }
            if (target != next) {
                target = std::move(next);
                route->model.GetModelHandle().DirtyVariable(std::string{key});
            }
        }
    });
}

void Runtime::set_route_scalar(RouteModel& route, std::string_view key, const UiScalar& value) {
    std::visit([&route, key](const auto& item) {
        using Value = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Value, bool>) {
            auto [it, inserted] = route.bool_values.try_emplace(std::string{key}, item);
            if (inserted) (void)route.model.Bind(it->first, &it->second);
            if (it->second != item) { it->second = item; route.model.GetModelHandle().DirtyVariable(it->first); }
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
            auto [it, inserted] = route.integer_values.try_emplace(std::string{key}, item);
            if (inserted) (void)route.model.Bind(it->first, &it->second);
            if (it->second != item) { it->second = item; route.model.GetModelHandle().DirtyVariable(it->first); }
        } else if constexpr (std::is_same_v<Value, double>) {
            auto [it, inserted] = route.number_values.try_emplace(std::string{key}, item);
            if (inserted) (void)route.model.Bind(it->first, &it->second);
            if (it->second != item) { it->second = item; route.model.GetModelHandle().DirtyVariable(it->first); }
        } else {
            auto [it, inserted] = route.string_values.try_emplace(std::string{key}, item);
            if (inserted) (void)route.model.Bind(it->first, &it->second);
            if (it->second != item) { it->second = item; route.model.GetModelHandle().DirtyVariable(it->first); }
        }
    }, value);
}

void Runtime::set_route_models(const UiRuntime& runtime) {
    const auto& states = runtime.route_controllers();
    for (std::size_t index = 0; index < states.size() && index < route_models_.size(); ++index) {
        auto& target = *route_models_[index];
        states[index].model.for_each_field([this, &target, &runtime](std::string_view key, const UiFieldState& field) {
            // Scene ViewModels publish their fields into the shared model. The
            // route model is an overlay: only fields that are not supplied by
            // the current scene may replace that snapshot (for example a
            // controller-owned diagnostic field). This keeps base-scene HUD
            // values live while preserving independent overlay state.
            if (runtime.model().find_field(key) != nullptr || runtime.model().find(key) != nullptr)
                return;
            set_route_scalar(target, key, field.value);
        });
    }
}

void Runtime::set_density_ratio(float ratio) {
    if (context_ != nullptr) context_->SetDensityIndependentPixelRatio(
        std::clamp(ratio, 0.75F, 1.50F));
}

void Runtime::set_action_router(IUiActionRouter* router) noexcept {
    action_listener_.set_router(router);
}

void Runtime::set_event_router(std::function<UiActionResult(const UiEvent&)> router) {
    event_router_ = std::move(router);
    action_listener_.set_event_router(event_router_);
}

void Runtime::set_file_dialog_service(IFileDialogService* service) noexcept {
    action_listener_.set_file_dialog_service(service);
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
