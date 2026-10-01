#include <genomes/destruction/MaterialAssembly.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <set>
#include <string_view>

namespace genomes::destruction {

namespace {

constexpr float kLayerTolerance = 1.0e-5F;

constexpr std::string_view kMaterialCatalogSchema =
    "genomes.destruction.material-catalog.v1";
constexpr std::string_view kMaterialCatalogId = "destruction.materials";

[[nodiscard]] foundation::Error catalogError(foundation::ErrorCode code,
                                              std::string_view message) noexcept {
    return {code, message};
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                         foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return dot(value, value);
}

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] MaterialDefinition definition(std::string_view name,
                                             float density,
                                             float strength,
                                             float penetration_work,
                                             float toughness,
                                             float ricochet,
                                             float spall_threshold,
                                             MaterialResponse response) {
    return {MaterialId::fromName(name), std::string{name}, density, strength, penetration_work,
            toughness, ricochet, spall_threshold, response,
            std::string{"SOURCE destruction calibration"}};
}

[[nodiscard]] std::optional<MaterialResponse> responseFromJson(
    const nlohmann::json& value) {
    if (!value.is_string()) {
        return std::nullopt;
    }
    const auto response = value.get<std::string>();
    if (response == "brittle") return MaterialResponse::Brittle;
    if (response == "ductile") return MaterialResponse::Ductile;
    if (response == "fibrous") return MaterialResponse::Fibrous;
    if (response == "soft") return MaterialResponse::Soft;
    return std::nullopt;
}

[[nodiscard]] foundation::SimConfigHash materialFingerprint(
    const std::vector<MaterialDefinition>& definitions) noexcept {
    std::uint64_t hash = foundation::stableHashString(kMaterialCatalogSchema);
    for (const auto& value : definitions) {
        hash = foundation::stableHashCombine(hash, value.id.value);
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.name));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.density_kg_m3));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.strength_pa));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.penetration_work_j_m3));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.toughness_j_m2));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.ricochet_factor));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.spall_threshold_j));
        hash = foundation::stableHashCombine(
            hash, static_cast<std::uint64_t>(value.response));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.provenance));
    }
    return {hash};
}

[[nodiscard]] std::uint64_t fieldCell(foundation::Vec3 local,
                                      PhysicalSolidId solid) noexcept {
    constexpr float cell_size = 0.25F;
    const auto quantize = [](float value) {
        return static_cast<std::int64_t>(std::floor(value / cell_size));
    };
    std::uint64_t hash = foundation::stableHashCombine(
        static_cast<std::uint64_t>(quantize(local.x)), solid.value);
    hash = foundation::stableHashCombine(hash,
                                         static_cast<std::uint64_t>(quantize(local.y)));
    hash = foundation::stableHashCombine(hash,
                                         static_cast<std::uint64_t>(quantize(local.z)));
    return hash == 0 ? 1 : hash;
}

} // namespace

bool MaterialDefinition::valid() const noexcept {
    return static_cast<bool>(id) && !name.empty() && std::isfinite(density_kg_m3) &&
           density_kg_m3 > 0.0F && std::isfinite(strength_pa) && strength_pa > 0.0F &&
           std::isfinite(penetration_work_j_m3) && penetration_work_j_m3 > 0.0F &&
           std::isfinite(toughness_j_m2) && toughness_j_m2 >= 0.0F &&
           std::isfinite(ricochet_factor) && ricochet_factor >= 0.0F &&
           ricochet_factor <= 1.0F && std::isfinite(spall_threshold_j) &&
           spall_threshold_j >= 0.0F && !provenance.empty();
}

bool MaterialCatalog::add(MaterialDefinition definition_value) noexcept {
    if (frozen_ || !definition_value.valid() || find(definition_value.id) != nullptr) {
        return false;
    }
    definitions_.push_back(definition_value);
    return true;
}

bool MaterialCatalog::freeze() noexcept {
    if (frozen_) {
        return true;
    }
    std::sort(definitions_.begin(), definitions_.end(),
              [](const MaterialDefinition& left, const MaterialDefinition& right) {
                  return left.id.value < right.id.value;
              });
    frozen_ = true;
    return true;
}

