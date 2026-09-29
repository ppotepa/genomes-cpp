#include <genomes/infantry/EquipmentCatalog.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>

namespace genomes::infantry {

namespace {

using foundation::StableId;

[[nodiscard]] std::uint32_t equipmentHash(std::uint32_t seed,
                                          std::string_view text) noexcept {
    std::uint32_t hash = 2166136261U ^ seed;
    for (const unsigned char character : text) {
        hash = (hash ^ character) * 16777619U;
    }
    hash = (hash ^ (hash >> 16U)) * 0x7feb352dU;
    hash = (hash ^ (hash >> 15U)) * 0x846ca68bU;
    return hash ^ (hash >> 16U);
}

class EquipmentRandom final {
public:
    explicit EquipmentRandom(std::uint32_t seed) noexcept : state_(seed) {}
    [[nodiscard]] double next() noexcept {
        state_ += 0x6D2B79F5U;
        std::uint32_t value = state_;
        value = (value ^ (value >> 15U)) * (value | 1U);
        value ^= value + (value ^ (value >> 7U)) * (value | 61U);
        return static_cast<double>(value ^ (value >> 14U)) / 4294967296.0;
    }
private:
    std::uint32_t state_{0U};
};

[[nodiscard]] const std::array<EquipmentSlotDefinition, kEquipmentSlotCount>& slotDefinitions() {
    static const std::array<EquipmentSlotDefinition, kEquipmentSlotCount> value{{
        {EquipmentSlot::Head, "head", "", "HEAD"},
        {EquipmentSlot::Face, "face", "", "HEAD_FRONT"},
        {EquipmentSlot::Neck, "neck", "", "NECK"},
        {EquipmentSlot::TorsoBase, "torsoBase", "field_jacket", ""},
        {EquipmentSlot::Legs, "legs", "field_pants", ""},
        {EquipmentSlot::Feet, "feet", "combat_boots", ""},
        {EquipmentSlot::Hands, "hands", "", ""},
        {EquipmentSlot::TorsoArmor, "torsoArmor", "", "CHEST_CENTER"},
        {EquipmentSlot::ChestRig, "chestRig", "", "CHEST_CENTER"},
        {EquipmentSlot::Back, "back", "", "BACK_CENTER"},
        {EquipmentSlot::Belt, "belt", "", "WAIST"},
        {EquipmentSlot::LeftHip, "leftHip", "", "HIP_L"},
        {EquipmentSlot::RightHip, "rightHip", "", "HIP_R"},
        {EquipmentSlot::LeftThigh, "leftThigh", "", "THIGH_L"},
        {EquipmentSlot::RightThigh, "rightThigh", "", "THIGH_R"},
        {EquipmentSlot::Utility1, "utility1", "", "WAIST_FRONT"},
        {EquipmentSlot::Utility2, "utility2", "", "WAIST_BACK"},
        {EquipmentSlot::Utility3, "utility3", "", "CHEST_LEFT"},
        {EquipmentSlot::MeleeWeapon, "meleeWeapon", "", "HIP_R"},
        {EquipmentSlot::Throwable, "throwable", "", "WAIST_FRONT"},
        {EquipmentSlot::PrimaryWeapon, "primaryWeapon", "", "WEAPON_BACK"},
        {EquipmentSlot::SecondaryWeapon, "secondaryWeapon", "", "WEAPON_HIP"},
    }};
    return value;
}

[[nodiscard]] const std::array<EquipmentItemDefinition, kEquipmentItemCount>& itemDefinitions() {
    static const std::array<EquipmentItemDefinition, kEquipmentItemCount> value = [] {
        std::array<EquipmentItemDefinition, kEquipmentItemCount> result{};
        std::size_t cursor = 0U;
        const auto add = [&result, &cursor](std::string_view identifier, EquipmentKind kind,
                                            std::initializer_list<EquipmentSlot> allowed,
                                            float weight, float scale, float thickness,
                                            std::string_view style) {
            EquipmentItemDefinition item{};
            item.id = foundation::stable_id(identifier);
            item.identifier = identifier;
            item.kind = kind;
            item.weight_kg = weight;
            item.fit_scale = scale;
            item.fit_thickness = thickness;
            item.style = style;
            item.visual.style = style;
            if (kind == EquipmentKind::Cap) item.visual.coverage = "cap";
            if (kind == EquipmentKind::Helmet) item.visual.coverage = "helmet";
            if (kind == EquipmentKind::Armor) item.visual.thickness = thickness;
            for (const EquipmentSlot slot : allowed) {
                if (item.allowed_slot_count < item.allowed_slots.size()) {
                    item.allowed_slots[item.allowed_slot_count++] = slot;
                }
            }
            result[cursor++] = item;
        };
        add("field_cap", EquipmentKind::Cap, {EquipmentSlot::Head}, .16F, 1.0F, .006F, "cap");
        add("patrol_cap", EquipmentKind::Cap, {EquipmentSlot::Head}, .19F, 1.01F, .006F, "patrol");
        add("beanie", EquipmentKind::Cap, {EquipmentSlot::Head}, .12F, 1.02F, .008F, "beanie");
        add("beret", EquipmentKind::Cap, {EquipmentSlot::Head}, .13F, 1.01F, .007F, "beret");
        add("boonie_hat", EquipmentKind::Cap, {EquipmentSlot::Head}, .22F, 1.04F, .008F, "boonie");
        add("helmet_light", EquipmentKind::Helmet, {EquipmentSlot::Head}, .95F, 1.03F, .018F, "light");
        add("helmet_standard", EquipmentKind::Helmet, {EquipmentSlot::Head}, 1.3F, 1.04F, .022F, "standard");
        add("helmet_heavy", EquipmentKind::Helmet, {EquipmentSlot::Head}, 1.7F, 1.06F, .032F, "heavy");
        add("helmet_cover", EquipmentKind::Helmet, {EquipmentSlot::Head}, 1.4F, 1.05F, .025F, "cover");
        add("glasses", EquipmentKind::Eyewear, {EquipmentSlot::Face}, .06F, 1.0F, .002F, "glasses");
        add("goggles", EquipmentKind::Eyewear, {EquipmentSlot::Face}, .18F, 1.02F, .004F, "goggles");
        add("balaclava", EquipmentKind::Mask, {EquipmentSlot::Face}, .10F, 1.01F, .003F, "balaclava");
        add("respirator", EquipmentKind::Mask, {EquipmentSlot::Face}, .36F, 1.02F, .012F, "respirator");
        add("scarf", EquipmentKind::Neckwear, {EquipmentSlot::Neck}, .2F, 1.02F, .010F, "scarf");
        add("neck_gaiter", EquipmentKind::Neckwear, {EquipmentSlot::Neck}, .1F, 1.01F, .006F, "gaiter");
        add("field_jacket", EquipmentKind::Clothing, {EquipmentSlot::TorsoBase}, .7F, 1.0F, .012F, "field");
        add("combat_shirt", EquipmentKind::Clothing, {EquipmentSlot::TorsoBase}, .45F, .97F, .008F, "combat");
        add("winter_jacket", EquipmentKind::Clothing, {EquipmentSlot::TorsoBase}, 1.35F, 1.08F, .022F, "winter");
        add("field_pants", EquipmentKind::Clothing, {EquipmentSlot::Legs}, .65F, 1.0F, .010F, "field");
        add("combat_pants", EquipmentKind::Clothing, {EquipmentSlot::Legs}, .8F, 1.025F, .012F, "combat");
        add("winter_pants", EquipmentKind::Clothing, {EquipmentSlot::Legs}, 1.0F, 1.07F, .018F, "winter");
        add("combat_boots", EquipmentKind::Clothing, {EquipmentSlot::Feet}, 1.2F, 1.0F, .025F, "combat");
        add("light_boots", EquipmentKind::Clothing, {EquipmentSlot::Feet}, .9F, .96F, .020F, "light");
        add("heavy_boots", EquipmentKind::Clothing, {EquipmentSlot::Feet}, 1.65F, 1.07F, .034F, "heavy");
        add("winter_boots", EquipmentKind::Clothing, {EquipmentSlot::Feet}, 1.5F, 1.10F, .032F, "winter");
        add("gloves_light", EquipmentKind::Clothing, {EquipmentSlot::Hands}, .08F, 1.0F, .004F, "fingerless");
        add("gloves_full", EquipmentKind::Clothing, {EquipmentSlot::Hands}, .12F, 1.0F, .005F, "full");
        add("gloves_winter", EquipmentKind::Clothing, {EquipmentSlot::Hands}, .2F, 1.03F, .010F, "winter");
        add("light_vest", EquipmentKind::Armor, {EquipmentSlot::TorsoArmor}, 2.5F, 1.01F, .010F, "light");
        add("plate_carrier", EquipmentKind::Armor, {EquipmentSlot::TorsoArmor}, 6.0F, 1.03F, .020F, "plate");
        add("heavy_armor", EquipmentKind::Armor, {EquipmentSlot::TorsoArmor}, 9.5F, 1.06F, .029F, "heavy");
        add("webbing", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, .7F, 1.0F, .006F, "webbing");
        add("chest_standard", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, 1.0F, 1.0F, .009F, "standard");
        add("chest_assault", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, 1.2F, 1.01F, .010F, "assault");
        add("chest_ammo", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, 2.0F, 1.02F, .012F, "ammo");
        add("chest_medical", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, 1.6F, 1.01F, .011F, "medical");
        add("chest_tools", EquipmentKind::Rig, {EquipmentSlot::ChestRig}, 1.9F, 1.02F, .012F, "tools");
        add("pack_small", EquipmentKind::Pack, {EquipmentSlot::Back}, .7F, 1.0F, .030F, "small");
        add("pack_medium", EquipmentKind::Pack, {EquipmentSlot::Back}, 1.2F, 1.05F, .040F, "medium");
        add("pack_large", EquipmentKind::Pack, {EquipmentSlot::Back}, 1.8F, 1.12F, .050F, "large");
        add("pack_medical", EquipmentKind::Pack, {EquipmentSlot::Back}, 3.5F, 1.08F, .048F, "medical");
        add("pack_radio", EquipmentKind::Pack, {EquipmentSlot::Back}, 5.1F, 1.06F, .045F, "radio");
        add("pack_engineer", EquipmentKind::Pack, {EquipmentSlot::Back}, 3.8F, 1.08F, .050F, "engineer");
        add("belt_light", EquipmentKind::Belt, {EquipmentSlot::Belt}, .24F, 1.0F, .008F, "light");
        add("belt_utility", EquipmentKind::Belt, {EquipmentSlot::Belt}, .5F, 1.01F, .010F, "utility");
        add("canteen", EquipmentKind::Pouch,
            {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::Utility1,
             EquipmentSlot::Utility2},
            1.0F, 1.0F, .020F, "canteen");
        add("pouch_utility", EquipmentKind::Pouch, {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::LeftThigh, EquipmentSlot::RightThigh, EquipmentSlot::Utility1, EquipmentSlot::Utility2, EquipmentSlot::Utility3}, .25F, 1.0F, .018F, "utility");
        add("pouch_ammo", EquipmentKind::Pouch, {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::LeftThigh, EquipmentSlot::RightThigh, EquipmentSlot::Utility1, EquipmentSlot::Utility2, EquipmentSlot::Utility3}, .7F, 1.02F, .022F, "ammo");
        add("pouch_medical", EquipmentKind::Pouch, {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::LeftThigh, EquipmentSlot::RightThigh, EquipmentSlot::Utility1, EquipmentSlot::Utility2, EquipmentSlot::Utility3}, .55F, 1.01F, .021F, "medical");
        add("pouch_tools", EquipmentKind::Pouch, {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::LeftThigh, EquipmentSlot::RightThigh, EquipmentSlot::Utility1, EquipmentSlot::Utility2, EquipmentSlot::Utility3}, .9F, 1.03F, .024F, "tools");
        add("radio_handheld", EquipmentKind::Pouch, {EquipmentSlot::Utility3, EquipmentSlot::LeftHip, EquipmentSlot::RightHip}, .3F, 1.0F, .018F, "radio");
        add("binoculars", EquipmentKind::Pouch, {EquipmentSlot::Utility3, EquipmentSlot::Utility1}, .5F, 1.0F, .015F, "binoculars");
        add("map_case", EquipmentKind::Pouch, {EquipmentSlot::LeftHip, EquipmentSlot::RightHip, EquipmentSlot::Utility1}, .2F, 1.0F, .012F, "map");
        add("rifle", EquipmentKind::Weapon, {EquipmentSlot::PrimaryWeapon}, 3.1F, 1.0F, .035F, "rifle");
        add("carbine", EquipmentKind::Weapon, {EquipmentSlot::PrimaryWeapon}, 2.6F, .98F, .032F, "carbine");
        add("support_gun", EquipmentKind::Weapon, {EquipmentSlot::PrimaryWeapon}, 5.2F, 1.04F, .042F, "support");
        add("heavy_support_gun", EquipmentKind::Weapon, {EquipmentSlot::PrimaryWeapon}, 7.0F, 1.08F, .050F, "heavy");
        add("marksman_rifle", EquipmentKind::Weapon, {EquipmentSlot::PrimaryWeapon}, 4.0F, 1.03F, .038F, "marksman");
        add("knife", EquipmentKind::Weapon, {EquipmentSlot::MeleeWeapon}, .25F, 1.0F, .012F, "knife");
        add("grenade", EquipmentKind::Weapon, {EquipmentSlot::Throwable}, .4F, 1.0F, .015F, "grenade");
        add("sidearm", EquipmentKind::Weapon, {EquipmentSlot::SecondaryWeapon}, .85F, 1.0F, .020F, "sidearm");
        const auto configure = [&result](std::string_view id,
                                         auto configure_visual) {
            const auto found = std::find_if(result.begin(), result.end(),
                [id](const EquipmentItemDefinition& item) {
                    return item.identifier == id;
                });
            if (found != result.end()) configure_visual(found->visual);
        };
        for (const auto id : {"field_jacket", "combat_shirt", "winter_jacket",
                              "field_pants", "combat_pants", "winter_pants"}) {
            configure(id, [&result, id](EquipmentVisualDefinition& visual) {
                const auto item = std::find_if(result.begin(), result.end(),
                    [id](const EquipmentItemDefinition& candidate) {
                        return candidate.identifier == id;
                    });
                visual.ease = item->fit_scale;
            });
        }
        configure("combat_pants", [](EquipmentVisualDefinition& visual) { visual.pads = true; });
        configure("combat_boots", [](EquipmentVisualDefinition& visual) {
            visual.width = 1.0F; visual.shaft = 1.0F;
        });
        configure("light_boots", [](EquipmentVisualDefinition& visual) {
            visual.width = 0.96F; visual.shaft = 0.87F;
        });
        configure("heavy_boots", [](EquipmentVisualDefinition& visual) {
            visual.width = 1.07F; visual.shaft = 1.08F;
        });
        configure("winter_boots", [](EquipmentVisualDefinition& visual) {
            visual.width = 1.10F; visual.shaft = 1.04F;
        });
        configure("webbing", [](EquipmentVisualDefinition& visual) { visual.count = 2U; });
        configure("chest_standard", [](EquipmentVisualDefinition& visual) { visual.count = 3U; });
        configure("chest_assault", [](EquipmentVisualDefinition& visual) { visual.count = 4U; });
        configure("chest_ammo", [](EquipmentVisualDefinition& visual) { visual.count = 3U; });
        configure("chest_medical", [](EquipmentVisualDefinition& visual) { visual.count = 2U; });
        configure("chest_tools", [](EquipmentVisualDefinition& visual) { visual.count = 3U; });
        configure("pack_small", [](EquipmentVisualDefinition& visual) { visual.size = {.26F, .32F, .13F}; });
        configure("pack_medium", [](EquipmentVisualDefinition& visual) { visual.size = {.32F, .43F, .19F}; });
        configure("pack_large", [](EquipmentVisualDefinition& visual) {
            visual.size = {.37F, .53F, .22F}; visual.roll = true;
        });
        configure("pack_medical", [](EquipmentVisualDefinition& visual) { visual.size = {.35F, .42F, .19F}; });
        configure("pack_radio", [](EquipmentVisualDefinition& visual) { visual.size = {.31F, .39F, .18F}; });
        configure("pack_engineer", [](EquipmentVisualDefinition& visual) { visual.size = {.33F, .43F, .20F}; });
        return result;
    }();
    return value;
}

[[nodiscard]] const std::array<InfantryLoadout, kInfantryLoadoutCount>& loadouts() {
    static const std::array<InfantryLoadout, kInfantryLoadoutCount> value = [] {
        std::array<InfantryLoadout, kInfantryLoadoutCount> result{};
        constexpr std::array<std::string_view, kInfantryLoadoutCount> names{
            "RIFLEMAN", "ASSAULT", "LIGHT_SUPPORT", "HEAVY_SUPPORT", "MARKSMAN",
            "SCOUT", "MEDIC", "ENGINEER", "RADIO_OPERATOR", "SQUAD_LEADER"};
        for (std::size_t index = 0U; index < result.size(); ++index) {
            result[index].identifier = names[index];
            result[index].id = foundation::stable_id(names[index]);
        }
        const auto id = [](std::string_view name) { return foundation::stable_id(name); };
        const auto set = [&result](std::size_t loadout, EquipmentSlot slot,
                                   std::initializer_list<std::string_view> choices) {
            LoadoutChoice choice{};
            for (const std::string_view item : choices) {
                if (choice.count < choice.definitions.size() && !item.empty()) {
                    choice.definitions[choice.count++] = foundation::stable_id(item);
                } else if (choice.count < choice.definitions.size()) {
                    ++choice.count;
                }
            }
            result[loadout].choices[equipmentSlotIndex(slot)] = choice;
        };
        for (std::size_t index = 0U; index < result.size(); ++index) {
            set(index, EquipmentSlot::TorsoBase, {"field_jacket"});
            set(index, EquipmentSlot::Legs, {"combat_pants"});
            set(index, EquipmentSlot::Feet, {"combat_boots"});
            set(index, EquipmentSlot::Belt, {"belt_utility"});
            set(index, EquipmentSlot::LeftHip, {"canteen"});
            set(index, EquipmentSlot::RightHip, {"pouch_utility"});
        }
        set(0, EquipmentSlot::Head, {"helmet_standard", "helmet_cover"});
        set(0, EquipmentSlot::TorsoArmor, {"light_vest", "plate_carrier"});
        set(0, EquipmentSlot::ChestRig, {"chest_standard"});
        set(0, EquipmentSlot::Back, {"", "", "pack_small"});
        set(0, EquipmentSlot::Hands, {"gloves_light"});
        set(0, EquipmentSlot::PrimaryWeapon, {"rifle"});
        set(1, EquipmentSlot::Head, {"helmet_light"});
        set(1, EquipmentSlot::TorsoBase, {"combat_shirt"});
        set(1, EquipmentSlot::TorsoArmor, {"plate_carrier"});
        set(1, EquipmentSlot::ChestRig, {"chest_assault"});
        set(1, EquipmentSlot::Back, {"", "pack_small"});
        set(1, EquipmentSlot::Face, {"", "goggles"});
        set(1, EquipmentSlot::Hands, {"gloves_full"});
        set(1, EquipmentSlot::PrimaryWeapon, {"carbine"});
        set(2, EquipmentSlot::Head, {"helmet_standard", "helmet_cover"});
        set(2, EquipmentSlot::TorsoArmor, {"plate_carrier"});
        set(2, EquipmentSlot::ChestRig, {"chest_ammo"});
        set(2, EquipmentSlot::Back, {"pack_medium"});
        set(2, EquipmentSlot::LeftThigh, {"pouch_ammo"});
        set(2, EquipmentSlot::Hands, {"gloves_full"});
        set(2, EquipmentSlot::PrimaryWeapon, {"support_gun"});
        set(3, EquipmentSlot::Head, {"helmet_heavy"});
        set(3, EquipmentSlot::TorsoArmor, {"heavy_armor"});
        set(3, EquipmentSlot::ChestRig, {"chest_ammo"});
        set(3, EquipmentSlot::Back, {"pack_large"});
        set(3, EquipmentSlot::LeftThigh, {"pouch_ammo"});
        set(3, EquipmentSlot::RightThigh, {"pouch_ammo"});
        set(3, EquipmentSlot::Feet, {"heavy_boots"});
        set(3, EquipmentSlot::Hands, {"gloves_full"});
        set(3, EquipmentSlot::PrimaryWeapon, {"heavy_support_gun"});
        set(4, EquipmentSlot::Head, {"field_cap", "helmet_light"});
        set(4, EquipmentSlot::TorsoArmor, {"light_vest"});
        set(4, EquipmentSlot::ChestRig, {"webbing"});
        set(4, EquipmentSlot::Back, {"pack_small"});
        set(4, EquipmentSlot::Hands, {"gloves_light"});
        set(4, EquipmentSlot::Utility3, {"binoculars"});
        set(4, EquipmentSlot::PrimaryWeapon, {"marksman_rifle"});
        set(5, EquipmentSlot::Head, {"boonie_hat", "field_cap", "patrol_cap", "beanie"});
        set(5, EquipmentSlot::TorsoBase, {"combat_shirt"});
        set(5, EquipmentSlot::Legs, {"field_pants"});
        set(5, EquipmentSlot::Feet, {"light_boots"});
        set(5, EquipmentSlot::Belt, {"belt_light"});
        set(5, EquipmentSlot::ChestRig, {"webbing"});
        set(5, EquipmentSlot::Back, {"pack_small"});
        set(5, EquipmentSlot::RightHip, {"map_case"});
        set(5, EquipmentSlot::Utility3, {"binoculars"});
        set(5, EquipmentSlot::PrimaryWeapon, {"carbine"});
        set(6, EquipmentSlot::Head, {"helmet_standard"});
        set(6, EquipmentSlot::TorsoArmor, {"light_vest"});
        set(6, EquipmentSlot::ChestRig, {"chest_medical"});
        set(6, EquipmentSlot::Back, {"pack_medical"});
        set(6, EquipmentSlot::RightHip, {"pouch_medical"});
        set(6, EquipmentSlot::LeftThigh, {"pouch_medical"});
        set(6, EquipmentSlot::Hands, {"gloves_full"});
        set(6, EquipmentSlot::PrimaryWeapon, {"carbine"});
        set(7, EquipmentSlot::Head, {"helmet_standard", "helmet_cover"});
        set(7, EquipmentSlot::TorsoArmor, {"plate_carrier"});
        set(7, EquipmentSlot::Face, {"goggles"});
        set(7, EquipmentSlot::ChestRig, {"chest_tools"});
        set(7, EquipmentSlot::Back, {"pack_engineer"});
        set(7, EquipmentSlot::RightHip, {"pouch_tools"});
        set(7, EquipmentSlot::RightThigh, {"pouch_tools"});
        set(7, EquipmentSlot::Feet, {"heavy_boots"});
        set(7, EquipmentSlot::Hands, {"gloves_full"});
        set(7, EquipmentSlot::PrimaryWeapon, {"carbine"});
        set(8, EquipmentSlot::Head, {"helmet_standard"});
        set(8, EquipmentSlot::TorsoArmor, {"light_vest"});
        set(8, EquipmentSlot::ChestRig, {"chest_standard"});
        set(8, EquipmentSlot::Back, {"pack_radio"});
        set(8, EquipmentSlot::Utility3, {"radio_handheld"});
        set(8, EquipmentSlot::Hands, {"gloves_light"});
        set(8, EquipmentSlot::PrimaryWeapon, {"rifle"});
        set(9, EquipmentSlot::Head, {"helmet_cover", "beret"});
        set(9, EquipmentSlot::TorsoArmor, {"plate_carrier"});
        set(9, EquipmentSlot::ChestRig, {"chest_standard"});
        set(9, EquipmentSlot::Back, {"pack_small"});
        set(9, EquipmentSlot::RightHip, {"map_case"});
        set(9, EquipmentSlot::Utility3, {"radio_handheld"});
        set(9, EquipmentSlot::Utility1, {"binoculars"});
        set(9, EquipmentSlot::Hands, {"gloves_light"});
        set(9, EquipmentSlot::PrimaryWeapon, {"carbine"});
        set(9, EquipmentSlot::SecondaryWeapon, {"sidearm"});
        (void)id;
        return result;
    }();
    return value;
}

[[nodiscard]] const EquipmentSlotDefinition* slotFor(EquipmentSlot slot) noexcept {
    const auto& definitions = slotDefinitions();
    return &definitions[equipmentSlotIndex(slot)];
}

} // namespace

bool EquipmentItemDefinition::allows(EquipmentSlot slot) const noexcept {
    for (std::size_t index = 0U; index < allowed_slot_count; ++index) {
        if (allowed_slots[index] == slot) {
            return true;
        }
    }
    return false;
}

std::span<const EquipmentSlotDefinition> EquipmentCatalog::slots() noexcept {
    const auto& value = slotDefinitions();
    return {value.data(), value.size()};
}

std::span<const EquipmentItemDefinition> EquipmentCatalog::items() noexcept {
    const auto& value = itemDefinitions();
    return {value.data(), value.size()};
}

const EquipmentSlotDefinition* EquipmentCatalog::findSlot(StableId id) noexcept {
    for (const auto& slot : slotDefinitions()) {
        if (foundation::stable_id(slot.identifier) == id) {
            return &slot;
        }
    }
    return nullptr;
}

const EquipmentItemDefinition* EquipmentCatalog::findItem(StableId id) noexcept {
    for (const auto& item : itemDefinitions()) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

const EquipmentItemDefinition* EquipmentCatalog::findItem(std::string_view identifier) noexcept {
    return findItem(foundation::stable_id(identifier));
}

StableId EquipmentCatalog::slotId(EquipmentSlot slot) noexcept {
    const EquipmentSlotDefinition* definition = slotFor(slot);
    return definition == nullptr ? 0 : foundation::stable_id(definition->identifier);
}

StableId EquipmentCatalog::loadoutId(std::string_view identifier) noexcept {
    return foundation::stable_id(identifier);
}

std::span<const InfantryLoadout> infantryLoadouts() noexcept {
    const auto& value = loadouts();
    return {value.data(), value.size()};
}

const InfantryLoadout* findInfantryLoadout(StableId id) noexcept {
    for (const auto& loadout : loadouts()) {
        if (loadout.id == id) {
            return &loadout;
        }
    }
    return nullptr;
}

std::size_t EquipmentState::count() const noexcept {
    std::size_t result = 0U;
    for (const auto& slot : slots) {
        result += slot.has_value() ? 1U : 0U;
    }
    return result;
}

const EquipmentItem* EquipmentState::item(EquipmentSlot slot) const noexcept {
    const auto& value = slots[equipmentSlotIndex(slot)];
    return value.has_value() ? &value.value() : nullptr;
}

bool EquipmentState::valid() const noexcept {
    if (version == 0U || identity == 0 || !std::isfinite(total_weight_kg) || total_weight_kg < 0.0F) {
        return false;
    }
    float total = 0.0F;
    for (std::size_t index = 0U; index < slots.size(); ++index) {
        if (!slots[index].has_value()) {
            continue;
        }
        const EquipmentItem& item = slots[index].value();
        const EquipmentItemDefinition* definition = EquipmentCatalog::findItem(item.definition_id);
        if (definition == nullptr || !definition->allows(static_cast<EquipmentSlot>(index)) ||
            item.slot != static_cast<EquipmentSlot>(index) || !std::isfinite(item.weight_kg) ||
            item.weight_kg < 0.0F || !std::isfinite(item.variant.size) ||
            !std::isfinite(item.variant.shade) || !std::isfinite(item.variant.detail)) {
            return false;
        }
        total += item.weight_kg;
    }
    return std::abs(total - total_weight_kg) <= 1.0e-4F;
}

foundation::Result<EquipmentState, foundation::Error> EquipmentResolver::resolve(
    proc::Seed unit_seed,
    StableId loadout_id,
    const EquipmentOverrideSet& overrides) {
    const InfantryLoadout* loadout = loadout_id == 0 ? nullptr : findInfantryLoadout(loadout_id);
    if (loadout_id != 0 && loadout == nullptr) {
        return foundation::Result<EquipmentState, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "unknown infantry equipment loadout"});
    }
    const std::uint32_t equipment_seed = equipmentHash(
        static_cast<std::uint32_t>(unit_seed), "EQUIPMENT/v1");
    EquipmentState state{};
    state.unit_seed = unit_seed;
    state.equipment_seed = equipment_seed;
    state.loadout_id = loadout_id;
    state.revision = 1U;

