#include <genomes/destruction/RubbleField.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::int64_t floor_div(std::int64_t value, std::int64_t divisor) noexcept {
    const std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

[[nodiscard]] std::int64_t floor_mod(std::int64_t value, std::int64_t divisor) noexcept {
    const std::int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

[[nodiscard]] std::int64_t cell_key(float value, float cell_size) noexcept {
    return static_cast<std::int64_t>(std::floor(value / cell_size));
}

struct Candidate final {
    RubbleField::CellLocation location{};
    double weight{0.0};
};

} // namespace

RubbleTile::RubbleTile(TileCoord coordinate, std::uint32_t cells_per_side)
    : coordinate_{coordinate}, cells_per_side_{cells_per_side},
      total_volume_(static_cast<std::size_t>(cells_per_side) * cells_per_side, 0.0) {}

double RubbleTile::cellVolume(std::uint32_t x, std::uint32_t z) const noexcept {
    if (x >= cells_per_side_ || z >= cells_per_side_) {
        return 0.0;
    }
    return total_volume_[index(x, z)];
}

double RubbleTile::materialVolume(std::uint32_t x,
                                  std::uint32_t z,
                                  MaterialId material) const noexcept {
    if (x >= cells_per_side_ || z >= cells_per_side_) {
        return 0.0;
    }
    const std::size_t material_index = materialIndex(material);
    return material_index == material_ids_.size()
               ? 0.0
               : material_volumes_[material_index * total_volume_.size() + index(x, z)];
}

std::size_t RubbleTile::index(std::uint32_t x, std::uint32_t z) const noexcept {
    return static_cast<std::size_t>(z) * cells_per_side_ + x;
}

std::size_t RubbleTile::materialIndex(MaterialId material) const noexcept {
    const auto iterator = std::find(material_ids_.begin(), material_ids_.end(), material);
    return iterator == material_ids_.end()
               ? material_ids_.size()
               : static_cast<std::size_t>(iterator - material_ids_.begin());
}

std::size_t RubbleTile::ensureMaterial(MaterialId material) {
    const std::size_t existing = materialIndex(material);
    if (existing != material_ids_.size()) {
        return existing;
    }
    material_ids_.push_back(material);
    material_volumes_.resize(material_ids_.size() * total_volume_.size(), 0.0);
    return material_ids_.size() - 1U;
}

void RubbleTile::add(std::uint32_t x,
                     std::uint32_t z,
                     MaterialId material,
                     double volume) {
    if (!material || volume <= 0.0 || x >= cells_per_side_ || z >= cells_per_side_) {
        return;
    }
    const std::size_t cell_index = index(x, z);
    const std::size_t material_index = ensureMaterial(material);
    total_volume_[cell_index] += volume;
    material_volumes_[material_index * total_volume_.size() + cell_index] += volume;
    total_tile_volume_ += volume;
    dirty_ = true;
}

double RubbleTile::remove(std::uint32_t x,
                          std::uint32_t z,
                          MaterialId material,
                          double volume) noexcept {
    if (!material || volume <= 0.0 || x >= cells_per_side_ || z >= cells_per_side_) {
        return 0.0;
    }
    const std::size_t material_index = materialIndex(material);
    if (material_index == material_ids_.size()) {
        return 0.0;
    }
    const std::size_t cell_index = index(x, z);
    double& channel = material_volumes_[material_index * total_volume_.size() + cell_index];
    const double removed = std::min(channel, volume);
    channel -= removed;
    total_volume_[cell_index] = std::max(0.0, total_volume_[cell_index] - removed);
    total_tile_volume_ = std::max(0.0, total_tile_volume_ - removed);
    if (removed > 0.0) {
        dirty_ = true;
    }
    return removed;
}

bool RubbleFieldSpec::valid() const noexcept {
    return std::isfinite(cell_size_m) && cell_size_m > 0.0F && std::isfinite(tile_size_m) &&
           tile_size_m > 0.0F && cells_per_tile > 0 && cells_per_tile <= 128 &&
           std::abs(tile_size_m - cell_size_m * static_cast<float>(cells_per_tile)) <= 1.0e-3F &&
           max_tiles > 0 && max_tiles <= 1'000'000U && max_chunk_instances > 0 &&
           std::isfinite(max_slope) && max_slope > 0.0F && std::isfinite(packing_factor) &&
           packing_factor > 0.0F && packing_factor <= 1.0F && relaxation_iterations <= 64;
}