const MaterialDefinition* MaterialCatalog::find(MaterialId id) const noexcept {
    const auto iterator = frozen_
                              ? std::lower_bound(
                                    definitions_.begin(), definitions_.end(), id,
                                    [](const MaterialDefinition& definition_value,
                                       MaterialId value) {
                                        return definition_value.id.value < value.value;
                                    })
                              : std::find_if(
                                    definitions_.begin(), definitions_.end(),
                                    [id](const MaterialDefinition& definition_value) {
                                        return definition_value.id == id;
                                    });
    return iterator == definitions_.end() || iterator->id != id ? nullptr : &*iterator;
}

MaterialCatalog MaterialCatalog::makeDefault() {
    MaterialCatalog catalog;
    (void)catalog.add(definition("concrete", 2400.0F, 45.0e6F, 600.0e6F, 80'000.0F,
                                 0.18F, 150.0F, MaterialResponse::Brittle));
    (void)catalog.add(definition("brick", 1800.0F, 16.0e6F, 160.0e6F, 40'000.0F,
                                 0.12F, 80.0F, MaterialResponse::Brittle));
    (void)catalog.add(definition("steel", 7850.0F, 600.0e6F, 10.0e9F, 250'000.0F,
                                 0.30F, 0.0F, MaterialResponse::Ductile));
    (void)catalog.add(definition("glass", 2500.0F, 5.0e6F, 20.0e6F, 700.0F,
                                 0.04F, 0.0F, MaterialResponse::Brittle));
    (void)catalog.add(definition("wood", 650.0F, 8.0e6F, 40.0e6F, 12'000.0F,
                                 0.08F, 0.0F, MaterialResponse::Fibrous));
    (void)catalog.add(definition("foliage", 100.0F, 10'000.0F, 10'000.0F, 30.0F,
                                 0.0F, 0.0F, MaterialResponse::Soft));
    (void)catalog.add(definition("rock", 2700.0F, 100.0e6F, 800.0e6F, 180'000.0F,
                                 0.24F, 300.0F, MaterialResponse::Brittle));
    (void)catalog.add(definition("tissue", 1000.0F, 0.6e6F, 0.6e6F, 2200.0F,
                                 0.0F, 0.0F, MaterialResponse::Soft));
    (void)catalog.add(definition("armor", 3000.0F, 350.0e6F, 8.0e9F, 100'000.0F,
                                 0.20F, 0.0F, MaterialResponse::Ductile));
    (void)catalog.freeze();
    return catalog;
}

