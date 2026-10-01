#include <genomes/content/ConfigurationSnapshot.hpp>

#include <cassert>
#include <cstdint>
#include <string>

int main() {
    using namespace genomes::content;

    ConfigurationSchema schema{
        {{"id", true, {}}, {"display_name", true, {}}, {"mode", false, {"safe", "fast"}},
         {"material", false, {}}},
        {{"material", {"steel", "wood"}}},
        {"mode"}};

    ConfigurationResolver resolver;
    assert(resolver.addLayer({"core", ConfigurationLayerKind::Core, 0, {},
                              R"({"id":"rifle","display_name":"Rifle","mode":"safe","material":"steel"})"}));
    assert(resolver.addLayer({"profile", ConfigurationLayerKind::NamedProfile, 0, {"core"},
                              R"({"display_name":"Field Rifle","mode":"fast"})"}));
    assert(resolver.addLayer({"cli", ConfigurationLayerKind::CommandLine, 0, {"profile"},
                              R"({"mode":"safe"})"}));

    auto snapshot = std::move(resolver).resolve(schema);
    assert(snapshot);
    assert(snapshot.value().fields.size() == 4U);
    assert(std::get<std::string>(snapshot.value().find("display_name")->value) == "Field Rifle");
    assert(snapshot.value().find("display_name")->source_layer == "profile");
    assert(snapshot.value().simulation_hash.value != 0U);
    assert(snapshot.value().presentation_hash.value != snapshot.value().simulation_hash.value);
    assert(snapshot.value().execution_hash.value != snapshot.value().presentation_hash.value);

    ConfigurationResolver equivalent;
    assert(equivalent.addLayer({"core", ConfigurationLayerKind::Core, 0, {},
                                "{\"material\":\"steel\",\"mode\":\"safe\",\"display_name\":\"Rifle\",\"id\":\"rifle\"}"}));
    auto equivalent_snapshot = std::move(equivalent).resolve(
        ConfigurationSchema{{{"id", true, {}}, {"display_name", true, {}}, {"mode", false, {"safe"}},
                             {"material", false, {}}},
                            {{"material", {"steel"}}}, {}});
    assert(equivalent_snapshot);
    assert(equivalent_snapshot.value().simulation_hash ==
           snapshot.value().simulation_hash);

    ConfigurationResolver unknown;
    assert(unknown.addLayer({"core", ConfigurationLayerKind::Core, 0, {},
                             R"({"id":"rifle","display_name":"Rifle","unknown":true})"}));
    assert(!std::move(unknown).resolve(schema));

    ConfigurationResolver missing_reference;
    assert(missing_reference.addLayer({"core", ConfigurationLayerKind::Core, 0, {},
                                       R"({"id":"rifle","display_name":"Rifle","material":"glass"})"}));
    assert(!std::move(missing_reference).resolve(schema));

    ConfigurationResolver bad_cli;
    assert(bad_cli.addLayer({"core", ConfigurationLayerKind::Core, 0, {},
                             R"({"id":"rifle","display_name":"Rifle"})"}));
    assert(bad_cli.addLayer({"cli", ConfigurationLayerKind::CommandLine, 0, {"core"},
                             R"({"display_name":"override"})"}));
    assert(!std::move(bad_cli).resolve(schema));
    return 0;
}