double RubbleDeposit::totalVolume() const noexcept {
    double result = 0.0;
    for (const MaterialVolume& volume : material_volumes) {
        result += volume.volume;
    }
    return result;
}

foundation::Result<RubbleField, foundation::Error> RubbleField::create(
    RubbleFieldSpec spec, TerrainHeightProvider terrain) {
    if (!spec.valid() || (terrain.sample != nullptr && terrain.context == nullptr)) {
        return foundation::Result<RubbleField, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid rubble field specification"});
    }
    RubbleField field;
    field.spec_ = spec;
    field.terrain_ = terrain;
    return foundation::Result<RubbleField, foundation::Error>::success(std::move(field));
}

foundation::Result<RubbleDepositResult, foundation::Error> RubbleField::deposit(
    const RubbleDeposit& deposit_value) {
    if (deposit_value.source_id == 0 || !finite(deposit_value.position) ||
        !std::isfinite(deposit_value.radius_m) || deposit_value.radius_m < 0.0F ||
        deposit_value.material_volumes.empty()) {
        return foundation::Result<RubbleDepositResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid rubble deposit"});
    }
    const double volume = deposit_value.totalVolume();
    if (!std::isfinite(volume) || volume <= 0.0) {
        return foundation::Result<RubbleDepositResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "rubble deposit has no material volume"});
    }
    for (const MaterialVolume& material : deposit_value.material_volumes) {
        if (!material.material || !std::isfinite(material.volume) || material.volume <= 0.0) {
            return foundation::Result<RubbleDepositResult, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid rubble material volume"});
        }
    }

    const CellLocation center = locate(deposit_value.position.x, deposit_value.position.z);
    const std::int64_t radius_cells = static_cast<std::int64_t>(std::ceil(
        deposit_value.radius_m / spec_.cell_size_m));
    std::vector<Candidate> candidates;
    for (std::int64_t z = center.global.z - radius_cells;
         z <= center.global.z + radius_cells; ++z) {
        for (std::int64_t x = center.global.x - radius_cells;
             x <= center.global.x + radius_cells; ++x) {
            const float world_x = (static_cast<float>(x) + 0.5F) * spec_.cell_size_m;
            const float world_z = (static_cast<float>(z) + 0.5F) * spec_.cell_size_m;
            const float dx = world_x - deposit_value.position.x;
            const float dz = world_z - deposit_value.position.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (deposit_value.radius_m > 0.0F && distance > deposit_value.radius_m) {
                continue;
            }
            const double weight = deposit_value.radius_m <= 0.0F
                                      ? 1.0
                                      : std::pow(std::max(0.0F,
                                                          1.0F - distance / deposit_value.radius_m),
                                                 2.0F);
            if (weight > 0.0) {
                candidates.push_back({locate(world_x, world_z), weight});
            }
        }
    }
    if (candidates.empty()) {
        candidates.push_back({center, 1.0});
    }
    double total_weight = 0.0;
    for (const Candidate& candidate : candidates) {
        total_weight += candidate.weight;
    }
    if (total_weight <= 0.0) {
        return foundation::Result<RubbleDepositResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "rubble deposit kernel has zero weight"});
    }

    RubbleDepositResult result{};
    for (const Candidate& candidate : candidates) {
        bool merged = false;
        RubbleTile* target = tileForCell(candidate.location.global, &merged);
        if (target == nullptr) {
            return foundation::Result<RubbleDepositResult, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "rubble tile capacity could not be resolved"});
        }
        result.tile_overflow_merged = result.tile_overflow_merged || merged;
        const std::int64_t base_x = static_cast<std::int64_t>(target->coordinate().x) *
                                    spec_.cells_per_tile;
        const std::int64_t base_z = static_cast<std::int64_t>(target->coordinate().z) *
                                    spec_.cells_per_tile;
        const std::uint32_t local_x = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
            candidate.location.global.x - base_x, 0, spec_.cells_per_tile - 1U));
        const std::uint32_t local_z = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
            candidate.location.global.z - base_z, 0, spec_.cells_per_tile - 1U));
        const double cell_volume = volume * candidate.weight / total_weight;
        for (const MaterialVolume& material : deposit_value.material_volumes) {
            target->add(local_x, local_z, material.material,
                        cell_volume * material.volume / volume);
        }
        markDirty(target->coordinate());
        result.deposited_volume += cell_volume;
        ++result.affected_cells;
    }
    return foundation::Result<RubbleDepositResult, foundation::Error>::success(std::move(result));
}

