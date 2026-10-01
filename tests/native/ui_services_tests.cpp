#include <genomes/ui/UiServices.hpp>

#include <cassert>
#include <string>

namespace {
class FakeFileDialogService final : public genomes::ui::IFileDialogService {
public:
    std::optional<std::string> open_file(std::string_view, std::string_view) override {
        return std::string{"C:/tmp/import.json"};
    }
    std::optional<std::string> save_file(std::string_view, std::string_view) override {
        return std::string{"C:/tmp/export.json"};
    }
};
}

int main() {
    genomes::ui::NullFileDialogService service;
    assert(!service.open_file("Open", "*.json"));
    assert(!service.save_file("Save", "*.json"));
    FakeFileDialogService fake;
    assert(fake.open_file("Open", "*.json") == std::optional<std::string>{"C:/tmp/import.json"});
    assert(fake.save_file("Save", "*.json") == std::optional<std::string>{"C:/tmp/export.json"});
    return 0;
}