foundation::Result<MaterialCatalog, foundation::Error> MaterialCatalog::load(
    const std::filesystem::path& path) {
    auto document = content::readContentText(path);
    if (!document) {
        return foundation::Result<MaterialCatalog, foundation::Error>::failure(document.error());
    }
    try {
        const nlohmann::json json = nlohmann::json::parse(document.value().text);
        static const std::set<std::string> fields{"schema", "schema_version", "id", "entries"};
        if (!json.is_object() || json.size() != fields.size()) {
            return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "unknown or missing material catalog fields"));
        }
        for (const auto& [key, value] : json.items()) {
            (void)value;
            if (!fields.contains(key)) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "unknown material catalog field"));
            }
        }
        if (json.at("schema").get<std::string>() != kMaterialCatalogSchema ||
            json.at("schema_version") != MaterialCatalogVersion ||
            json.at("id").get<std::string>() != kMaterialCatalogId ||
            !json.at("entries").is_array() ||
            json.at("entries").empty()) {
            return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "invalid material catalog header"));
        }

        static const std::set<std::string> entry_fields{
            "id", "density_kg_m3", "strength_pa", "penetration_work_j_m3",
            "toughness_j_m2", "ricochet_factor", "spall_threshold_j", "response",
            "provenance"};
        MaterialCatalog catalog;
        for (const auto& item : json.at("entries")) {
            if (!item.is_object() || item.size() != entry_fields.size()) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid material catalog entry fields"));
            }
            for (const auto& [key, value] : item.items()) {
                (void)value;
                if (!entry_fields.contains(key)) {
                    return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                        catalogError(foundation::ErrorCode::InvalidArgument,
                                     "unknown material catalog entry field"));
                }
            }
            const auto& name = item.at("id");
            const auto& provenance = item.at("provenance");
            if (!name.is_string() || !provenance.is_string() ||
                name.get<std::string>().empty() || provenance.get<std::string>().empty()) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid material catalog identity"));
            }
            const auto response = responseFromJson(item.at("response"));
            if (!response.has_value()) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid material catalog response"));
            }
            const auto number = [&item](std::string_view key) {
                return item.at(std::string{key}).is_number();
            };
            if (!number("density_kg_m3") || !number("strength_pa") ||
                !number("penetration_work_j_m3") || !number("toughness_j_m2") ||
                !number("ricochet_factor") || !number("spall_threshold_j")) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid material catalog numeric value"));
            }
            const std::string name_value = name.get<std::string>();
            const MaterialDefinition definition_value{
                MaterialId::fromName(name_value), name_value,
                item.at("density_kg_m3").get<float>(), item.at("strength_pa").get<float>(),
                item.at("penetration_work_j_m3").get<float>(),
                item.at("toughness_j_m2").get<float>(), item.at("ricochet_factor").get<float>(),
                item.at("spall_threshold_j").get<float>(), *response,
                provenance.get<std::string>()};
            if (!catalog.add(definition_value)) {
                return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid or duplicate material catalog entry"));
            }
        }
        if (!catalog.freeze()) {
            return foundation::Result<MaterialCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidState,
                             "material catalog could not be frozen"));
        }

        document.value().provenance.source_id = json.at("id").get<std::string>();
        content::ContentSnapshotBuilder snapshot_builder{
            json.at("id").get<std::string>(), MaterialCatalogVersion};
        auto added = snapshot_builder.add(std::move(document.value().provenance));
        if (!added) {
            return foundation::Result<MaterialCatalog, foundation::Error>::failure(added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<MaterialCatalog, foundation::Error>::failure(snapshot.error());
        }
        catalog.snapshot_ = std::move(snapshot.value());
        catalog.fingerprint_ = materialFingerprint(catalog.definitions_);
        return foundation::Result<MaterialCatalog, foundation::Error>::success(std::move(catalog));
    } catch (const std::exception&) {
        return foundation::Result<MaterialCatalog, foundation::Error>::failure(
            catalogError(foundation::ErrorCode::InvalidArgument,
                         "invalid material catalog document"));
    }
}

bool MaterialFrame::valid(float tolerance) const noexcept {
    if (!finite(origin) || !finite(normal) || !finite(tangent) || !finite(bitangent) ||
        !std::isfinite(tolerance) || tolerance <= 0.0F) {
        return false;
    }
    const float normal_length = length_squared(normal);
    const float tangent_length = length_squared(tangent);
    const float bitangent_length = length_squared(bitangent);
    return std::abs(normal_length - 1.0F) <= tolerance &&
           std::abs(tangent_length - 1.0F) <= tolerance &&
           std::abs(bitangent_length - 1.0F) <= tolerance &&
           std::abs(dot(normal, tangent)) <= tolerance &&
           std::abs(dot(normal, bitangent)) <= tolerance &&
           std::abs(dot(tangent, bitangent)) <= tolerance;
}

foundation::Vec3 MaterialFrame::worldToMaterial(foundation::Vec3 point) const noexcept {
    const foundation::Vec3 relative = subtract(point, origin);
    return {dot(relative, tangent), dot(relative, normal), dot(relative, bitangent)};
}

foundation::Vec3 MaterialFrame::materialToWorld(foundation::Vec3 point) const noexcept {
    return {origin.x + tangent.x * point.x + normal.x * point.y + bitangent.x * point.z,
            origin.y + tangent.y * point.x + normal.y * point.y + bitangent.y * point.z,
            origin.z + tangent.z * point.x + normal.z * point.y + bitangent.z * point.z};
}

foundation::Result<MaterialAssembly, foundation::Error> MaterialAssembly::create(
    proc::Seed root_seed,
    MaterialFrame frame,
    std::vector<Layer> layers,
    const MaterialCatalog& catalog) {
    MaterialAssembly assembly;
    assembly.root_seed_ = root_seed == 0 ? 1 : root_seed;
    assembly.frame_ = frame;
    assembly.layers_ = std::move(layers);
    const auto validation = assembly.validate(catalog);
    if (!validation) {
        return foundation::Result<MaterialAssembly, foundation::Error>::failure(
            validation.error());
    }
    return foundation::Result<MaterialAssembly, foundation::Error>::success(std::move(assembly));
}