    for (std::size_t index = 0U; index < kEquipmentSlotCount; ++index) {
        const EquipmentSlot slot = static_cast<EquipmentSlot>(index);
        const EquipmentSlotDefinition* slot_definition = slotFor(slot);
        const LoadoutChoice choice = loadout == nullptr ? LoadoutChoice{}
                                                         : loadout->choices[index];
        StableId selected = 0;
        if (choice.count != 0U) {
            EquipmentRandom random(equipmentHash(
                equipment_seed, std::string("slot/") + std::string(slot_definition->identifier)));
            const std::uint32_t selected_index = static_cast<std::uint32_t>(
                random.next() * static_cast<double>(choice.count));
            if (selected_index < choice.count) {
                selected = choice.definitions[selected_index];
            }
        }
        const EquipmentOverride& override = overrides.slots[index];
        if (override.specified) {
            selected = override.empty ? 0 : override.definition_id;
        }
        if (selected == 0 && slot_definition != nullptr && !slot_definition->required_item.empty()) {
            selected = foundation::stable_id(slot_definition->required_item);
        }
        if (selected == 0) {
            continue;
        }
        const EquipmentItemDefinition* definition = EquipmentCatalog::findItem(selected);
        if (definition == nullptr) {
            return foundation::Result<EquipmentState, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "unknown equipment item"});
        }
        if (!definition->allows(slot)) {
            return foundation::Result<EquipmentState, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "equipment item is incompatible with slot"});
        }
        const std::uint32_t variant_seed = equipmentHash(
            equipment_seed, std::string(slot_definition->identifier) + "/" +
                                std::string(definition->identifier));
        EquipmentRandom variant_random(variant_seed);
        EquipmentItem item{};
        item.definition_id = definition->id;
        item.slot = slot;
        item.seed = variant_seed;
        item.variant = {static_cast<float>(.965 + variant_random.next() * .07),
                        static_cast<float>(.92 + variant_random.next() * .13),
                        static_cast<float>(variant_random.next())};
        item.weight_kg = definition->weight_kg;
        state.slots[index] = item;
        state.total_weight_kg += item.weight_kg;
    }

    StableId identity = foundation::stable_id("infantry.equipment.state.v1");
    identity = foundation::stableHashCombine(identity, unit_seed);
    identity = foundation::stableHashCombine(identity, loadout_id);
    for (const auto& slot : state.slots) {
        identity = foundation::stableHashCombine(identity, slot.has_value() ? slot->definition_id : 0U);
        if (slot.has_value()) {
            identity = foundation::stableHashCombine(identity, slot->seed);
            identity = foundation::stableHashCombine(identity, foundation::stableHashFloat(slot->variant.size));
            identity = foundation::stableHashCombine(identity, foundation::stableHashFloat(slot->variant.shade));
            identity = foundation::stableHashCombine(identity, foundation::stableHashFloat(slot->variant.detail));
        }
    }
    state.identity = identity;
    return state.valid()
               ? foundation::Result<EquipmentState, foundation::Error>::success(std::move(state))
               : foundation::Result<EquipmentState, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "equipment state failed validation"});
}

} // namespace genomes::infantry