foundation::Result<RubbleRemoveResult, foundation::Error> RubbleField::remove(
    const RubbleRemoveRequest& request) {
    if (!finite(request.position) || !std::isfinite(request.radius_m) || request.radius_m < 0.0F ||
        !std::isfinite(request.volume) || request.volume <= 0.0) {
        return foundation::Result<RubbleRemoveResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid rubble removal request"});
    }
    const CellLocation center = locate(request.position.x, request.position.z);
    const std::int64_t radius_cells = static_cast<std::int64_t>(std::ceil(
        request.radius_m / spec_.cell_size_m));
    struct RemovalCandidate final {
        CellLocation location;
        double weight;
        double available;
    };
    std::vector<RemovalCandidate> candidates;
    double weighted_available = 0.0;
    for (std::int64_t z = center.global.z - radius_cells;
         z <= center.global.z + radius_cells; ++z) {
        for (std::int64_t x = center.global.x - radius_cells;
             x <= center.global.x + radius_cells; ++x) {
            const float world_x = (static_cast<float>(x) + 0.5F) * spec_.cell_size_m;
            const float world_z = (static_cast<float>(z) + 0.5F) * spec_.cell_size_m;
            const float dx = world_x - request.position.x;
            const float dz = world_z - request.position.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (request.radius_m > 0.0F && distance > request.radius_m) {
                continue;
            }
            const double weight = request.radius_m <= 0.0F
                                      ? 1.0
                                      : std::pow(std::max(0.0F,
                                                          1.0F - distance / request.radius_m),
                                                 2.0F);
            const CellLocation location = locate(world_x, world_z);
            const double available = cellVolume(location.global);
            if (weight > 0.0 && available > 0.0) {
                candidates.push_back({location, weight, available});
                weighted_available += available * weight;
            }
        }
    }
    RubbleRemoveResult result{};
    const double target = std::min(request.volume, weighted_available);
    double removed_so_far = 0.0;
    for (const RemovalCandidate& candidate : candidates) {
        const double desired = (&candidate == &candidates.back())
                                   ? target - removed_so_far
                                   : target * candidate.available * candidate.weight /
                                         std::max(1.0e-12, weighted_available);
        if (desired <= 0.0) {
            continue;
        }
        RubbleTile* target_tile = tile(candidate.location.tile);
        if (target_tile == nullptr) {
            continue;
        }
        const std::size_t cell_index = static_cast<std::size_t>(candidate.location.local_z) *
                                           spec_.cells_per_tile + candidate.location.local_x;
        const std::vector<MaterialId> material_ids = target_tile->material_ids_;
        double removed_cell = 0.0;
        for (const MaterialId material : material_ids) {
            const double available_material = target_tile->material_volumes_[
                target_tile->materialIndex(material) * target_tile->cellCount() + cell_index];
            const double share = candidate.available > 0.0
                                     ? desired * available_material / candidate.available
                                     : 0.0;
            const double removed = target_tile->remove(candidate.location.local_x,
                                                       candidate.location.local_z, material, share);
            removed_cell += removed;
            const auto iterator = std::find_if(
                result.material_volumes.begin(), result.material_volumes.end(),
                [material](const MaterialVolume& current) { return current.material == material; });
            if (iterator == result.material_volumes.end()) {
                result.material_volumes.push_back({material, removed});
            } else {
                iterator->volume += removed;
            }
        }
        removed_so_far += removed_cell;
        markDirty(target_tile->coordinate());
        if (removed_so_far >= target - 1.0e-12) {
            break;
        }
    }
    result.removed_volume = removed_so_far;
    std::sort(result.material_volumes.begin(), result.material_volumes.end(),
              [](const MaterialVolume& left, const MaterialVolume& right) {
                  return left.material.value < right.material.value;
              });
    return foundation::Result<RubbleRemoveResult, foundation::Error>::success(std::move(result));
}

