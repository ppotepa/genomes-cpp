#include <genomes/combat/TacticalAI.hpp>

#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <exception>
#include <set>

namespace genomes::combat {

foundation::Result<TacticalAIProfileSnapshot, foundation::Error> loadTacticalAIProfile(
    const std::filesystem::path& path) {
    try {
        auto document = content::readContentText(path);
        if (!document) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                document.error());
        }
        const nlohmann::json json = nlohmann::json::parse(document.value().text);
        static const std::set<std::string> fields{
            "schema_version", "id", "observation_period_ticks", "memory_ticks",
            "target_switch_ratio", "fire_alignment_cos"};
        if (!json.is_object() || json.size() != fields.size()) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "unknown or missing tactical AI profile fields"});
        }
        for (const auto& [key, value] : json.items()) {
            (void)value;
            if (!fields.contains(key)) {
                return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidArgument, "unknown tactical AI profile field"});
            }
        }
        if (json.at("schema_version") != 1 || !json.at("id").is_string() ||
            json.at("id").get<std::string>().empty() ||
            !json.at("observation_period_ticks").is_number_unsigned() ||
            !json.at("memory_ticks").is_number_unsigned() ||
            !json.at("target_switch_ratio").is_number() ||
            !json.at("fire_alignment_cos").is_number()) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid tactical AI profile schema"});
        }
        TacticalAIProfileSnapshot result{};
        result.id = json.at("id").get<std::string>();
        result.source = path;
        result.profile.observation_period_ticks = json.at("observation_period_ticks").get<std::uint32_t>();
        result.profile.memory_ticks = json.at("memory_ticks").get<std::uint32_t>();
        result.profile.target_switch_ratio = json.at("target_switch_ratio").get<float>();
        result.profile.fire_alignment_cos = json.at("fire_alignment_cos").get<float>();
        if (!result.profile.valid()) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid tactical AI profile values"});
        }
        document.value().provenance.source_id = result.id;
        content::ContentSnapshotBuilder snapshot_builder{"combat.tactical-ai", 1U};
        if (auto added = snapshot_builder.add(std::move(document.value().provenance)); !added) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                snapshot.error());
        }
        result.content = std::move(snapshot.value());
        const std::array<foundation::CanonicalConfigField, 5U> fingerprint_fields{{
            {"id", foundation::stableHashString(result.id)},
            {"observation_period_ticks", result.profile.observation_period_ticks},
            {"memory_ticks", result.profile.memory_ticks},
            {"target_switch_ratio",
             foundation::stableHashFloat(result.profile.target_switch_ratio)},
            {"fire_alignment_cos", foundation::stableHashFloat(result.profile.fire_alignment_cos)},
        }};
        result.fingerprint = foundation::makeSimConfigHash(
            "combat.tactical-ai.v1", fingerprint_fields);
        return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::success(
            std::move(result));
    } catch (const std::exception&) {
        return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid tactical AI profile document"});
    }
}

} // namespace genomes::combat
