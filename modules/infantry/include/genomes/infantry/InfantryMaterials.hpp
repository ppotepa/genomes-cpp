#pragma once

#include <cstdint>
#include <string_view>

namespace genomes::infantry {

enum class AppearanceMaterialRegion : std::uint16_t {
    UniformCloth = 0,
    Skin = 1,
    Eyelid = 2,
    Mouth = 3,
    Hair = 4,
    EyeSclera = 5,
    Iris = 6,
    Pupil = 7,
    Ear = 8,
    Nose = 9,
    Lip = 10,
    Trousers = 11,
    SkinHand = 12,
    BootLeather = 13,
    EquipmentCloth = 14,
    EquipmentLeather = 15,
    EquipmentMetal = 16,
    EquipmentPaint = 17,
};

[[nodiscard]] constexpr std::string_view materialRegionName(
    AppearanceMaterialRegion region) noexcept {
    switch (region) {
    case AppearanceMaterialRegion::UniformCloth: return "uniformCloth";
    case AppearanceMaterialRegion::Skin: return "skin";
    case AppearanceMaterialRegion::Eyelid: return "eyelid";
    case AppearanceMaterialRegion::Mouth: return "mouth";
    case AppearanceMaterialRegion::Hair: return "hair";
    case AppearanceMaterialRegion::EyeSclera: return "eyeSclera";
    case AppearanceMaterialRegion::Iris: return "iris";
    case AppearanceMaterialRegion::Pupil: return "pupil";
    case AppearanceMaterialRegion::Ear: return "ear";
    case AppearanceMaterialRegion::Nose: return "nose";
    case AppearanceMaterialRegion::Lip: return "lip";
    case AppearanceMaterialRegion::Trousers: return "trousers";
    case AppearanceMaterialRegion::SkinHand: return "skinHand";
    case AppearanceMaterialRegion::BootLeather: return "bootLeather";
    case AppearanceMaterialRegion::EquipmentCloth: return "equipmentCloth";
    case AppearanceMaterialRegion::EquipmentLeather: return "equipmentLeather";
    case AppearanceMaterialRegion::EquipmentMetal: return "equipmentMetal";
    case AppearanceMaterialRegion::EquipmentPaint: return "equipmentPaint";
    }
    return "unknown";
}

} // namespace genomes::infantry