foundation::Result<void, foundation::Error> MaterialAssembly::validate(
    const MaterialCatalog& catalog) const noexcept {
    if (version_ != MaterialAssemblyVersion || root_seed_ == 0 || !frame_.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid material assembly header"});
    }
    if (layers_.empty()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "material assembly has no layers"});
    }
    float previous_end = -std::numeric_limits<float>::infinity();
    for (const Layer& layer : layers_) {
        if (!std::isfinite(layer.start) || !std::isfinite(layer.end) ||
            layer.start < previous_end - kLayerTolerance || layer.end < layer.start ||
            layer.start < 0.0F) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid material layer interval"});
        }
        if (layer.isVoid()) {
            if (layer.material || layer.physical_solid) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidArgument, "void layer carries material identity"});
            }
        } else {
            if (!layer.material || !layer.physical_solid || catalog.find(layer.material) == nullptr) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "material layer references unknown identity"});
            }
        }
        previous_end = std::max(previous_end, layer.end);
    }
    return foundation::Result<void, foundation::Error>::success();
}

std::optional<std::size_t> MaterialAssembly::layerIndexAt(float distance) const noexcept {
    if (!std::isfinite(distance) || distance < 0.0F) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        const Layer& layer = layers_[index];
        const bool final_layer = index + 1U == layers_.size();
        if (distance >= layer.start - kLayerTolerance &&
            (distance < layer.end - kLayerTolerance ||
             (final_layer && distance <= layer.end + kLayerTolerance))) {
            return index;
        }
        if (distance < layer.start) {
            break;
        }
    }
    return std::nullopt;
}

std::vector<TraversalSegment> MaterialAssembly::traverse(float start, float end) const {
    std::vector<TraversalSegment> result;
    if (!std::isfinite(start) || !std::isfinite(end)) {
        return result;
    }
    const bool reversed = end < start;
    const float low = std::min(start, end);
    const float high = std::max(start, end);
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        const Layer& layer = layers_[index];
        const float segment_start = std::max(low, layer.start);
        const float segment_end = std::min(high, layer.end);
        if (segment_end <= segment_start + kLayerTolerance) {
            continue;
        }
        result.push_back({index, layer, reversed ? segment_end : segment_start,
                          reversed ? segment_start : segment_end, reversed});
    }
    if (reversed) {
        std::reverse(result.begin(), result.end());
    }
    return result;
}

std::vector<InterfaceGroup> MaterialAssembly::interfaceGroups() const {
    std::vector<InterfaceGroup> result;
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        const Layer& layer = layers_[index];
        if (layer.isVoid() || !layer.physical_solid) {
            continue;
        }
        if (!result.empty()) {
            InterfaceGroup& previous = result.back();
            const Layer& previous_layer = layers_[previous.layer_indices.back()];
            if (previous.physical_solid == layer.physical_solid &&
                layer.start <= previous_layer.end + kLayerTolerance) {
                previous.end = std::max(previous.end, layer.end);
                previous.layer_indices.push_back(index);
                continue;
            }
        }
        result.push_back({layer.physical_solid, layer.start, layer.end, {index}});
    }
    return result;
}

MaterialFieldSample MaterialAssembly::sample(foundation::Vec3 world_point) const noexcept {
    MaterialFieldSample result{};
    result.local_position = frame_.worldToMaterial(world_point);
    const auto layer_index = layerIndexAt(result.local_position.y);
    if (!layer_index.has_value()) {
        return result;
    }
    const Layer& layer = layers_[*layer_index];
    result.material = layer.material;
    result.physical_solid = layer.physical_solid;
    result.void_layer = layer.isVoid();
    if (result.void_layer) {
        return result;
    }
    result.cell_seed = fieldCell(result.local_position, result.physical_solid);
    const proc::SeedPath field_path =
        proc::SeedPath(root_seed_).childStableId("material-field", result.physical_solid.value)
            .child("cell", result.cell_seed);
    proc::RandomStream random(field_path);
    result.density_scale = static_cast<float>(random.uniformRange(0.95, 1.05));
    result.strength_scale = static_cast<float>(random.uniformRange(0.92, 1.08));
    result.roughness = static_cast<float>(random.uniform01());
    return result;
}

} // namespace genomes::destruction