RubbleRelaxResult RubbleField::relax() noexcept {
    struct Transfer final {
        CellCoord from;
        CellCoord to;
        double volume{0.0};
    };
    RubbleRelaxResult result{};
    const double cell_area = static_cast<double>(spec_.cell_size_m) * spec_.cell_size_m;
    for (std::uint32_t iteration = 0; iteration < spec_.relaxation_iterations; ++iteration) {
        std::vector<Transfer> transfers;
        for (const auto& [coordinate, tile_value] : tiles_) {
            for (std::uint32_t z = 0; z < spec_.cells_per_tile; ++z) {
                for (std::uint32_t x = 0; x < spec_.cells_per_tile; ++x) {
                    const CellCoord from{static_cast<std::int64_t>(coordinate.x) *
                                             spec_.cells_per_tile + x,
                                         static_cast<std::int64_t>(coordinate.z) *
                                             spec_.cells_per_tile + z};
                    const CellCoord neighbors[] = {{from.x + 1, from.z}, {from.x, from.z + 1}};
                    for (const CellCoord to : neighbors) {
                        const double from_height = cellSurfaceHeight(from);
                        const double to_height = cellSurfaceHeight(to);
                        const double difference = from_height - to_height;
                        const double reverse_difference = -difference;
                        const CellCoord source = difference >= reverse_difference ? from : to;
                        const CellCoord destination = difference >= reverse_difference ? to : from;
                        const double slope = std::abs(difference) / spec_.cell_size_m;
                        if (slope <= spec_.max_slope) {
                            continue;
                        }
                        const double available = cellVolume(source);
                        const double excess_height = slope - spec_.max_slope;
                        const double amount = std::min(
                            available * 0.25,
                            excess_height * cell_area * spec_.packing_factor * 0.25);
                        if (amount > 0.0) {
                            transfers.push_back({source, destination, amount});
                        }
                    }
                }
            }
        }
        if (transfers.empty()) {
            break;
        }
        for (const Transfer& transfer : transfers) {
            RubbleTile* source_tile = tileForCell(transfer.from, nullptr);
            RubbleTile* destination_tile = tileForCell(transfer.to, nullptr);
            if (source_tile == nullptr || destination_tile == nullptr) {
                continue;
            }
            const std::uint32_t source_x = static_cast<std::uint32_t>(floor_mod(
                transfer.from.x, spec_.cells_per_tile));
            const std::uint32_t source_z = static_cast<std::uint32_t>(floor_mod(
                transfer.from.z, spec_.cells_per_tile));
            const std::uint32_t destination_x = static_cast<std::uint32_t>(floor_mod(
                transfer.to.x, spec_.cells_per_tile));
            const std::uint32_t destination_z = static_cast<std::uint32_t>(floor_mod(
                transfer.to.z, spec_.cells_per_tile));
            const double actual_amount = std::min(transfer.volume, source_tile->cellVolume(source_x, source_z));
            if (actual_amount <= 0.0) {
                continue;
            }
            const std::size_t source_cell = static_cast<std::size_t>(source_z) *
                                                spec_.cells_per_tile + source_x;
            const std::vector<MaterialId> materials = source_tile->material_ids_;
            const double source_total = source_tile->cellVolume(source_x, source_z);
            for (const MaterialId material : materials) {
                const double material_amount = source_tile->material_volumes_[
                    source_tile->materialIndex(material) * source_tile->cellCount() + source_cell];
                const double moved = source_total > 0.0 ? actual_amount * material_amount / source_total
                                                        : 0.0;
                (void)source_tile->remove(source_x, source_z, material, moved);
                destination_tile->add(destination_x, destination_z, material, moved);
            }
            markDirty(source_tile->coordinate());
            markDirty(destination_tile->coordinate());
            ++result.transfers;
            result.transferred_volume += actual_amount;
        }
    }
    return result;
}

float RubbleField::pileHeightAt(float x, float z) const noexcept {
    const CellLocation location = locate(x, z);
    const double volume = cellVolume(location.global);
    const double cell_area = static_cast<double>(spec_.cell_size_m) * spec_.cell_size_m;
    return static_cast<float>(volume / std::max(1.0e-9, cell_area * spec_.packing_factor));
}

float RubbleField::surfaceHeightAt(float x, float z) const noexcept {
    return static_cast<float>(baseHeight(x, z)) + pileHeightAt(x, z);
}

float RubbleField::pileSlopeAt(float x, float z) const noexcept {
    const float center = surfaceHeightAt(x, z);
    const float offset = spec_.cell_size_m;
    const float slopes[] = {
        std::abs(surfaceHeightAt(x + offset, z) - center) / offset,
        std::abs(surfaceHeightAt(x - offset, z) - center) / offset,
        std::abs(surfaceHeightAt(x, z + offset) - center) / offset,
        std::abs(surfaceHeightAt(x, z - offset) - center) / offset};
    return *std::max_element(std::begin(slopes), std::end(slopes));
}

