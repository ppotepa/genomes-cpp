#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace genomes::ui {

class IFileDialogService {
public:
    virtual ~IFileDialogService() = default;
    [[nodiscard]] virtual std::optional<std::string> open_file(
        std::string_view title, std::string_view filter) = 0;
    [[nodiscard]] virtual std::optional<std::string> save_file(
        std::string_view title, std::string_view filter) = 0;
};

class NullFileDialogService final : public IFileDialogService {
public:
    std::optional<std::string> open_file(std::string_view, std::string_view) override { return {}; }
    std::optional<std::string> save_file(std::string_view, std::string_view) override { return {}; }
};

} // namespace genomes::ui
