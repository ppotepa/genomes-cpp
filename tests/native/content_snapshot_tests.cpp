#include <genomes/content/ContentSnapshot.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    using namespace genomes;
    const auto root = std::filesystem::temp_directory_path() / "genomes-content-snapshot-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "profiles");
    const auto profile = root / "profiles" / "example.json";
    {
        std::ofstream stream(profile, std::ios::binary);
        stream << "{\"id\":\"example\"}";
    }

    const auto resolved = content::resolveContentPath(root, "profiles/example.json");
    assert(resolved && resolved.value() == std::filesystem::weakly_canonical(profile));
    assert(!content::resolveContentPath(root, "../outside.json"));
    assert(!content::resolveContentPath(root, "profiles/../example.json"));

    const auto document = content::readContentText(resolved.value(), {.maximum_bytes = 1024U});
    assert(document && document.value().text == "{\"id\":\"example\"}");
    assert(document.value().provenance.content_hash != 0U);
    assert(!content::readContentText(resolved.value(), {.maximum_bytes = 2U}));

    const auto manifest_path = root / "mod.json";
    {
        std::ofstream stream(manifest_path, std::ios::binary);
        stream << "{\"schema_version\":1,\"id\":\"test\",\"version\":\"1.0\","
                  "\"load_priority\":2,\"dependencies\":[\"core\"],\"domain_field\":true}";
    }
    const auto manifest = content::readContentManifest(manifest_path);
    assert(manifest && manifest.value().manifest.id == "test");
    assert(manifest.value().manifest.load_priority == 2);
    assert(manifest.value().manifest.dependencies == std::vector<std::string>{"core"});
    assert(!manifest.value().document.text.empty());

    content::ContentSnapshotBuilder first{"test.content", 1U};
    auto first_provenance = document.value().provenance;
    first_provenance.source_id = "profile";
    assert(first.add(first_provenance));
    const auto first_snapshot = std::move(first).freeze();
    assert(first_snapshot && first_snapshot.value().sources.size() == 1U);

    content::ContentSnapshotBuilder duplicate{"test.content", 1U};
    assert(duplicate.add(first_provenance));
    assert(duplicate.add(first_provenance));
    assert(!std::move(duplicate).freeze());
    std::filesystem::remove_all(root);
    return 0;
}