float RubbleField::movementCostAt(float x, float z) const noexcept {
    const float slope = pileSlopeAt(x, z);
    return 1.0F + pileHeightAt(x, z) * 0.25F + slope * 2.0F;
}

RubbleNavigationSample RubbleField::sampleNavigation(float x, float z) const noexcept {
    const float slope = pileSlopeAt(x, z);
    return {surfaceHeightAt(x, z), movementCostAt(x, z), slope > spec_.max_slope * 1.8F};
}

RubbleCoverSample RubbleField::coverAt(float x, float z) const noexcept {
    const float height = pileHeightAt(x, z);
    return {height, std::clamp(height / 1.5F, 0.0F, 1.0F), height > 0.001F};
}

double RubbleField::totalVolume() const noexcept {
    double total = 0.0;
    for (const auto& [coordinate, tile_value] : tiles_) {
        (void)coordinate;
        total += tile_value.totalVolume();
    }
    return total;
}

double RubbleField::materialVolume(MaterialId material) const noexcept {
    double total = 0.0;
    for (const auto& [coordinate, tile_value] : tiles_) {
        (void)coordinate;
        for (std::uint32_t z = 0; z < spec_.cells_per_tile; ++z) {
            for (std::uint32_t x = 0; x < spec_.cells_per_tile; ++x) {
                total += tile_value.materialVolume(x, z, material);
            }
        }
    }
    return total;
}

std::vector<TileCoord> RubbleField::takeDirtyTiles() noexcept {
    std::vector<TileCoord> result = std::move(dirty_tiles_);
    dirty_tiles_.clear();
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    for (const TileCoord coordinate : result) {
        if (RubbleTile* tile_value = tile(coordinate)) {
            tile_value->clearDirty();
        }
    }
    return result;
}

RubbleField::CellLocation RubbleField::locate(float x, float z) const noexcept {
    const CellCoord global{cell_key(x, spec_.cell_size_m), cell_key(z, spec_.cell_size_m)};
    const std::int64_t divisor = static_cast<std::int64_t>(spec_.cells_per_tile);
    const TileCoord tile_coordinate{static_cast<std::int32_t>(floor_div(global.x, divisor)),
                                    static_cast<std::int32_t>(floor_div(global.z, divisor))};
    return {global, tile_coordinate,
            static_cast<std::uint32_t>(floor_mod(global.x, divisor)),
            static_cast<std::uint32_t>(floor_mod(global.z, divisor))};
}

RubbleTile* RubbleField::ensureTile(TileCoord coordinate, bool* merged) {
    if (merged != nullptr) {
        *merged = false;
    }
    const auto existing = tiles_.find(coordinate);
    if (existing != tiles_.end()) {
        return &existing->second;
    }
    if (tiles_.size() < spec_.max_tiles) {
        return &tiles_.emplace(coordinate, RubbleTile{coordinate, spec_.cells_per_tile})
                    .first->second;
    }
    if (tiles_.empty()) {
        return nullptr;
    }
    auto victim = std::min_element(
        tiles_.begin(), tiles_.end(), [](const auto& left, const auto& right) {
            if (left.second.totalVolume() != right.second.totalVolume()) {
                return left.second.totalVolume() < right.second.totalVolume();
            }
            return left.first < right.first;
        });
    if (tiles_.size() == 1U) {
        if (merged != nullptr) {
            *merged = true;
        }
        return &victim->second;
    }
    auto target = tiles_.end();
    double best_distance = std::numeric_limits<double>::max();
    for (auto iterator = tiles_.begin(); iterator != tiles_.end(); ++iterator) {
        if (iterator == victim) {
            continue;
        }
        const double dx = static_cast<double>(iterator->first.x - victim->first.x);
        const double dz = static_cast<double>(iterator->first.z - victim->first.z);
        const double distance = dx * dx + dz * dz;
        if (target == tiles_.end() || distance < best_distance ||
            (distance == best_distance && iterator->first < target->first)) {
            target = iterator;
            best_distance = distance;
        }
    }
    const TileCoord target_coordinate = target->first;
    mergeTile(victim->first, target_coordinate);
    if (merged != nullptr) {
        *merged = true;
    }
    const auto result = tiles_.find(target_coordinate);
    return result == tiles_.end() ? nullptr : &result->second;
}

