#include "infantry_fixture_reader.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

int main() {
    using namespace genomes::test::infantry_fixture;
    const auto manifest_path = std::filesystem::path(GENOMES_SOURCE_DIR) /
                               "reference/fixtures/manifest.json";
    std::ifstream manifest_input(manifest_path);
    assert(manifest_input);
    const nlohmann::json fixture_manifest = nlohmann::json::parse(manifest_input);
    assert(fixture_manifest.at("schema") == "genomes.fixture-manifest.v1");
    assert(fixture_manifest.at("generator") ==
           "tools/reference/export_infantry_reference.cjs");
    assert(fixture_manifest.at("families").is_array());
    assert(fixture_manifest.at("families").size() >= 5U);
    for (const auto& family : fixture_manifest.at("families")) {
        assert(family.at("id").is_string());
        assert(family.at("root").is_string());
        assert(family.at("format").is_string());
        const auto root = std::filesystem::path(GENOMES_SOURCE_DIR) /
                          "reference/fixtures" / family.at("root").get<std::string>();
        assert(std::filesystem::is_directory(root));
    }
    Stream expected{"positions", ScalarType::Float32, 3U, std::vector<std::byte>(12U)};
    Stream actual = expected;
    const std::array<float, 3U> left{1.0F, 2.0F, 3.0F};
    const std::array<float, 3U> right{1.0F, 2.00001F, 3.0F};
    std::memcpy(expected.bytes.data(), left.data(), expected.bytes.size());
    std::memcpy(actual.bytes.data(), right.data(), actual.bytes.size());
    assert(compare(expected, actual, 2.0e-5).equal);
    const auto mismatch = compare(expected, actual, 2.0e-6);
    assert(!mismatch.equal && mismatch.first_difference == 1U);
    assert(mismatch.maximum_error > 2.0e-6);
    actual.element_count = 2U;
    assert(!compare(expected, actual, 2.0e-5).equal);

    const std::filesystem::path fixture_root =
        std::filesystem::path(GENOMES_SOURCE_DIR) /
        "reference/fixtures/infantry/full-avatar";
    constexpr std::array seeds{0U, 8841U, 1003U, 1592598566U};
    constexpr std::array details{"far", "world", "high"};
    constexpr std::array equipment_variants{"neutral", "default", "full"};
    constexpr std::array required_streams{
        "body.positions", "body.normals", "body.colors", "body.uvs",
        "body.skinIndices", "body.skinWeights", "body.indices",
        "gear.positions", "gear.normals", "gear.colors", "gear.uvs",
        "gear.skinIndices", "gear.skinWeights", "gear.indices"};
    std::size_t fixture_count = 0U;
    for (const auto seed : seeds) {
        for (const char* detail : details) {
            for (const char* equipment : equipment_variants) {
                const std::string stem = "avatar-" + std::to_string(seed) + "-" + detail +
                                         "-" + equipment + ".gnif";
                const auto binary = fixture_root / stem;
                const Fixture fixture = read(binary, binary.string() + ".json");
                assert(fixture.manifest.at("schema") == "genomes.infantry.reference.v1");
                assert(fixture.manifest.at("provenance").at("sourceCommit") ==
                       "da885ca68b2ae63154a004574fed00eb9dfeb458");
                assert(fixture.manifest.at("provenance").at("sourceTree") ==
                       "50e15a425f00611df1900b3d56c1d382978ec19e");
                assert(fixture.manifest.at("provenance").at("generatorVersion") ==
                       "genomes-infantry-exporter-v10");
                assert(fixture.manifest.at("request").at("seed") == seed);
                assert(fixture.manifest.at("request").at("detail") == detail);
                assert(fixture.manifest.at("request").at("equipment") == equipment);
                const auto& equipment_state = fixture.manifest.at("equipmentState");
                assert(equipment_state.at("schema") == "EQUIPMENT/v1");
                assert(equipment_state.at("wear") == 0.25);
                const auto& equipment_fit = fixture.manifest.at("equipmentFit");
                assert(equipment_fit.at("jacket").size() == 14U);
                assert(equipment_fit.at("sockets").size() == 19U);
                assert(equipment_fit.at("headBottom").size() ==
                       (std::string_view(equipment) == "neutral" ? 0U : 4U));
                for (const char* stream_name : required_streams) {
                    assert(fixture.find(stream_name) != nullptr);
                }
                const Stream* positions = fixture.find("body.positions");
                const Stream* indices = fixture.find("body.indices");
                assert(positions->type == ScalarType::Float32);
                assert(indices->type == ScalarType::Uint32);
                assert(positions->element_count > 0U && positions->element_count % 3U == 0U);
                assert(indices->element_count > 0U && indices->element_count % 3U == 0U);
                const auto& gear = fixture.manifest.at("gear");
                for (const char* mesh_name : {"body", "gear"}) {
                    const auto& mesh_manifest = fixture.manifest.at(mesh_name);
                    if (mesh_manifest.is_null()) continue;
                    const auto& extrema = mesh_manifest.at("extrema");
                    if (extrema.is_null()) continue;
                    if (extrema.at("positions").is_null()) continue;
                    assert(extrema.at("positions").at("minimum").size() == 3U);
                    assert(extrema.at("positions").at("maximum").size() == 3U);
                    assert(extrema.at("normals").at("minimum").size() == 3U);
                    assert(extrema.at("normals").at("maximum").size() == 3U);
                    assert(extrema.at("indices").at("minimum").size() == 1U);
                    assert(extrema.at("indices").at("maximum").size() == 1U);
                    const auto* positions = fixture.find(std::string(mesh_name) + ".positions");
                    const auto* indices = fixture.find(std::string(mesh_name) + ".indices");
                    assert(positions != nullptr && indices != nullptr);
                    std::array<float, 3U> position_min{
                        std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity()};
                    std::array<float, 3U> position_max{
                        -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()};
                    for (std::size_t value = 0; value < positions->element_count; ++value) {
                        float sample{};
                        std::memcpy(&sample, positions->bytes.data() + value * sizeof(float),
                                    sizeof(float));
                        position_min[value % 3U] = std::min(position_min[value % 3U], sample);
                        position_max[value % 3U] = std::max(position_max[value % 3U], sample);
                    }
                    for (std::size_t component = 0; component < 3U; ++component) {
                        assert(extrema.at("positions").at("minimum").at(component).get<float>() ==
                               position_min[component]);
                        assert(extrema.at("positions").at("maximum").at(component).get<float>() ==
                               position_max[component]);
                    }
                    const auto* normals = fixture.find(std::string(mesh_name) + ".normals");
                    assert(normals != nullptr && normals->element_count % 3U == 0U);
                    std::array<float, 3U> normal_min{
                        std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity()};
                    std::array<float, 3U> normal_max{
                        -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()};
                    for (std::size_t value = 0; value < normals->element_count; ++value) {
                        float sample{};
                        std::memcpy(&sample, normals->bytes.data() + value * sizeof(float),
                                    sizeof(float));
                        normal_min[value % 3U] = std::min(normal_min[value % 3U], sample);
                        normal_max[value % 3U] = std::max(normal_max[value % 3U], sample);
                    }
                    for (std::size_t component = 0; component < 3U; ++component) {
                        assert(extrema.at("normals").at("minimum").at(component).get<float>() ==
                               normal_min[component]);
                        assert(extrema.at("normals").at("maximum").at(component).get<float>() ==
                               normal_max[component]);
                    }
                    assert(indices->element_count > 0U);
                    std::uint32_t index_min = std::numeric_limits<std::uint32_t>::max();
                    std::uint32_t index_max = 0U;
                    for (std::size_t value = 0; value < indices->element_count; ++value) {
                        std::uint32_t sample{};
                        std::memcpy(&sample, indices->bytes.data() + value * sizeof(sample),
                                    sizeof(sample));
                        index_min = std::min(index_min, sample);
                        index_max = std::max(index_max, sample);
                    }
                    assert(extrema.at("indices").at("minimum").at(0).get<std::uint32_t>() ==
                           index_min);
                    assert(extrema.at("indices").at("maximum").at(0).get<std::uint32_t>() ==
                           index_max);
                }
                std::size_t recorded_vertices = 0U;
                for (const auto& record : gear.at("records")) {
                    assert(record.at("slot").is_string());
                    assert(record.at("id").is_string());
                    recorded_vertices += record.at("vertices").get<std::size_t>();
                }
                assert(recorded_vertices == gear.at("vertices").get<std::size_t>());
                ++fixture_count;
            }
        }
    }
    assert(fixture_count == 36U);

    constexpr std::array animation_states{"REST", "IDLE", "WALK", "RUN", "CROUCH",
                                           "CROUCH_WALK", "SITTING", "PRONE",
                                           "PRONE_MOVE"};
    for (const auto seed : seeds) {
        const std::string stem = "animation-" + std::to_string(seed) + "-v1.gnif";
        const auto binary = fixture_root.parent_path() / stem;
        const Fixture fixture = read(binary, binary.string() + ".json");
        assert(fixture.manifest.at("request").at("count") == 600U);
        assert(fixture.manifest.at("animation").size() == animation_states.size());
        for (const char* state : animation_states) {
            const Stream* translations = fixture.find(
                std::string("animation.") + state + ".translations");
            const Stream* rotations = fixture.find(
                std::string("animation.") + state + ".rotations");
            const Stream* morphs = fixture.find(
                std::string("animation.") + state + ".morphWeights");
            const Stream* foot_goals = fixture.find(
                std::string("animation.") + state + ".footGoals");
            assert(translations != nullptr && translations->element_count == 600U * 69U * 3U);
            assert(rotations != nullptr && rotations->element_count == 600U * 69U * 4U);
            assert(morphs != nullptr && morphs->element_count == 600U * 4U);
            assert(foot_goals != nullptr && foot_goals->element_count == 600U * 2U * 3U);
            assert(compare(*translations, *translations, 0.0).equal);
            assert(compare(*rotations, *rotations, 0.0).equal);
            assert(compare(*morphs, *morphs, 0.0).equal);
        }
    }
    return 0;
}
