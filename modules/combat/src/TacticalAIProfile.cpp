#include <genomes/combat/TacticalAI.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <exception>
#include <fstream>
#include <set>

namespace genomes::combat {

foundation::Result<TacticalAIProfileSnapshot, foundation::Error> loadTacticalAIProfile(
    const std::filesystem::path& path) {
    try {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return foundation::Result<TacticalAIProfileSnapshot, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "tactical AI profile cannot be opened"});
        }
        const nlohmann::json json = nlohmann::json::parse(stream);
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