RubbleTile* RubbleField::tile(TileCoord coordinate) noexcept {
    const auto iterator = tiles_.find(coordinate);
    return iterator == tiles_.end() ? nullptr : &iterator->second;
}

const RubbleTile* RubbleField::tile(TileCoord coordinate) const noexcept {
    const auto iterator = tiles_.find(coordinate);
    return iterator == tiles_.end() ? nullptr : &iterator->second;
}

void RubbleField::markDirty(TileCoord coordinate) {
    if (std::find(dirty_tiles_.begin(), dirty_tiles_.end(), coordinate) == dirty_tiles_.end()) {
        dirty_tiles_.push_back(coordinate);
    }
}

void RubbleField::mergeTile(TileCoord source, TileCoord target) {
    if (source == target) {
        return;
    }
    auto source_iterator = tiles_.find(source);
    auto target_iterator = tiles_.find(target);
    if (source_iterator == tiles_.end() || target_iterator == tiles_.end()) {
        return;
    }
    RubbleTile& source_tile = source_iterator->second;
    RubbleTile& target_tile = target_iterator->second;
    for (std::uint32_t z = 0; z < spec_.cells_per_tile; ++z) {
        for (std::uint32_t x = 0; x < spec_.cells_per_tile; ++x) {
            const std::int64_t global_x = static_cast<std::int64_t>(source.x) *
                                              spec_.cells_per_tile + x;
            const std::int64_t global_z = static_cast<std::int64_t>(source.z) *
                                              spec_.cells_per_tile + z;
            const std::uint32_t target_x = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
                global_x - static_cast<std::int64_t>(target.x) * spec_.cells_per_tile, 0,
                static_cast<std::int64_t>(spec_.cells_per_tile) - 1));
            const std::uint32_t target_z = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
                global_z - static_cast<std::int64_t>(target.z) * spec_.cells_per_tile, 0,
                static_cast<std::int64_t>(spec_.cells_per_tile) - 1));
            const std::size_t source_cell = source_tile.index(x, z);
            const std::vector<MaterialId> materials = source_tile.material_ids_;
            for (const MaterialId material : materials) {
                const double amount = source_tile.material_volumes_[
                    source_tile.materialIndex(material) * source_tile.cellCount() + source_cell];
                (void)source_tile.remove(x, z, material, amount);
                target_tile.add(target_x, target_z, material, amount);
            }
        }
    }
    markDirty(target);
    tiles_.erase(source_iterator);
}

double RubbleField::baseHeight(float x, float z) const noexcept {
    return terrain_.sample == nullptr ? 0.0 : terrain_.sample(terrain_.context, x, z);
}

double RubbleField::cellSurfaceHeight(CellCoord coordinate) const noexcept {
    const float x = (static_cast<float>(coordinate.x) + 0.5F) * spec_.cell_size_m;
    const float z = (static_cast<float>(coordinate.z) + 0.5F) * spec_.cell_size_m;
    return baseHeight(x, z) + pileHeightAt(x, z);
}

double RubbleField::cellVolume(CellCoord coordinate) const noexcept {
    const std::int64_t divisor = static_cast<std::int64_t>(spec_.cells_per_tile);
    const TileCoord tile_coordinate{static_cast<std::int32_t>(floor_div(coordinate.x, divisor)),
                                    static_cast<std::int32_t>(floor_div(coordinate.z, divisor))};
    const RubbleTile* tile_value = tile(tile_coordinate);
    if (tile_value == nullptr) {
        return 0.0;
    }
    const std::uint32_t local_x = static_cast<std::uint32_t>(floor_mod(coordinate.x, divisor));
    const std::uint32_t local_z = static_cast<std::uint32_t>(floor_mod(coordinate.z, divisor));
    return tile_value->cellVolume(local_x, local_z);
}

RubbleTile* RubbleField::tileForCell(CellCoord coordinate, bool* merged) {
    const std::int64_t divisor = static_cast<std::int64_t>(spec_.cells_per_tile);
    const TileCoord tile_coordinate{static_cast<std::int32_t>(floor_div(coordinate.x, divisor)),
                                    static_cast<std::int32_t>(floor_div(coordinate.z, divisor))};
    return ensureTile(tile_coordinate, merged);
}

} // namespace genomes::destruction
