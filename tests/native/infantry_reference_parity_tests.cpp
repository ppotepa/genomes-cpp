#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/InfantryDamage.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/ReferenceBodySurfaceGenerator.hpp>
#include <genomes/infantry/ReferenceFaceSurfaceGenerator.hpp>
#include <genomes/infantry/RagdollSchema.hpp>
#include <genomes/infantry/RigBuilder.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include "infantry_fixture_reader.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

[[nodiscard]] genomes::foundation::Color referenceSrgb(std::uint32_t hex) {
    const auto channel=[](std::uint32_t value){
        const double srgb=static_cast<double>(value)/255.0;
        return static_cast<float>(srgb<0.04045?srgb*0.0773993808:
            std::pow(srgb*0.9478672986+0.0521327014,2.4));
    };
    return {channel((hex>>16U)&0xffU),channel((hex>>8U)&0xffU),
            channel(hex&0xffU),1.0F};
}

[[nodiscard]] float fixtureNumber(const std::string& text, std::size_t capture_start,
                                  std::string_view field) {
    const std::size_t field_start = text.find('"' + std::string(field) + '"', capture_start);
    assert(field_start != std::string::npos);
    const std::size_t colon = text.find(':', field_start);
    assert(colon != std::string::npos);
    char* end = nullptr;
    const float value = std::strtof(text.c_str() + colon + 1U, &end);
    assert(end != text.c_str() + colon + 1U);
    return value;
}

[[nodiscard]] std::array<double, 21U> bodyValues(
    const genomes::infantry::BodyPhenotype& body) {
    return {body.frame, body.mass, body.muscle, body.fat, body.torso_leg_bias,
            body.shoulder_width_scale, body.hip_width_scale, body.chest_width_scale,
            body.chest_depth_scale, body.waist_width_scale, body.waist_depth_scale,
            body.arm_thickness_scale, body.leg_thickness_scale, body.neck_scale,
            body.leg_length_scale, body.arm_length_scale, body.hip_y, body.head_scale,
            body.hand_scale, body.foot_scale, static_cast<double>(body.skin_color_hex)};
}

[[nodiscard]] std::array<double, 65U> faceValues(
    const genomes::infantry::FacePhenotype& face) {
    return {face.head_width_scale, face.head_depth_scale, face.head_length_scale,
            face.forehead_width_scale, face.forehead_slope, face.temple_width_scale,
            face.brow_ridge, face.jaw_width_scale, face.jaw_length_scale, face.jaw_angle,
            face.chin_width_scale, face.chin_height, face.chin_projection,
            face.cheekbone_scale, face.cheekbone_y, face.cheek_fullness,
            face.midface_projection, face.eye_spacing_ratio, face.eye_width_scale,
            face.eye_height_scale, face.eye_size_scale, face.eye_roundness, face.eye_depth,
            face.eye_tilt, face.eye_y_ratio, static_cast<double>(face.eye_color_hex),
            face.brow_y_ratio, face.brow_thickness, face.brow_tilt, face.brow_spacing,
            face.nose_width_scale, face.nose_length_scale, face.nose_projection_scale,
            face.nose_bridge_scale, face.nose_tip_width_scale, face.nose_tip_rotation,
            face.nostril_width_scale, face.mouth_width_ratio, face.upper_lip, face.lower_lip,
            face.mouth_y_ratio, face.ear_scale, face.ear_angle,
            static_cast<double>(face.hair_color_hex), static_cast<double>(face.hair_style),
            face.hair_density, face.hair_thickness, face.hair_volume, face.hairline_ratio,
            face.temple_recession, face.widow_peak, face.neutral_eye_open, face.neutral_brow,
            face.neutral_mouth, face.eye_asymmetry, face.brow_asymmetry,
            face.mouth_asymmetry, face.ear_asymmetry, face.blink_interval,
            face.blink_duration, face.gaze_restlessness, face.expression_scale,
            face.eye_expression_scale, face.mouth_expression_scale,
            face.brow_expression_scale};
}

[[nodiscard]] bool compareGearFixture(
    const genomes::infantry::AppearanceMesh& mesh,
    const genomes::test::infantry_fixture::Fixture& fixture,
    std::string_view label,
    double normal_tolerance = 2e-5) {
    using genomes::infantry::kInvalidBoneIndex;
    if(!std::isfinite(mesh.minimum.x)||!std::isfinite(mesh.minimum.y)||
       !std::isfinite(mesh.minimum.z)||!std::isfinite(mesh.maximum.x)||
       !std::isfinite(mesh.maximum.y)||!std::isfinite(mesh.maximum.z)||
       !std::isfinite(mesh.sphere_center.x)||!std::isfinite(mesh.sphere_center.y)||
       !std::isfinite(mesh.sphere_center.z)||!std::isfinite(mesh.sphere_radius)){
        std::cerr<<"gear fixture non-finite bounds fixture="<<label<<'\n';return false;}
    const auto stream=[&](std::string_view name,auto value_at,std::size_t count,double tolerance){
        const auto* expected=fixture.find(name);if(expected==nullptr||expected->element_count!=count){
            std::cerr<<"gear fixture stream shape mismatch fixture="<<label<<" stream="<<name<<'\n';return false;}
        for(std::size_t index=0;index<count;++index){float reference{};
            std::memcpy(&reference,expected->bytes.data()+index*sizeof(float),sizeof(float));
            const double actual=value_at(index);if(std::abs(actual-reference)>tolerance){
                std::cerr<<"gear fixture stream mismatch fixture="<<label<<" stream="<<name
                    <<" element="<<index<<" expected="<<reference<<" actual="<<actual<<'\n';return false;}}
        return true;};
    if(!stream("gear.positions",[&](std::size_t i){const auto& v=mesh.vertices[i/3U].position;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},mesh.vertices.size()*3U,2e-6)||
       !stream("gear.normals",[&](std::size_t i){const auto& v=mesh.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},mesh.vertices.size()*3U,normal_tolerance)||
       !stream("gear.colors",[&](std::size_t i){const auto& v=mesh.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},mesh.vertices.size()*3U,2e-6)||
       !stream("gear.uvs",[&](std::size_t i){const auto& v=mesh.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},mesh.vertices.size()*2U,2e-6)||
       !stream("gear.skinIndices",[&](std::size_t i){const auto& v=mesh.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},mesh.vertices.size()*4U,0.0)||
       !stream("gear.skinWeights",[&](std::size_t i){return mesh.vertices[i/4U].influences[i%4U].weight;},mesh.vertices.size()*4U,2e-6))return false;
    const auto* expected_indices=fixture.find("gear.indices");
    if(expected_indices==nullptr||expected_indices->element_count!=mesh.indices.size()){
        std::cerr<<"gear fixture index shape mismatch fixture="<<label<<'\n';return false;}
    for(std::size_t index=0;index<mesh.indices.size();++index){std::uint32_t reference{};
        std::memcpy(&reference,expected_indices->bytes.data()+index*sizeof(reference),sizeof(reference));
        if(reference!=mesh.indices[index]){std::cerr<<"gear fixture index mismatch fixture="<<label
            <<" element="<<index<<" expected="<<reference<<" actual="<<mesh.indices[index]<<'\n';return false;}}
    const auto& expected=fixture.manifest.at("gear");
    if(expected.at("groups").size()!=mesh.groups.size()||expected.at("tags").size()!=mesh.tags.size()){
        std::cerr<<"gear fixture metadata shape mismatch fixture="<<label<<'\n';return false;}
    for(std::size_t index=0;index<mesh.groups.size();++index){const auto& group=expected.at("groups").at(index);
        if(group.at("start")!=mesh.groups[index].start||group.at("count")!=mesh.groups[index].count||
           group.at("material")!=mesh.groups[index].material){std::cerr<<"gear fixture group mismatch fixture="
               <<label<<" group="<<index<<'\n';return false;}}
    for(const auto& tag:mesh.tags){if(!expected.at("tags").contains(tag.name)||
        expected.at("tags").at(tag.name).size()!=tag.vertices.size()){
            std::cerr<<"gear fixture tag shape mismatch fixture="<<label<<" tag="<<tag.name<<'\n';return false;}
        for(std::size_t index=0;index<tag.vertices.size();++index)
            if(expected.at("tags").at(tag.name).at(index)!=tag.vertices[index]){
                std::cerr<<"gear fixture tag mismatch fixture="<<label<<" tag="<<tag.name
                    <<" element="<<index<<'\n';return false;}}
    return true;
}

} // namespace

int main() {
    using namespace genomes::infantry;
    {
        constexpr std::array<std::string_view,8> weapon_ids{"knife","grenade","sidearm",
            "carbine","rifle","marksman_rifle","support_gun","heavy_support_gun"};
        for(const auto id:weapon_ids){
            std::ifstream weapon_file(std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/weapon-"+std::string(id)+"-v1.gnif.json");
            const auto fixture=nlohmann::json::parse(weapon_file);
            const auto& expected=fixture.at("weapon").at("definition");
            const auto* actual=genomes::weapons::WeaponCatalog::find(id);
            if(actual==nullptr){std::cerr<<"missing weapon definition id="<<id<<'\n';return 2;}
            const std::string_view family=actual->family==genomes::weapons::WeaponFamily::OneHanded
                ?"ONE_HANDED":"TWO_HANDED";
            constexpr std::array<std::string_view,4> kind_names{"knife","grenade","pistol","long"};
            const std::uint8_t expected_mask=expected.at("profiles").size()==2U?3U:
                expected.at("profiles").at(0).get<std::string>()=="2H"?2U:1U;
            const auto close=[](double a,double b){return std::abs(a-b)<=1e-6;};
            if(expected.at("id").get<std::string>()!=actual->identifier ||
               expected.at("family").get<std::string>()!=family ||
               expected.at("kind").get<std::string>()!=kind_names[static_cast<std::size_t>(actual->visual_kind)] ||
               expected.at("firearm").get<bool>()!=actual->firearm || expected_mask!=actual->grip_profile_mask ||
               !close(expected.at("length").get<double>(),actual->visual_length) ||
               !close(expected.at("draw").get<double>(),actual->draw_seconds) ||
               !close(expected.at("holster").get<double>(),actual->holster_seconds) ||
               !close(expected.value("interval",0.0),actual->fire_interval_seconds) ||
               !close(expected.value("kick",0.0),actual->visual_kick)){
                std::cerr<<"weapon catalog mismatch id="<<id<<'\n';return 2;
            }
            {
                const auto native=genomes::weapons::WeaponGeometryGenerator::build(*actual);
                const auto binary=std::string(GENOMES_SOURCE_DIR)+
                    "/reference/fixtures/infantry/weapon-"+std::string(id)+"-v1.gnif";
                const auto gnif=genomes::test::infantry_fixture::read(binary,binary+".json");
                if(!native){std::cerr<<"weapon geometry build failed id="<<id<<'\n';return 2;}
                const auto compare_float_stream=[&](std::string_view name,auto value_at,std::size_t count,double tolerance){
                    const auto* stream=gnif.find(name);if(stream==nullptr||stream->element_count!=count){std::cerr<<"weapon stream shape mismatch id="<<id<<" stream="<<name<<'\n';return false;}
                    double maximum=0;std::size_t first=count;double first_reference=0,first_actual=0;for(std::size_t i=0;i<count;++i){float reference{};std::memcpy(&reference,stream->bytes.data()+i*sizeof(float),sizeof(float));const double actual_value=value_at(i),error=std::abs(actual_value-reference);maximum=std::max(maximum,error);
                        if(error>tolerance&&first==count){first=i;first_reference=reference;first_actual=actual_value;}}
                    if(first!=count){std::cerr<<"weapon stream mismatch id="<<id<<" stream="<<name<<" element="<<first<<" expected="<<first_reference<<" actual="<<first_actual<<" maximum="<<maximum<<'\n';return false;}
                    return true;};
                const auto& mesh=native.value().mesh;
                if(!compare_float_stream("weapon.primary.positions",[&](std::size_t i){const auto& v=mesh.vertices[i/3].position;return i%3==0?v.x:i%3==1?v.y:v.z;},mesh.vertices.size()*3U,2e-6) ||
                   !compare_float_stream("weapon.primary.normals",[&](std::size_t i){const auto& v=mesh.vertices[i/3].normal;return i%3==0?v.x:i%3==1?v.y:v.z;},mesh.vertices.size()*3U,2e-5) ||
                   !compare_float_stream("weapon.primary.colors",[&](std::size_t i){const auto& v=mesh.vertices[i/3].color;return i%3==0?v.r:i%3==1?v.g:v.b;},mesh.vertices.size()*3U,2e-6) ||
                   !compare_float_stream("weapon.primary.uvs",[&](std::size_t i){const auto& v=mesh.vertices[i/2].uv;return i%2==0?v.x:v.y;},mesh.vertices.size()*2U,2e-6))return 2;
                const auto* indices=gnif.find("weapon.primary.indices");
                if(indices==nullptr||indices->element_count!=mesh.indices.size()){std::cerr<<"weapon index shape mismatch id="<<id<<'\n';return 2;}
                for(std::size_t i=0;i<mesh.indices.size();++i){std::uint32_t reference{};std::memcpy(&reference,indices->bytes.data()+i*sizeof(reference),sizeof(reference));if(reference!=mesh.indices[i]){std::cerr<<"weapon index mismatch id="<<id<<" element="<<i<<'\n';return 2;}}
                if(actual->visual_kind==genomes::weapons::WeaponVisualKind::Pistol){const auto& slide=native.value().slide;
                    if(!compare_float_stream("weapon.slide.positions",[&](std::size_t i){const auto& v=slide.vertices[i/3].position;return i%3==0?v.x:i%3==1?v.y:v.z;},slide.vertices.size()*3U,2e-6) ||
                       !compare_float_stream("weapon.slide.normals",[&](std::size_t i){const auto& v=slide.vertices[i/3].normal;return i%3==0?v.x:i%3==1?v.y:v.z;},slide.vertices.size()*3U,2e-5) ||
                       !compare_float_stream("weapon.slide.colors",[&](std::size_t i){const auto& v=slide.vertices[i/3].color;return i%3==0?v.r:i%3==1?v.g:v.b;},slide.vertices.size()*3U,2e-6) ||
                       !compare_float_stream("weapon.slide.uvs",[&](std::size_t i){const auto& v=slide.vertices[i/2].uv;return i%2==0?v.x:v.y;},slide.vertices.size()*2U,2e-6))return 2;
                    const auto* slide_indices=gnif.find("weapon.slide.indices");if(slide_indices==nullptr||slide_indices->element_count!=slide.indices.size()){std::cerr<<"weapon slide index shape mismatch\n";return 2;}for(std::size_t i=0;i<slide.indices.size();++i){std::uint32_t reference{};std::memcpy(&reference,slide_indices->bytes.data()+i*sizeof(reference),sizeof(reference));if(reference!=slide.indices[i]){std::cerr<<"weapon slide index mismatch element="<<i<<'\n';return 2;}}
                }
                if(actual->firearm&&
                   (native.value().muzzle_flash.vertices.size()!=40U||
                    native.value().muzzle_flash.indices.size()/3U!=48U)){
                    std::cerr<<"weapon muzzle flash topology mismatch id="<<id<<'\n';return 2;}
                if(actual->firearm){const auto& flash=native.value().muzzle_flash;
                    if(!compare_float_stream("weapon.muzzleFlash.positions",[&](std::size_t i){const auto& v=flash.vertices[i/3].position;return i%3==0?v.x:i%3==1?v.y:v.z;},flash.vertices.size()*3U,2e-6) ||
                       !compare_float_stream("weapon.muzzleFlash.normals",[&](std::size_t i){const auto& v=flash.vertices[i/3].normal;return i%3==0?v.x:i%3==1?v.y:v.z;},flash.vertices.size()*3U,2e-5) ||
                       !compare_float_stream("weapon.muzzleFlash.colors",[&](std::size_t i){const auto& v=flash.vertices[i/3].color;return i%3==0?v.r:i%3==1?v.g:v.b;},flash.vertices.size()*3U,2e-6) ||
                       !compare_float_stream("weapon.muzzleFlash.uvs",[&](std::size_t i){const auto& v=flash.vertices[i/2].uv;return i%2==0?v.x:v.y;},flash.vertices.size()*2U,2e-6))return 2;
                    const auto* flash_indices=gnif.find("weapon.muzzleFlash.indices");if(flash_indices==nullptr||flash_indices->element_count!=flash.indices.size()){std::cerr<<"weapon flash index shape mismatch id="<<id<<'\n';return 2;}for(std::size_t i=0;i<flash.indices.size();++i){std::uint32_t reference{};std::memcpy(&reference,flash_indices->bytes.data()+i*sizeof(reference),sizeof(reference));if(reference!=flash.indices[i]){std::cerr<<"weapon flash index mismatch id="<<id<<" element="<<i<<'\n';return 2;}}
                }
                const auto& grips=fixture.at("weapon").at("grips");const auto compare_grip=[&](const char* name,const genomes::weapons::WeaponAttachment& attachment){const auto& expected_grip=grips.at(name);if(expected_grip.is_null())return true;const std::array<double,7> values{attachment.local_position.x,attachment.local_position.y,attachment.local_position.z,attachment.local_rotation[0],attachment.local_rotation[1],attachment.local_rotation[2],attachment.local_rotation[3]};for(std::size_t i=0;i<values.size();++i)if(std::abs(values[i]-expected_grip.at(i).get<double>())>2e-6){std::cerr<<"weapon grip mismatch id="<<id<<" grip="<<name<<" component="<<i<<'\n';return false;}return true;};
                if(!compare_grip("primary",native.value().primary_grip)||!compare_grip("secondary",native.value().support_grip))return 2;
                if(!grips.at("muzzle").is_null()){const auto& muzzle=grips.at("muzzle");const auto& p=native.value().muzzle.local_position;if(std::abs(p.x-muzzle.at(0).get<double>())>2e-6||std::abs(p.y-muzzle.at(1).get<double>())>2e-6||std::abs(p.z-muzzle.at(2).get<double>())>2e-6){std::cerr<<"weapon muzzle mismatch id="<<id<<'\n';return 2;}}
            }
        }
    }
    {
        std::ifstream equipment_file(std::string(GENOMES_SOURCE_DIR) +
            "/reference/fixtures/infantry/equipment_catalog_v1.json");
        const auto equipment = nlohmann::json::parse(equipment_file);
        if (equipment.at("equipmentSchema") != "EQUIPMENT/v1" ||
            equipment.at("slots").size() != kEquipmentSlotCount ||
            equipment.at("items").size() != kEquipmentItemCount ||
            equipment.at("loadouts").size() != kInfantryLoadoutCount) {
            std::cerr << "equipment catalog shape mismatch\n";
            return 2;
        }
        const auto native_slots = EquipmentCatalog::slots();
        for (std::size_t index=0; index<native_slots.size(); ++index) {
            const auto& expected=equipment.at("slots").at(index);
            const std::string required=expected.at("required").is_null()
                ? std::string{} : expected.at("required").get<std::string>();
            const std::string socket=expected.at("socket").is_null()
                ? std::string{} : expected.at("socket").get<std::string>();
            if(expected.at("id").get<std::string>()!=native_slots[index].identifier ||
               required!=native_slots[index].required_item||socket!=native_slots[index].socket){
                std::cerr << "equipment slot mismatch index=" << index << '\n'; return 2;
            }
        }
        constexpr std::array<std::string_view,12> kind_names{"cap","helmet","eyewear","mask",
            "neckwear","clothing","armor","rig","pack","belt","pouch","weapon"};
        const auto native_items=EquipmentCatalog::items();
        for(std::size_t index=0;index<native_items.size();++index){
            const auto& actual=native_items[index]; const auto& expected=equipment.at("items").at(index);
            if(expected.at("id").get<std::string>()!=actual.identifier ||
               expected.at("kind").get<std::string>()!=kind_names[static_cast<std::size_t>(actual.kind)] ||
               std::abs(expected.at("weightKg").get<double>()-actual.weight_kg)>1e-6 ||
               expected.at("slots").size()!=actual.allowed_slot_count){
                std::cerr << "equipment item mismatch item=" << actual.identifier << '\n'; return 2;
            }
            for(std::size_t slot=0;slot<actual.allowed_slot_count;++slot)
                if(expected.at("slots").at(slot).get<std::string>()!=
                   native_slots[equipmentSlotIndex(actual.allowed_slots[slot])].identifier){
                    std::cerr << "equipment allowed slot mismatch item=" << actual.identifier << '\n'; return 2;
                }
            const auto& visual=expected.at("visual");
            const auto string_field=[&](std::string_view name,std::string_view value){
                return !visual.contains(name)||visual.at(name).get<std::string>()==value;
            };
            const auto number_field=[&](std::string_view name,float value){
                return !visual.contains(name)||std::abs(visual.at(name).get<double>()-value)<=1e-6;
            };
            if(!string_field("style",actual.visual.style)||
               !string_field("coverage",actual.visual.coverage)||
               !number_field("ease",actual.visual.ease)||
               !number_field("width",actual.visual.width)||
               !number_field("shaft",actual.visual.shaft)||
               !number_field("thickness",actual.visual.thickness)||
               !number_field("count",static_cast<float>(actual.visual.count))||
               (visual.contains("pads")&&visual.at("pads").get<bool>()!=actual.visual.pads)||
               (visual.contains("roll")&&visual.at("roll").get<bool>()!=actual.visual.roll)){
                std::cerr<<"equipment visual mismatch item="<<actual.identifier<<'\n';return 2;
            }
            if(visual.contains("size"))for(std::size_t component=0;component<3U;++component){
                const std::array<float,3U> size{actual.visual.size.x,actual.visual.size.y,
                                                actual.visual.size.z};
                if(std::abs(visual.at("size").at(component).get<double>()-size[component])>1e-6){
                    std::cerr<<"equipment visual size mismatch item="<<actual.identifier<<'\n';return 2;
                }
            }
        }
        const auto native_loadouts=infantryLoadouts();
        for(std::size_t index=0;index<native_loadouts.size();++index){
            const auto& expected=equipment.at("loadouts").at(index);
            if(expected.at("id").get<std::string>()!=native_loadouts[index].identifier){
                std::cerr << "loadout id mismatch index=" << index << '\n'; return 2;
            }
            for(std::size_t slot=0;slot<native_slots.size();++slot){
                const auto& choice=native_loadouts[index].choices[slot];
                const auto& expected_slots=expected.at("slots");
                const std::string slot_name(native_slots[slot].identifier);
                if(!expected_slots.contains(slot_name)){
                    if(choice.count!=0){std::cerr<<"unexpected loadout choice loadout="
                        <<native_loadouts[index].identifier<<" slot="<<slot_name<<'\n';return 2;}
                    continue;
                }
                const auto& value=expected_slots.at(slot_name);
                const std::size_t expected_count=value.is_array()?value.size():1U;
                if(choice.count!=expected_count){std::cerr<<"loadout choice count mismatch loadout="
                    <<native_loadouts[index].identifier<<" slot="<<native_slots[slot].identifier<<'\n';return 2;}
                for(std::size_t option=0;option<expected_count;++option){
                    const auto& selected=value.is_array()?value.at(option):value;
                    const auto expected_id=selected.is_null()?0U:genomes::foundation::stable_id(selected.get<std::string>());
                    if(choice.definitions[option]!=expected_id){std::cerr<<"loadout choice mismatch loadout="
                        <<native_loadouts[index].identifier<<" slot="<<native_slots[slot].identifier
                        <<" option="<<option<<'\n';return 2;}
                }
            }
        }
        const auto fit_genome=InfantryGenome::generate(
            equipment.at("request").at("seed").get<genomes::proc::Seed>(),
            equipment.at("request").at("variation").get<double>());
        const auto fit_phenotype=fit_genome?PhenotypeResolver::resolve(fit_genome.value()):
            genomes::foundation::Result<PhenotypeArtifact,genomes::foundation::Error>::failure(
                fit_genome.error());
        const auto fit_rig=fit_phenotype?RigBuilder::build(fit_phenotype.value().body,
            fit_phenotype.value().face):
            genomes::foundation::Result<SkeletonData,genomes::foundation::Error>::failure(
                fit_phenotype.error());
        const auto fit_state=EquipmentResolver::resolve(
            equipment.at("request").at("seed").get<genomes::proc::Seed>(),
            EquipmentCatalog::loadoutId(equipment.at("request").at("loadout").get<std::string>()));
        if(!fit_phenotype||!fit_rig||!fit_state){std::cerr<<"equipment fit setup failed\n";return 2;}
        const auto& expected_state=equipment.at("equipmentState").at("slots");
        for(std::size_t index=0;index<native_slots.size();++index){
            const auto& expected=expected_state.at(std::string(native_slots[index].identifier));
            const auto* actual=fit_state.value().item(static_cast<EquipmentSlot>(index));
            if(expected.is_null()!= (actual==nullptr)){
                std::cerr<<"equipment resolved presence mismatch slot="<<native_slots[index].identifier<<'\n';return 2;
            }
            if(actual==nullptr)continue;
            const auto* definition=EquipmentCatalog::findItem(actual->definition_id);
            const auto& variant=expected.at("variant");
            if(definition==nullptr||definition->identifier!=expected.at("definitionId").get<std::string>()||
               actual->seed!=expected.at("seed").get<genomes::proc::Seed>()||
               std::abs(actual->variant.size-variant.at("size").get<double>())>2e-6||
               std::abs(actual->variant.shade-variant.at("shade").get<double>())>2e-6||
               std::abs(actual->variant.detail-variant.at("detail").get<double>())>2e-6){
                std::cerr<<"equipment resolved item mismatch slot="<<native_slots[index].identifier<<'\n';return 2;
            }
        }
        const auto native_fit=EquipmentFitter::build(fit_state.value(),fit_phenotype.value(),fit_rig.value());
        if(!native_fit){std::cerr<<"equipment fit build failed\n";return 2;}
        const auto& expected_fit=equipment.at("equipmentFit");
        const auto near=[](double left,double right){return std::abs(left-right)<=2e-6;};
        if(!near(native_fit.value().height,expected_fit.at("height").get<double>())||
           !near(native_fit.value().hip_y,expected_fit.at("hipY").get<double>())||
           !near(native_fit.value().armor_thickness,expected_fit.at("armorGap").get<double>())){
            std::cerr<<"equipment fit scalar mismatch\n";return 2;
        }
        for(std::size_t index=0;index<native_fit.value().jacket.size();++index){
            const auto& actual=native_fit.value().jacket[index];const auto& expected=expected_fit.at("jacket").at(index);
            if(!near(actual.y,expected.at(0).get<double>())||
               !near(actual.half_width,expected.at(1).get<double>())||
               !near(actual.half_depth,expected.at(2).get<double>())){
                std::cerr<<"equipment jacket mismatch index="<<index<<'\n';return 2;
            }
        }
        const std::array<float,3U> pack_dimensions{native_fit.value().pack_dimensions.x,
                                                   native_fit.value().pack_dimensions.y,
                                                   native_fit.value().pack_dimensions.z};
        for(std::size_t component=0;component<pack_dimensions.size();++component)
            if(!near(pack_dimensions[component],expected_fit.at("packDimensions").at(component).get<double>())){
                std::cerr<<"equipment pack dimensions mismatch component="<<component
                         <<" expected="<<expected_fit.at("packDimensions").at(component)
                         <<" actual="<<pack_dimensions[component]<<'\n';return 2;
            }
        constexpr std::array<std::pair<std::string_view,EquipmentSocketId>,19U> socket_ids{{
            {"HEAD",EquipmentSocketId::Head},{"HEAD_FRONT",EquipmentSocketId::HeadFront},
            {"NECK",EquipmentSocketId::Neck},{"CHEST_CENTER",EquipmentSocketId::ChestCenter},
            {"CHEST_LEFT",EquipmentSocketId::ChestLeft},{"BACK_CENTER",EquipmentSocketId::BackCenter},
            {"WAIST",EquipmentSocketId::Waist},{"WAIST_FRONT",EquipmentSocketId::WaistFront},
            {"WAIST_BACK",EquipmentSocketId::WaistBack},{"HIP_L",EquipmentSocketId::HipL},
            {"HIP_R",EquipmentSocketId::HipR},{"THIGH_L",EquipmentSocketId::ThighL},
            {"THIGH_R",EquipmentSocketId::ThighR},{"HAND_L",EquipmentSocketId::HandL},
            {"HAND_R",EquipmentSocketId::HandR},{"FOOT_L",EquipmentSocketId::FootL},
            {"FOOT_R",EquipmentSocketId::FootR},{"WEAPON_BACK",EquipmentSocketId::WeaponBack},
            {"WEAPON_HIP",EquipmentSocketId::WeaponHip}}};
        for(const auto& [name,id]:socket_ids){
            const auto& actual=native_fit.value().socket(id);
            const auto& expected=expected_fit.at("sockets").at(name);
            const auto* bone=fit_rig.value().find(actual.bone);
            if(bone==nullptr||bone->name!=expected.at("bone").get<std::string>()){
                std::cerr<<"equipment socket bone mismatch name="<<name<<'\n';return 2;
            }
            const std::array<float,3U> position{actual.position.x,actual.position.y,actual.position.z};
            const std::array<float,3U> normal{actual.normal.x,actual.normal.y,actual.normal.z};
            for(std::size_t component=0;component<3U;++component)
                if(!near(position[component],expected.at("position").at(component).get<double>())||
                   !near(normal[component],expected.at("normal").at(component).get<double>())){
                    std::cerr<<"equipment socket mismatch name="<<name<<" component="<<component
                             <<" expected="<<expected.at("position").at(component)
                             <<" actual="<<position[component]<<'\n';return 2;
                }
        }
        const std::array<float,4U> head_angles{-3.14159265358979323846F,
            -1.57079632679489661923F,0.0F,1.57079632679489661923F};
        for(std::size_t index=0;index<head_angles.size();++index)
            if(!near(native_fit.value().headBottom(head_angles[index]),
                     expected_fit.at("headBottom").at(index).get<double>())){
                std::cerr<<"equipment head bottom mismatch index="<<index
                         <<" expected="<<expected_fit.at("headBottom").at(index)
                         <<" actual="<<native_fit.value().headBottom(head_angles[index])<<'\n';return 2;
            }
        {
            ReferenceSurfaceBuilder jacket_builder(fit_phenotype.value().body.height,kRigBoneCount);
            ReferenceJacketTopology jacket_topology{};
            const auto jacket_build=ReferenceBodySurfaceGenerator::appendJacket(
                jacket_builder,native_fit.value(),fit_rig.value(),kDefaultUniformColor,3U,
                &jacket_topology);
            const auto sleeve_build=ReferenceBodySurfaceGenerator::appendSleeve(
                jacket_builder,jacket_topology,native_fit.value(),kDefaultUniformColor,true);
            const auto hand_build=ReferenceBodySurfaceGenerator::appendHand(
                jacket_builder,native_fit.value(),true,3U,referenceSrgb(0x3d4136U),
                referenceSrgb(fit_phenotype.value().body.skin_color_hex));
            const auto right_sleeve_build=ReferenceBodySurfaceGenerator::appendSleeve(
                jacket_builder,jacket_topology,native_fit.value(),kDefaultUniformColor,false);
            const auto right_hand_build=ReferenceBodySurfaceGenerator::appendHand(
                jacket_builder,native_fit.value(),false,3U,referenceSrgb(0x3d4136U),
                referenceSrgb(fit_phenotype.value().body.skin_color_hex));
            const auto pants_build=ReferenceBodySurfaceGenerator::appendPants(
                jacket_builder,native_fit.value(),kDefaultUniformColor,3U);
            const auto left_boot_build=ReferenceBodySurfaceGenerator::appendBoot(
                jacket_builder,native_fit.value(),true,3U,referenceSrgb(0x302d29U));
            const auto right_boot_build=ReferenceBodySurfaceGenerator::appendBoot(
                jacket_builder,native_fit.value(),false,3U,referenceSrgb(0x302d29U));
            const auto cloth_build=ReferenceBodySurfaceGenerator::appendClothDetails(
                jacket_builder,native_fit.value(),kDefaultUniformColor,3U);
            const auto face_shell_build=ReferenceFaceSurfaceGenerator::appendShell(
                jacket_builder,native_fit.value(),
                referenceSrgb(fit_phenotype.value().body.skin_color_hex),3U);
            const std::array<std::string_view,4U> morph_names{"eyelidsClose","eyelidsArc","neckFlex","handsRelax"};
            std::array<std::vector<genomes::foundation::Vec3>,4U> morph_positions;
            for(std::size_t index=0;index<morph_names.size();++index)morph_positions[index]=
                jacket_builder.morphPositions(morph_names[index]);
            const auto jacket=jacket_builder.finalize();
            std::array<std::vector<genomes::foundation::Vec3>,4U> morph_normals;
            for(std::size_t index=0;index<morph_names.size();++index)morph_normals[index]=
                jacket_builder.morphNormals(morph_names[index],jacket);
            const auto avatar_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/full-avatar/avatar-0-high-default.gnif";
            const auto avatar_fixture=genomes::test::infantry_fixture::read(
                avatar_binary,avatar_binary+".json");
            if(jacket_build.vertices!=449U||jacket_build.triangles!=768U||
               sleeve_build.vertices!=375U||sleeve_build.triangles!=726U||
               hand_build.vertices!=320U||hand_build.triangles!=562U||
               right_sleeve_build.vertices!=375U||right_sleeve_build.triangles!=726U||
               right_hand_build.vertices!=320U||right_hand_build.triangles!=562U||
               pants_build.vertices!=793U||pants_build.triangles!=1552U||
               left_boot_build.vertices!=342U||left_boot_build.triangles!=620U||
               right_boot_build.vertices!=342U||right_boot_build.triangles!=620U||
               cloth_build.vertices!=599U||cloth_build.triangles!=776U||
               face_shell_build.vertices!=10228U||face_shell_build.triangles!=19268U||
               jacket.vertices.size()!=jacket_build.vertices+sleeve_build.vertices+
                   hand_build.vertices+right_sleeve_build.vertices+right_hand_build.vertices+
                   pants_build.vertices+left_boot_build.vertices+right_boot_build.vertices+
                   cloth_build.vertices+face_shell_build.vertices||
               jacket.indices.size()!=(jacket_build.triangles+sleeve_build.triangles+
                   hand_build.triangles+right_sleeve_build.triangles+
                   right_hand_build.triangles+pants_build.triangles+
                   left_boot_build.triangles+right_boot_build.triangles+
                   cloth_build.triangles+face_shell_build.triangles)*3U){
                std::cerr<<"reference jacket topology shape mismatch vertices="<<jacket.vertices.size()
                    <<" triangles="<<jacket.indices.size()/3U<<'\n';return 2;}
            const auto jacket_stream=[&](std::string_view name,auto value_at,std::size_t count,double tolerance){
                const auto* expected=avatar_fixture.find(name);if(expected==nullptr||expected->element_count<count){
                    std::cerr<<"reference jacket stream shape mismatch stream="<<name<<'\n';return false;}
                for(std::size_t index=0;index<count;++index){float reference{};
                    std::memcpy(&reference,expected->bytes.data()+index*sizeof(float),sizeof(float));
                    const double actual=value_at(index);if(std::abs(actual-reference)>tolerance){
                        std::cerr<<"reference jacket stream mismatch stream="<<name<<" element="<<index
                            <<" expected="<<reference<<" actual="<<actual;
                        if(name=="body.positions"||name=="body.normals"){const auto vertex=static_cast<std::uint32_t>(index/3U);for(const auto& tag:jacket.tags)
                            if(std::find(tag.vertices.begin(),tag.vertices.end(),vertex)!=tag.vertices.end())std::cerr<<" tag="<<tag.name;
                            const auto& p=jacket.vertices[vertex].position;std::cerr<<std::setprecision(17)<<" point="<<p.x<<','<<p.y<<','<<p.z;
                            std::cerr<<" H="<<fit_phenotype.value().body.height<<" refH="<<fit_phenotype.value().body.reference_height
                                <<" hip="<<fit_phenotype.value().body.hip_y<<" refHip="<<fit_phenotype.value().body.reference_hip_y;
                            if(const auto* expected_positions=avatar_fixture.find("body.positions")){
                                std::array<float,3U> expected_point{};
                                std::memcpy(expected_point.data(),expected_positions->bytes.data()+vertex*3U*sizeof(float),3U*sizeof(float));
                                std::cerr<<" expectedPoint="<<expected_point[0]<<','<<expected_point[1]<<','<<expected_point[2]
                                    <<" pointError="<<std::abs(static_cast<double>(p.x)-expected_point[0])<<','
                                    <<std::abs(static_cast<double>(p.y)-expected_point[1])<<','
                                    <<std::abs(static_cast<double>(p.z)-expected_point[2]);}}
                        std::cerr<<'\n';return false;}}
                return true;};
            if(const auto* positions=avatar_fixture.find("body.positions");positions==nullptr||
                positions->element_count!=jacket.vertices.size()*3U){std::cerr<<"reference body vertex count mismatch\n";return 2;}
            if(!jacket_stream("body.positions",[&](std::size_t i){const auto& v=jacket.vertices[i/3U].position;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},jacket.vertices.size()*3U,2e-6)||
               !jacket_stream("body.normals",[&](std::size_t i){const auto& v=jacket.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},jacket.vertices.size()*3U,2e-5)||
               !jacket_stream("body.colors",[&](std::size_t i){const auto& v=jacket.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},jacket.vertices.size()*3U,2e-6)||
               !jacket_stream("body.uvs",[&](std::size_t i){const auto& v=jacket.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},jacket.vertices.size()*2U,2e-6)||
               !jacket_stream("body.skinIndices",[&](std::size_t i){const auto& v=jacket.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},jacket.vertices.size()*4U,0.0)||
               !jacket_stream("body.skinWeights",[&](std::size_t i){return jacket.vertices[i/4U].influences[i%4U].weight;},jacket.vertices.size()*4U,1e-5))return 2;
            for(std::size_t morph=0;morph<morph_names.size();++morph){const std::string stream_name="body.morph."+
                std::string(morph_names[morph])+".positions";if(!jacket_stream(stream_name,[&](std::size_t i){const auto& value=morph_positions[morph][i/3U];
                    return i%3U==0U?value.x:i%3U==1U?value.y:value.z;},jacket.vertices.size()*3U,2e-6))return 2;
                const std::string normal_name="body.morph."+std::string(morph_names[morph])+".normals";
                if(!jacket_stream(normal_name,[&](std::size_t i){const auto& value=morph_normals[morph][i/3U];
                    return i%3U==0U?value.x:i%3U==1U?value.y:value.z;},jacket.vertices.size()*3U,2e-5))return 2;}
            const auto* expected_indices=avatar_fixture.find("body.indices");
            const std::size_t verified_index_count=jacket.indices.size();
            if(expected_indices==nullptr||expected_indices->element_count!=verified_index_count){
                std::cerr<<"reference body index count mismatch\n";return 2;}
            for(std::size_t index=0;index<verified_index_count;++index){std::uint32_t reference{};
                std::memcpy(&reference,expected_indices->bytes.data()+index*sizeof(reference),sizeof(reference));
                if(reference!=jacket.indices[index]){std::cerr<<"reference jacket index mismatch element="
                    <<index<<" expected="<<reference<<" actual="<<jacket.indices[index]<<'\n';return 2;}}
            const auto neutral_state=EquipmentResolver::resolve(0U,genomes::foundation::StableId{0});
            const auto neutral_fit=neutral_state?EquipmentFitter::build(neutral_state.value(),
                fit_phenotype.value(),fit_rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(neutral_state.error());
            if(!neutral_fit){std::cerr<<"neutral hair fit setup failed\n";return 2;}
            ReferenceSurfaceBuilder neutral_builder(fit_phenotype.value().body.height,kRigBoneCount);
            const auto neutral_face_build=ReferenceFaceSurfaceGenerator::appendShell(neutral_builder,
                neutral_fit.value(),referenceSrgb(fit_phenotype.value().body.skin_color_hex),3U);
            const auto neutral_face=std::move(neutral_builder).finalize();
            if(neutral_face_build.vertices!=11237U||neutral_face.vertices.size()!=11237U){
                std::cerr<<"neutral hair topology shape mismatch vertices="<<neutral_face.vertices.size()<<'\n';return 2;}
            const auto neutral_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/full-avatar/avatar-0-high-neutral.gnif";
            const auto neutral_fixture=genomes::test::infantry_fixture::read(neutral_binary,neutral_binary+".json");
            constexpr std::size_t face_offset=3915U;
            const auto neutral_stream=[&](std::string_view name,auto value_at,std::size_t width,double tolerance){const auto* expected=neutral_fixture.find(name);
                if(expected==nullptr||expected->element_count<(face_offset+neutral_face.vertices.size())*width)return false;
                for(std::size_t index=0;index<neutral_face.vertices.size()*width;++index){float reference{};std::memcpy(&reference,
                    expected->bytes.data()+(face_offset*width+index)*sizeof(float),sizeof(float));if(std::abs(value_at(index)-reference)>tolerance){
                    std::cerr<<"neutral hair stream mismatch stream="<<name<<" element="<<index<<" expected="<<reference<<" actual="<<value_at(index)<<'\n';return false;}}return true;};
            if(!neutral_stream("body.positions",[&](std::size_t i){const auto& v=neutral_face.vertices[i/3U].position;return i%3U==0?v.x:i%3U==1?v.y:v.z;},3U,2e-6)||
               !neutral_stream("body.colors",[&](std::size_t i){const auto& v=neutral_face.vertices[i/3U].color;return i%3U==0?v.r:i%3U==1?v.g:v.b;},3U,2e-6)||
               !neutral_stream("body.uvs",[&](std::size_t i){const auto& v=neutral_face.vertices[i/2U].uv;return i%2U==0?v.x:v.y;},2U,2e-6)||
               !neutral_stream("body.skinIndices",[&](std::size_t i){const auto& v=neutral_face.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},4U,0.0)||
               !neutral_stream("body.skinWeights",[&](std::size_t i){return neutral_face.vertices[i/4U].influences[i%4U].weight;},4U,1e-5))return 2;
        }
        const auto native_gear=GearGenerator::build(fit_state.value(),native_fit.value(),
            fit_rig.value(),kDefaultUniformColor,3U,.25F);
        const auto native_surface=native_gear?GearSurfaceGenerator::build(native_gear.value()):
            genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(
                native_gear.error());
        const auto binary=std::string(GENOMES_SOURCE_DIR)+
            "/reference/fixtures/infantry/equipment-default-0-high-v1.gnif";
        const auto gnif=genomes::test::infantry_fixture::read(binary,binary+".json");
        if(!native_surface){std::cerr<<"equipment surface build failed\n";return 2;}
        const auto compare_stream=[&](std::string_view name,auto value_at,
                                      std::size_t count,double tolerance){
            const auto* stream=gnif.find(name);
            if(stream==nullptr||stream->element_count!=count){
                std::cerr<<"equipment stream shape mismatch stream="<<name<<'\n';return false;}
            double maximum=0.0;std::size_t first=count;
            for(std::size_t index=0;index<count;++index){float expected{};
                std::memcpy(&expected,stream->bytes.data()+index*sizeof(float),sizeof(float));
                const double actual=value_at(index),error=std::abs(actual-expected);
                maximum=std::max(maximum,error);if(error>tolerance&&first==count)first=index;}
            if(first!=count){float expected{};std::memcpy(&expected,stream->bytes.data()+first*sizeof(float),sizeof(float));
                std::cerr<<"equipment stream mismatch stream="<<name<<" element="<<first
                         <<" expected="<<expected<<" actual="<<value_at(first)
                         <<" maximum="<<maximum<<'\n';return false;}return true;};
        const auto& surface=native_surface.value();
        if(!compare_stream("gear.positions",[&](std::size_t i){const auto& v=surface.vertices[i/3U].position;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},surface.vertices.size()*3U,2e-6)||
           !compare_stream("gear.normals",[&](std::size_t i){const auto& v=surface.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},surface.vertices.size()*3U,2e-5)||
           !compare_stream("gear.colors",[&](std::size_t i){const auto& v=surface.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},surface.vertices.size()*3U,2e-6)||
           !compare_stream("gear.uvs",[&](std::size_t i){const auto& v=surface.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},surface.vertices.size()*2U,2e-6)||
           !compare_stream("gear.skinIndices",[&](std::size_t i){const auto& v=surface.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},surface.vertices.size()*4U,0.0)||
           !compare_stream("gear.skinWeights",[&](std::size_t i){const auto& v=surface.vertices[i/4U];return v.influences[i%4U].weight;},surface.vertices.size()*4U,2e-6))return 2;
        const auto* indices=gnif.find("gear.indices");
        if(indices==nullptr||indices->element_count!=surface.indices.size()){
            std::cerr<<"equipment index shape mismatch\n";return 2;}
        for(std::size_t index=0;index<surface.indices.size();++index){std::uint32_t expected{};
            std::memcpy(&expected,indices->bytes.data()+index*sizeof(expected),sizeof(expected));
            if(expected!=surface.indices[index]){std::cerr<<"equipment index mismatch element="<<index
                <<" expected="<<expected<<" actual="<<surface.indices[index]<<'\n';return 2;}}

        constexpr std::array matrix_seeds{0U,8841U,1003U,1592598566U};
        constexpr std::array matrix_details{std::pair{"far",1U},std::pair{"world",2U},
                                             std::pair{"high",3U}};
        constexpr std::array matrix_equipment{std::pair{"neutral",""},
                                               std::pair{"default","RIFLEMAN"},
                                               std::pair{"full","HEAVY_SUPPORT"}};
        for(const auto seed:matrix_seeds)for(const auto& [detail,level]:matrix_details)
        for(const auto& [equipment_variant,loadout]:matrix_equipment){
            const std::string stem="avatar-"+std::to_string(seed)+"-"+detail+"-"+
                equipment_variant+".gnif";
            const auto matrix_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/full-avatar/"+stem;
            const auto matrix_fixture=genomes::test::infantry_fixture::read(
                matrix_binary,matrix_binary+".json");
            const auto genome=InfantryGenome::generate(seed,0.0);
            const auto phenotype=genome?PhenotypeResolver::resolve(genome.value()):
                genomes::foundation::Result<PhenotypeArtifact,genomes::foundation::Error>::failure(genome.error());
            const auto rig=phenotype?RigBuilder::build(phenotype.value().body,phenotype.value().face):
                genomes::foundation::Result<SkeletonData,genomes::foundation::Error>::failure(phenotype.error());
            const auto state=EquipmentResolver::resolve(seed,*loadout?EquipmentCatalog::loadoutId(loadout):0U);
            const auto fit=state&&phenotype&&rig?EquipmentFitter::build(state.value(),phenotype.value(),rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure({genomes::foundation::ErrorCode::InvalidState,"matrix setup"});
            const auto avatar=fit?ReferenceBodySurfaceGenerator::build(fit.value(),rig.value(),
                kDefaultUniformColor,level):genomes::foundation::Result<ReferenceAvatarSurface,genomes::foundation::Error>::failure(fit.error());
            const auto gear=fit?GearGenerator::build(state.value(),fit.value(),rig.value(),
                kDefaultUniformColor,level,.25F):
                genomes::foundation::Result<GearArtifact,genomes::foundation::Error>::failure(fit.error());
            const auto mesh=gear?GearSurfaceGenerator::build(gear.value()):
                genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(gear.error());
            const auto* expected_positions=matrix_fixture.find("gear.positions");
            if(!mesh||expected_positions==nullptr||expected_positions->element_count!=mesh.value().vertices.size()*3U){
                std::cerr<<"gear matrix shape mismatch fixture="<<stem<<" expected="
                    <<(expected_positions?expected_positions->element_count/3U:0U)<<" actual="
                    <<(mesh?mesh.value().vertices.size():0U)<<'\n';return 2;}
            for(std::size_t index=0;index<expected_positions->element_count;++index){float expected{};
                std::memcpy(&expected,expected_positions->bytes.data()+index*sizeof(float),sizeof(float));
                const auto& vertex=mesh.value().vertices[index/3U];
                const float actual=index%3U==0U?vertex.position.x:index%3U==1U?vertex.position.y:vertex.position.z;
                if(std::abs(actual-expected)>2e-6){std::cerr<<"gear matrix position mismatch fixture="
                    <<stem<<" element="<<index<<" expected="<<expected<<" actual="<<actual<<'\n';return 2;}}
            const auto matrix_stream=[&](std::string_view name,auto value_at,std::size_t count,double tolerance){
                const auto* stream=matrix_fixture.find(name);if(stream==nullptr||stream->element_count!=count){
                    std::cerr<<"gear matrix stream shape mismatch fixture="<<stem<<" stream="<<name<<'\n';return false;}
                for(std::size_t index=0;index<count;++index){float expected{};
                    std::memcpy(&expected,stream->bytes.data()+index*sizeof(float),sizeof(float));
                    const double actual=value_at(index);if(std::abs(actual-expected)>tolerance){
                        std::cerr<<"gear matrix stream mismatch fixture="<<stem<<" stream="<<name
                            <<" element="<<index<<" expected="<<expected<<" actual="<<actual;
                        if(name.starts_with("body.")&&avatar){const auto components=(name=="body.uvs"?2U:name=="body.positions"||name=="body.normals"||name=="body.colors"?3U:4U);const auto vertex=static_cast<std::uint32_t>(index/components);for(const auto& tag:avatar.value().mesh.tags)
                            if(std::find(tag.vertices.begin(),tag.vertices.end(),vertex)!=tag.vertices.end())std::cerr<<" tag="<<tag.name;}
                        std::cerr<<'\n';return false;}}
                return true;};
            const auto& matrix_mesh=mesh.value();
            if(!matrix_stream("gear.normals",[&](std::size_t i){const auto& v=matrix_mesh.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},matrix_mesh.vertices.size()*3U,2e-5)||
               !matrix_stream("gear.colors",[&](std::size_t i){const auto& v=matrix_mesh.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},matrix_mesh.vertices.size()*3U,2e-6)||
               !matrix_stream("gear.uvs",[&](std::size_t i){const auto& v=matrix_mesh.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},matrix_mesh.vertices.size()*2U,2e-6)||
               !matrix_stream("gear.skinIndices",[&](std::size_t i){const auto& v=matrix_mesh.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},matrix_mesh.vertices.size()*4U,0.0)||
               !matrix_stream("gear.skinWeights",[&](std::size_t i){return mesh.value().vertices[i/4U].influences[i%4U].weight;},matrix_mesh.vertices.size()*4U,2e-6))return 2;
            if(!avatar){std::cerr<<"body matrix build failed fixture="<<stem<<'\n';return 2;}const auto& body=avatar.value().mesh;
            if(!matrix_stream("body.positions",[&](std::size_t i){const auto& v=body.vertices[i/3U].position;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},body.vertices.size()*3U,2e-6)||
               !matrix_stream("body.normals",[&](std::size_t i){const auto& v=body.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},body.vertices.size()*3U,2e-5)||
               !matrix_stream("body.colors",[&](std::size_t i){const auto& v=body.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},body.vertices.size()*3U,2e-6)||
               !matrix_stream("body.uvs",[&](std::size_t i){const auto& v=body.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},body.vertices.size()*2U,2e-6)||
               !matrix_stream("body.skinIndices",[&](std::size_t i){const auto& v=body.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},body.vertices.size()*4U,0.0)||
               !matrix_stream("body.skinWeights",[&](std::size_t i){return body.vertices[i/4U].influences[i%4U].weight;},body.vertices.size()*4U,1e-5))return 2;
            for(std::size_t morph=0;morph<avatar.value().morphs.size();++morph){const auto& target=avatar.value().morphs[morph];
                const auto position_name="body.morph."+std::string(target.name)+".positions";
                if(matrix_fixture.find(position_name)==nullptr){if(std::any_of(target.position_deltas.begin(),target.position_deltas.end(),[](const auto& v){return v.x!=0.0F||v.y!=0.0F||v.z!=0.0F;})){
                        std::cerr<<"unexpected body morph fixture="<<stem<<" morph="<<target.name<<'\n';return 2;}continue;}
                if(!matrix_stream(position_name,[&](std::size_t i){const auto& v=target.position_deltas[i/3U];return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},body.vertices.size()*3U,2e-6)||
                   !matrix_stream("body.morph."+std::string(target.name)+".normals",[&](std::size_t i){const auto& v=target.normal_deltas[i/3U];return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},body.vertices.size()*3U,2e-5))return 2;}
            const auto* body_indices=matrix_fixture.find("body.indices");if(body_indices==nullptr||body_indices->element_count!=body.indices.size()){
                std::cerr<<"body matrix index shape mismatch fixture="<<stem<<'\n';return 2;}for(std::size_t index=0;index<body.indices.size();++index){std::uint32_t expected{};
                std::memcpy(&expected,body_indices->bytes.data()+index*sizeof(expected),sizeof(expected));if(expected!=body.indices[index]){
                    std::cerr<<"body matrix index mismatch fixture="<<stem<<" element="<<index<<" expected="<<expected<<" actual="<<body.indices[index]<<'\n';return 2;}}
            const auto* matrix_indices=matrix_fixture.find("gear.indices");
            if(matrix_indices==nullptr||matrix_indices->element_count!=matrix_mesh.indices.size()){
                std::cerr<<"gear matrix index shape mismatch fixture="<<stem<<'\n';return 2;}
            for(std::size_t index=0;index<matrix_mesh.indices.size();++index){std::uint32_t expected{};
                std::memcpy(&expected,matrix_indices->bytes.data()+index*sizeof(expected),sizeof(expected));
                if(expected!=matrix_mesh.indices[index]){std::cerr<<"gear matrix index mismatch fixture="
                    <<stem<<" element="<<index<<" expected="<<expected<<" actual="
                    <<matrix_mesh.indices[index]<<'\n';return 2;}}
            const auto& expected_gear=matrix_fixture.manifest.at("gear");
            if(expected_gear.at("groups").size()!=matrix_mesh.groups.size()){
                std::cerr<<"gear matrix group count mismatch fixture="<<stem<<'\n';return 2;}
            for(std::size_t index=0;index<matrix_mesh.groups.size();++index){
                const auto& expected=expected_gear.at("groups").at(index);
                const auto& actual=matrix_mesh.groups[index];
                if(expected.at("start")!=actual.start||expected.at("count")!=actual.count||
                   expected.at("material")!=actual.material){
                    std::cerr<<"gear matrix group mismatch fixture="<<stem<<" group="<<index<<'\n';return 2;}}
            const auto& expected_tags=expected_gear.at("tags");
            if(expected_tags.size()!=matrix_mesh.tags.size()){
                std::cerr<<"gear matrix tag count mismatch fixture="<<stem<<'\n';return 2;}
            for(const auto& actual:matrix_mesh.tags){
                if(!expected_tags.contains(actual.name)||expected_tags.at(actual.name).size()!=actual.vertices.size()){
                    std::cerr<<"gear matrix tag shape mismatch fixture="<<stem<<" tag="<<actual.name<<'\n';return 2;}
                for(std::size_t index=0;index<actual.vertices.size();++index)
                    if(expected_tags.at(actual.name).at(index)!=actual.vertices[index]){
                        std::cerr<<"gear matrix tag mismatch fixture="<<stem<<" tag="<<actual.name
                            <<" element="<<index<<'\n';return 2;}}
        }
        EquipmentOverrideSet cap_overrides{};
        cap_overrides.slots[equipmentSlotIndex(EquipmentSlot::Head)]=
            EquipmentOverride::item(genomes::foundation::stable_id("field_cap"));
        const auto cap_state=EquipmentResolver::resolve(0U,0U,cap_overrides);
        const auto cap_fit=cap_state?EquipmentFitter::build(cap_state.value(),fit_phenotype.value(),fit_rig.value()):
            genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(cap_state.error());
        const auto cap_gear=cap_fit?GearGenerator::build(cap_state.value(),cap_fit.value(),fit_rig.value(),
            kDefaultUniformColor,3U,.25F):genomes::foundation::Result<GearArtifact,genomes::foundation::Error>::failure(cap_fit.error());
        const auto cap_mesh=cap_gear?GearSurfaceGenerator::build(cap_gear.value()):
            genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(cap_gear.error());
        const auto cap_binary=std::string(GENOMES_SOURCE_DIR)+
            "/reference/fixtures/infantry/equipment-field_cap-0-high-v1.gnif";
        const auto cap_fixture=genomes::test::infantry_fixture::read(cap_binary,cap_binary+".json");
        const auto* cap_positions=cap_fixture.find("gear.positions");
        if(!cap_mesh||cap_positions==nullptr||cap_positions->element_count!=cap_mesh.value().vertices.size()*3U){
            std::cerr<<"field cap shape mismatch expected="<<(cap_positions?cap_positions->element_count/3U:0U)
                <<" actual="<<(cap_mesh?cap_mesh.value().vertices.size():0U)<<'\n';return 2;}
        for(std::size_t index=0;index<cap_positions->element_count;++index){float expected{};
            std::memcpy(&expected,cap_positions->bytes.data()+index*sizeof(float),sizeof(float));
            const auto& vertex=cap_mesh.value().vertices[index/3U];
            const float actual=index%3U==0U?vertex.position.x:index%3U==1U?vertex.position.y:vertex.position.z;
            if(std::abs(actual-expected)>2e-6){std::cerr<<"field cap position mismatch element="
                <<index<<" expected="<<expected<<" actual="<<actual<<'\n';return 2;}}
        const auto cap_stream=[&](std::string_view name,auto value_at,std::size_t count,double tolerance){
            const auto* stream=cap_fixture.find(name);if(stream==nullptr||stream->element_count!=count){
                std::cerr<<"field cap stream shape mismatch stream="<<name<<'\n';return false;}
            for(std::size_t index=0;index<count;++index){float expected{};
                std::memcpy(&expected,stream->bytes.data()+index*sizeof(float),sizeof(float));
                const double actual=value_at(index);if(std::abs(actual-expected)>tolerance){
                    std::cerr<<"field cap stream mismatch stream="<<name<<" element="<<index
                        <<" expected="<<expected<<" actual="<<actual<<'\n';return false;}}return true;};
        const auto& cap_surface=cap_mesh.value();
        if(!cap_stream("gear.normals",[&](std::size_t i){const auto& v=cap_surface.vertices[i/3U].normal;return i%3U==0U?v.x:i%3U==1U?v.y:v.z;},cap_surface.vertices.size()*3U,2e-5)||
           !cap_stream("gear.colors",[&](std::size_t i){const auto& v=cap_surface.vertices[i/3U].color;return i%3U==0U?v.r:i%3U==1U?v.g:v.b;},cap_surface.vertices.size()*3U,2e-6)||
           !cap_stream("gear.uvs",[&](std::size_t i){const auto& v=cap_surface.vertices[i/2U].uv;return i%2U==0U?v.x:v.y;},cap_surface.vertices.size()*2U,2e-6)||
           !cap_stream("gear.skinIndices",[&](std::size_t i){const auto& v=cap_surface.vertices[i/4U];return static_cast<float>(v.influences[i%4U].bone_index==kInvalidBoneIndex?0U:v.influences[i%4U].bone_index);},cap_surface.vertices.size()*4U,0.0)||
           !cap_stream("gear.skinWeights",[&](std::size_t i){return cap_surface.vertices[i/4U].influences[i%4U].weight;},cap_surface.vertices.size()*4U,2e-6))return 2;
        const auto* cap_indices=cap_fixture.find("gear.indices");
        if(cap_indices==nullptr||cap_indices->element_count!=cap_mesh.value().indices.size()){
            std::cerr<<"field cap index shape mismatch\n";return 2;}
        for(std::size_t index=0;index<cap_indices->element_count;++index){std::uint32_t expected{};
            std::memcpy(&expected,cap_indices->bytes.data()+index*sizeof(expected),sizeof(expected));
            if(expected!=cap_mesh.value().indices[index]){std::cerr<<"field cap index mismatch element="
                <<index<<" expected="<<expected<<" actual="<<cap_mesh.value().indices[index]<<'\n';return 2;}}
        const auto& expected_cap=cap_fixture.manifest.at("gear");
        if(expected_cap.at("groups").size()!=cap_surface.groups.size()||
           expected_cap.at("tags").at("gear.head").size()!=cap_surface.tags.front().vertices.size()){
            std::cerr<<"field cap groups or tags mismatch\n";return 2;}
        constexpr std::array<std::string_view,4U> additional_caps{
            "patrol_cap","beanie","beret","boonie_hat"};
        for(const auto item_id:additional_caps){EquipmentOverrideSet overrides{};
            overrides.slots[equipmentSlotIndex(EquipmentSlot::Head)]=
                EquipmentOverride::item(genomes::foundation::stable_id(item_id));
            const auto state=EquipmentResolver::resolve(0U,0U,overrides);
            const auto fit=state?EquipmentFitter::build(state.value(),fit_phenotype.value(),fit_rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(state.error());
            const auto gear=fit?GearGenerator::build(state.value(),fit.value(),fit_rig.value(),
                kDefaultUniformColor,3U,.25F):genomes::foundation::Result<GearArtifact,genomes::foundation::Error>::failure(fit.error());
            const auto mesh=gear?GearSurfaceGenerator::build(gear.value()):
                genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(gear.error());
            const auto item_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/equipment-"+std::string(item_id)+"-0-high-v1.gnif";
            const auto fixture=genomes::test::infantry_fixture::read(item_binary,item_binary+".json");
            const auto* positions=fixture.find("gear.positions");const auto* indices=fixture.find("gear.indices");
            if(!mesh||positions==nullptr||indices==nullptr||positions->element_count!=mesh.value().vertices.size()*3U||
               indices->element_count!=mesh.value().indices.size()){
                std::cerr<<"cap family shape mismatch item="<<item_id<<" expectedVertices="
                    <<(positions?positions->element_count/3U:0U)<<" actualVertices="
                    <<(mesh?mesh.value().vertices.size():0U)<<'\n';return 2;}
            if(!std::isfinite(mesh.value().minimum.x)||!std::isfinite(mesh.value().minimum.y)||
               !std::isfinite(mesh.value().minimum.z)||!std::isfinite(mesh.value().maximum.x)||
               !std::isfinite(mesh.value().maximum.y)||!std::isfinite(mesh.value().maximum.z)||
               !std::isfinite(mesh.value().sphere_radius)){
                std::cerr<<"face/neck non-finite bounds item="<<item_id<<'\n';return 2;}
            for(std::size_t index=0;index<positions->element_count;++index){float expected{};
                std::memcpy(&expected,positions->bytes.data()+index*sizeof(float),sizeof(float));
                const auto& vertex=mesh.value().vertices[index/3U];const float actual=index%3U==0U?
                    vertex.position.x:index%3U==1U?vertex.position.y:vertex.position.z;
                if(std::abs(actual-expected)>2e-6){std::cerr<<"cap family position mismatch item="
                    <<item_id<<" element="<<index<<'\n';return 2;}}
            for(std::size_t index=0;index<indices->element_count;++index){std::uint32_t expected{};
                std::memcpy(&expected,indices->bytes.data()+index*sizeof(expected),sizeof(expected));
                if(expected!=mesh.value().indices[index]){std::cerr<<"cap family index mismatch item="
                    <<item_id<<" element="<<index<<" expected="<<expected<<" actual="
                    <<mesh.value().indices[index]<<'\n';return 2;}}
            if(!compareGearFixture(mesh.value(),fixture,item_id))return 2;
        }
        constexpr std::array<std::pair<EquipmentSlot,std::string_view>,6U> face_neck_items{{
            {EquipmentSlot::Face,"glasses"},{EquipmentSlot::Face,"goggles"},
            {EquipmentSlot::Face,"balaclava"},{EquipmentSlot::Face,"respirator"},
            {EquipmentSlot::Neck,"scarf"},{EquipmentSlot::Neck,"neck_gaiter"}}};
        for(const auto& [slot,item_id]:face_neck_items){EquipmentOverrideSet overrides{};
            overrides.slots[equipmentSlotIndex(slot)]=EquipmentOverride::item(
                genomes::foundation::stable_id(item_id));
            const auto state=EquipmentResolver::resolve(0U,0U,overrides);
            const auto fit=state?EquipmentFitter::build(state.value(),fit_phenotype.value(),fit_rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(state.error());
            const auto gear=fit?GearGenerator::build(state.value(),fit.value(),fit_rig.value(),
                kDefaultUniformColor,3U,.25F):genomes::foundation::Result<GearArtifact,genomes::foundation::Error>::failure(fit.error());
            const auto mesh=gear?GearSurfaceGenerator::build(gear.value()):
                genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(gear.error());
            const auto item_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/equipment-"+std::string(item_id)+"-0-high-v1.gnif";
            const auto fixture=genomes::test::infantry_fixture::read(item_binary,item_binary+".json");
            const auto* positions=fixture.find("gear.positions");const auto* indices=fixture.find("gear.indices");
            if(!mesh||positions==nullptr||indices==nullptr||positions->element_count!=mesh.value().vertices.size()*3U||
               indices->element_count!=mesh.value().indices.size()){
                std::cerr<<"face/neck shape mismatch item="<<item_id<<" expectedVertices="
                    <<(positions?positions->element_count/3U:0U)<<" actualVertices="
                    <<(mesh?mesh.value().vertices.size():0U)<<'\n';return 2;}
            for(std::size_t index=0;index<positions->element_count;++index){float expected{};
                std::memcpy(&expected,positions->bytes.data()+index*sizeof(float),sizeof(float));
                const auto& vertex=mesh.value().vertices[index/3U];const float actual=index%3U==0U?
                    vertex.position.x:index%3U==1U?vertex.position.y:vertex.position.z;
                if(std::abs(actual-expected)>2e-6){std::cerr<<"face/neck position mismatch item="
                    <<item_id<<" element="<<index<<" expected="<<expected<<" actual="<<actual<<'\n';return 2;}}
            for(std::size_t index=0;index<indices->element_count;++index){std::uint32_t expected{};
                std::memcpy(&expected,indices->bytes.data()+index*sizeof(expected),sizeof(expected));
                if(expected!=mesh.value().indices[index]){std::cerr<<"face/neck index mismatch item="
                    <<item_id<<" element="<<index<<" expected="<<expected<<" actual="
                    <<mesh.value().indices[index]<<'\n';return 2;}}
            if(!compareGearFixture(mesh.value(),fixture,item_id))return 2;
        }
        constexpr std::array<std::pair<EquipmentSlot,std::string_view>,19U> accessory_items{{
            {EquipmentSlot::Utility3,"radio_handheld"},{EquipmentSlot::Utility3,"binoculars"},
            {EquipmentSlot::LeftHip,"map_case"},{EquipmentSlot::LeftHip,"pouch_medical"},
            {EquipmentSlot::LeftHip,"pouch_tools"},{EquipmentSlot::MeleeWeapon,"knife"},
            {EquipmentSlot::Throwable,"grenade"},{EquipmentSlot::SecondaryWeapon,"sidearm"},
            {EquipmentSlot::Back,"pack_medium"},{EquipmentSlot::Back,"pack_medical"},
            {EquipmentSlot::Back,"pack_radio"},{EquipmentSlot::Back,"pack_engineer"},
            {EquipmentSlot::Head,"helmet_light"},{EquipmentSlot::TorsoArmor,"plate_carrier"},
            {EquipmentSlot::ChestRig,"webbing"},{EquipmentSlot::ChestRig,"chest_assault"},
            {EquipmentSlot::ChestRig,"chest_medical"},{EquipmentSlot::ChestRig,"chest_tools"},
            {EquipmentSlot::Belt,"belt_light"}}};
        for(const auto& [slot,item_id]:accessory_items){EquipmentOverrideSet overrides{};
            overrides.slots[equipmentSlotIndex(slot)]=EquipmentOverride::item(
                genomes::foundation::stable_id(item_id));
            const auto state=EquipmentResolver::resolve(0U,0U,overrides);
            const auto fit=state?EquipmentFitter::build(state.value(),fit_phenotype.value(),fit_rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(state.error());
            const auto gear=fit?GearGenerator::build(state.value(),fit.value(),fit_rig.value(),
                kDefaultUniformColor,3U,.25F):genomes::foundation::Result<GearArtifact,genomes::foundation::Error>::failure(fit.error());
            const auto mesh=gear?GearSurfaceGenerator::build(gear.value()):
                genomes::foundation::Result<AppearanceMesh,genomes::foundation::Error>::failure(gear.error());
            const auto item_binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/equipment-"+std::string(item_id)+"-0-high-v1.gnif";
            const auto fixture=genomes::test::infantry_fixture::read(item_binary,item_binary+".json");
            const auto* positions=fixture.find("gear.positions");const auto* indices=fixture.find("gear.indices");
            if(!mesh||positions==nullptr||indices==nullptr||positions->element_count!=mesh.value().vertices.size()*3U||
               indices->element_count!=mesh.value().indices.size()){
                std::cerr<<"accessory shape mismatch item="<<item_id<<" expectedVertices="
                    <<(positions?positions->element_count/3U:0U)<<" actualVertices="
                    <<(mesh?mesh.value().vertices.size():0U)<<'\n';return 2;}
            for(std::size_t index=0;index<positions->element_count;++index){float expected{};
                std::memcpy(&expected,positions->bytes.data()+index*sizeof(float),sizeof(float));
                const auto& vertex=mesh.value().vertices[index/3U];const float actual=index%3U==0U?
                    vertex.position.x:index%3U==1U?vertex.position.y:vertex.position.z;
                if(std::abs(actual-expected)>2e-6){std::cerr<<"accessory position mismatch item="
                    <<item_id<<" element="<<index<<" expected="<<expected<<" actual="<<actual<<'\n';return 2;}}
            for(std::size_t index=0;index<indices->element_count;++index){std::uint32_t expected{};
                std::memcpy(&expected,indices->bytes.data()+index*sizeof(expected),sizeof(expected));
                if(expected!=mesh.value().indices[index]){std::cerr<<"accessory index mismatch item="
                    <<item_id<<" element="<<index<<" expected="<<expected<<" actual="
                    <<mesh.value().indices[index]<<'\n';return 2;}}
            if(!compareGearFixture(mesh.value(),fixture,item_id))return 2;
        }
    }
    {
        constexpr std::array<std::string_view, 4U> rig_fixtures{
            "rig-0-v1.json", "rig-8841-v1.json", "rig-1003-v1.json",
            "rig-1592598566-v1.json"};
        for (const std::string_view fixture_name : rig_fixtures) {
            std::ifstream fixture_file(std::string(GENOMES_SOURCE_DIR) +
                "/reference/fixtures/infantry/" + std::string(fixture_name));
            const auto fixture_json = nlohmann::json::parse(fixture_file);
            const auto seed = fixture_json.at("request").at("seed").get<genomes::proc::Seed>();
            const double variation = fixture_json.at("request").at("variation").get<double>();
            const auto genome = InfantryGenome::generate(seed, variation);
            if (!genome) {
                std::cerr << "rig genome failed fixture=" << fixture_name << '\n';
                return 2;
            }
            const auto phenotype = PhenotypeResolver::resolve(genome.value());
            const auto rig = phenotype
                ? RigBuilder::build(phenotype.value().body, phenotype.value().face)
                : genomes::foundation::Result<SkeletonData, genomes::foundation::Error>::failure(
                      phenotype.error());
            if (!rig) {
                std::cerr << "rig build failed fixture=" << fixture_name << '\n';
                return 2;
            }
            const auto anatomy=FaceAnatomyEvaluator::resolve(phenotype.value());
            if(!anatomy){std::cerr<<"face anatomy failed fixture="<<fixture_name<<'\n';return 2;}
            const auto& expected_levels=fixture_json.at("anatomy").at("face").at("levels");
            if(anatomy.value().head_sections.size()!=expected_levels.size()){
                std::cerr<<"face anatomy level count mismatch fixture="<<fixture_name
                         <<" expected="<<expected_levels.size()<<" actual="
                         <<anatomy.value().head_sections.size()<<'\n';return 2;
            }
            for(std::size_t level=0;level<expected_levels.size();++level){
                const auto& actual=anatomy.value().head_sections[level];
                const std::array<double,4> values{actual.y,actual.half_width,actual.half_depth,actual.center_z};
                for(std::size_t component=0;component<values.size();++component){
                    const double reference=expected_levels.at(level).at(component).get<double>()*
                                           phenotype.value().body.height;
                    if(std::abs(values[component]-reference)>2e-6){
                        std::cerr<<"face anatomy level mismatch fixture="<<fixture_name
                                 <<" level="<<level<<" component="<<component
                                 <<" expected="<<reference<<" actual="<<values[component]<<'\n';return 2;
                    }
                }
            }
            for(const auto& sample:fixture_json.at("anatomy").at("face").at("sectionSamples")){
                const float y=sample.at(0).get<float>()*phenotype.value().body.height;
                const auto actual=FaceAnatomyEvaluator::sectionAt(anatomy.value(),y);
                const std::array<double,4> values{actual.y,actual.half_width,actual.half_depth,actual.center_z};
                for(std::size_t component=0;component<values.size();++component){
                    const double reference=sample.at(component).get<double>()*phenotype.value().body.height;
                    if(std::abs(values[component]-reference)>2e-6){std::cerr
                        <<"face anatomy sample mismatch fixture="<<fixture_name
                        <<" y="<<y<<" component="<<component<<" expected="<<reference
                        <<" actual="<<values[component]<<'\n';return 2;}
                }
            }
            const auto& expected_bones = fixture_json.at("rig");
            if (expected_bones.size() != rig.value().bones().size()) {
                std::cerr << "rig bone count mismatch fixture=" << fixture_name << '\n';
                return 2;
            }
            for (std::size_t index = 0; index < rig.value().bones().size(); ++index) {
                const auto& actual = rig.value().bones()[index];
                const auto& expected = expected_bones.at(index);
                const std::string expected_parent = actual.parent == kInvalidBoneIndex
                    ? "unitWorldRoot"
                    : std::string(rig.value().bones()[actual.parent].name);
                if (expected.at("name").get<std::string>() != actual.name ||
                    expected.at("parent").get<std::string>() != expected_parent) {
                    std::cerr << "rig schema mismatch fixture=" << fixture_name
                              << " bone=" << index << '\n';
                    return 2;
                }
                const std::array<double, 3U> position{
                    actual.local_bind.translation.x, actual.local_bind.translation.y,
                    actual.local_bind.translation.z};
                for (std::size_t component = 0; component < position.size(); ++component) {
                    const double reference = expected.at("position").at(component).get<double>();
                    if (std::abs(position[component] - reference) > 2.0e-6) {
                        std::cerr << "rig position mismatch fixture=" << fixture_name
                                  << " bone=" << actual.name << " component=" << component
                                  << " expected=" << reference
                                  << " actual=" << position[component] << '\n';
                        return 2;
                    }
                }
                const std::array<double, 16U> inverse_bind{
                    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                    actual.inverse_bind.translation.x, actual.inverse_bind.translation.y,
                    actual.inverse_bind.translation.z, 1};
                for (std::size_t component = 0; component < inverse_bind.size(); ++component) {
                    const double reference = expected.at("inverseBind").at(component).get<double>();
                    if (std::abs(inverse_bind[component] - reference) > 2.0e-6) {
                        std::cerr << "rig inverse bind mismatch fixture=" << fixture_name
                                  << " bone=" << actual.name << " component=" << component
                                  << " expected=" << reference
                                  << " actual=" << inverse_bind[component] << '\n';
                        return 2;
                    }
                }
            }
        }
    }
    {
        std::ifstream catalog_file(std::string(GENOMES_SOURCE_DIR) +
            "/reference/fixtures/infantry/genome_catalog_v1.json");
        const auto catalog = nlohmann::json::parse(catalog_file);
        assert(catalog.at("provenance").at("sourceCommit") ==
               "da885ca68b2ae63154a004574fed00eb9dfeb458");
        assert(catalog.at("provenance").at("sourceTree") ==
               "50e15a425f00611df1900b3d56c1d382978ec19e");
        assert(catalog.at("geneNames").size() == GenomeGeneCount);
        for (std::size_t gene_index = 0; gene_index < GenomeGeneCount; ++gene_index) {
            assert(catalog.at("geneNames").at(gene_index).get<std::string>() ==
                   genomeGeneName(static_cast<GenomeGene>(gene_index)));
        }
        assert(catalog.at("catalog").size() == 1024U);
        const std::array<std::string_view, 21U> body_names{
            "frame", "mass", "muscle", "fat", "torsoLegBias", "shoulderWidthScale",
            "hipWidthScale", "chestWidthScale", "chestDepthScale", "waistWidthScale",
            "waistDepthScale", "armThicknessScale", "legThicknessScale", "neckScale",
            "legLengthScale", "armLengthScale", "hipY", "headScale", "handScale",
            "footScale", "skinColor"};
        const std::array<std::string_view, 65U> face_names{
            "headWidthScale", "headDepthScale", "headLengthScale", "foreheadWidthScale",
            "foreheadSlope", "templeWidthScale", "browRidge", "jawWidthScale",
            "jawLengthScale", "jawAngle", "chinWidthScale", "chinHeight",
            "chinProjection", "cheekboneScale", "cheekboneY", "cheekFullness",
            "midfaceProjection", "eyeSpacing", "eyeWidthScale", "eyeHeightScale",
            "eyeSizeScale", "eyeRoundness", "eyeDepth", "eyeTilt", "eyeY", "eyeColor",
            "browY", "browThickness", "browTilt", "browSpacing", "noseWidthScale",
            "noseLengthScale", "noseProjectionScale", "noseBridgeScale",
            "noseTipWidthScale", "noseTipRotation", "nostrilWidthScale", "mouthWidth",
            "upperLip", "lowerLip", "mouthY", "earScale", "earAngle", "hairColor",
            "hairStyle", "hairDensity", "hairThickness", "hairVolume", "hairline",
            "templeRecession", "widowPeak", "neutralEyeOpen", "neutralBrow",
            "neutralMouth", "eyeAsymmetry", "browAsymmetry", "mouthAsymmetry",
            "earAsymmetry", "blinkInterval", "blinkDuration", "gazeRestlessness",
            "expressionScale", "eyeExpressionScale", "mouthExpressionScale",
            "browExpressionScale"};
        assert(catalog.at("phenotypeBodyNames").size() == body_names.size());
        for (std::size_t index = 0; index < body_names.size(); ++index)
            assert(catalog.at("phenotypeBodyNames").at(index).get<std::string>() ==
                   body_names[index]);
        assert(catalog.at("phenotypeFaceNames").size() == face_names.size());
        for (std::size_t index = 0; index < face_names.size(); ++index)
            assert(catalog.at("phenotypeFaceNames").at(index).get<std::string>() ==
                   face_names[index]);
        for (const auto& capture : catalog.at("catalog")) {
            const auto generated = InfantryGenome::generate(
                capture.at("seed").get<genomes::proc::Seed>(), 1.0F);
            assert(generated);
            const auto& expected = capture.at("genes");
            assert(expected.size() == GenomeGeneCount);
            for (std::size_t gene_index = 0; gene_index < GenomeGeneCount; ++gene_index) {
                const double actual = generated.value().geneValue(
                    static_cast<GenomeGene>(gene_index));
                assert(std::abs(actual - expected.at(gene_index).get<double>()) <= 2.0e-6);
            }
            const auto resolved = PhenotypeResolver::resolve(generated.value());
            if (!resolved) {
                std::cerr << "phenotype resolution failed seed=" << capture.at("seed")
                          << " error=" << resolved.error().message << '\n';
                return 2;
            }
            const auto actual_body = bodyValues(resolved.value().body);
            const auto& expected_body = capture.at("body");
            assert(expected_body.size() == actual_body.size());
            for (std::size_t index = 0; index < actual_body.size(); ++index) {
                const double tolerance = index + 1U == actual_body.size() ? 0.0 : 2.0e-6;
                const double reference = expected_body.at(index).get<double>();
                if (std::abs(actual_body[index] - reference) > tolerance) {
                    std::cerr << "phenotype body mismatch seed=" << capture.at("seed")
                              << " field=" << body_names[index] << " expected=" << reference
                              << " actual=" << actual_body[index] << '\n';
                    return 2;
                }
            }
            const auto actual_face = faceValues(resolved.value().face);
            const auto& expected_face = capture.at("face");
            assert(expected_face.size() == actual_face.size());
            for (std::size_t index = 0; index < actual_face.size(); ++index) {
                const bool integral = index == 25U || index == 43U || index == 44U;
                const double tolerance = integral ? 0.0 : 2.0e-6;
                const double reference = expected_face.at(index).get<double>();
                if (std::abs(actual_face[index] - reference) > tolerance) {
                    std::cerr << "phenotype face mismatch seed=" << capture.at("seed")
                              << " field=" << face_names[index] << " expected=" << reference
                              << " actual=" << actual_face[index] << '\n';
                    return 2;
                }
            }
        }
    }
    std::ifstream fixture(std::string(GENOMES_SOURCE_DIR) +
                          "/reference/fixtures/infantry/semantic_reference_v1.json");
    const std::string fixture_text((std::istreambuf_iterator<char>(fixture)),
                                   std::istreambuf_iterator<char>());
    assert(fixture_text.find("\"schema_version\": 1") != std::string::npos);
    assert(fixture_text.find("da885ca68b2ae63154a004574fed00eb9dfeb458") !=
           std::string::npos);
    assert(fixture_text.find("1592598566") != std::string::npos);
    assert(fixture_text.find("305419896") != std::string::npos);
    assert(fixture_text.find("3405691582") != std::string::npos);
    const std::array<genomes::proc::Seed, 3U> fixture_seeds{
        0x5EED2026U, 0x12345678U, 0xCAFEBABEU};
    for (const auto seed : fixture_seeds) {
        const std::size_t capture_start = fixture_text.find(
            "\"seed\": " + std::to_string(seed));
        assert(capture_start != std::string::npos);
        const auto genome = InfantryGenome::generate(seed, 1.0F);
        assert(genome);
        const auto phenotype = PhenotypeResolver::resolve(genome.value());
        assert(phenotype);
        const float reference_height = fixtureNumber(fixture_text, capture_start, "height");
        const float reference_shoulder = fixtureNumber(
            fixture_text, capture_start, "shoulder_width_scale");
        const float reference_hip = fixtureNumber(
            fixture_text, capture_start, "hip_width_scale");
        const float reference_eye_ratio = fixtureNumber(fixture_text, capture_start, "eye_y");
        const float reference_mouth_ratio = fixtureNumber(fixture_text, capture_start, "mouth_y");
        const float reference_head_level_count = fixtureNumber(
            fixture_text, capture_start, "head_level_count");
        assert(reference_height >= 1.60F && reference_height <= 1.95F);
        assert(std::abs(phenotype.value().body.height - reference_height) < 1.0e-6F);
        assert(std::abs(phenotype.value().body.shoulder_width /
                            (0.256F * phenotype.value().body.height) - reference_shoulder) <
               1.0e-5F);
        assert(std::abs(phenotype.value().body.hip_width -
                        (0.104F * phenotype.value().body.height * reference_hip)) < 1.0e-5F);
        assert(std::abs(phenotype.value().face.eye_y / phenotype.value().body.height -
                        reference_eye_ratio) < 0.02F);
        assert(std::abs(phenotype.value().face.mouth_y / phenotype.value().body.height -
                        reference_mouth_ratio) < 0.02F);
        const auto anatomy = FaceAnatomyEvaluator::resolve(phenotype.value());
        assert(anatomy && anatomy.value().head_sections.size() ==
                            static_cast<std::size_t>(reference_head_level_count));
    }
    constexpr std::array<genomes::proc::Seed, 5U> seeds{
        0U, 1U, 0x12345678U, 0xDEADBEEFU, 0xFFFFFFFFU};
    for (const auto seed : seeds) {
        const auto first = InfantryGenome::generate(seed, 1.0F);
        const auto second = InfantryGenome::generate(seed, 1.0F);
        assert(first && second && first.value().identityHash() == second.value().identityHash());
        const auto phenotype = PhenotypeResolver::resolve(first.value());
        const auto repeated = PhenotypeResolver::resolve(second.value());
        assert(phenotype && repeated && phenotype.value().cache_key == repeated.value().cache_key);
        const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
        assert(rig && rig.value().valid() && rig.value().bones().size() == kRigBoneCount);

        AppearanceOptions options{};
        options.seed = seed;
        options.hair_style = static_cast<HairStyle>(seed % 7U);
        const auto appearance = AppearanceCompiler::build(phenotype.value(), rig.value(), options);
        if(!appearance){std::cerr<<"appearance build failed seed="<<seed<<" error="
            <<appearance.error().message<<'\n';return 2;}
        if(!appearance.value().valid(rig.value())){std::cerr<<"appearance invalid seed="<<seed<<'\n';return 2;}

        for (const auto& loadout : infantryLoadouts()) {
            const auto equipment = EquipmentResolver::resolve(seed, loadout.id);
            assert(equipment && equipment.value().valid());
            const auto fit = EquipmentFitter::build(equipment.value(), phenotype.value(),
                                                     rig.value());
            assert(fit && fit.value().valid(rig.value()));
            const auto gear = GearGenerator::build(equipment.value(), fit.value(), rig.value());
            assert(gear && gear.value().valid(rig.value()));
        }

        const auto locomotion = LocomotionController::create(phenotype.value().body);
        assert(locomotion);
        LocomotionState locomotion_state = locomotion.value().initialState();
        assert(locomotion.value().setPreset(locomotion_state, BipedPreset::Walk));
        for (int tick = 0; tick < 60; ++tick) {
            assert(locomotion.value().step(locomotion_state, 1.0F / 60.0F));
        }
        assert(locomotion_state.valid());

        const auto face = FaceAnimator::create(seed, phenotype.value().face);
        assert(face);
        FaceAnimator face_copy = face.value();
        assert(face_copy.setExpression(FaceExpression::Alert, 0.5F));
        for (int tick = 0; tick < 60; ++tick) {
            assert(face_copy.step(1.0F / 60.0F));
        }
        assert(face_copy.output().valid() && face_copy.identity().valid());

        const auto damage_schema = RagdollSchema::build(phenotype.value().body, rig.value());
        assert(damage_schema && damage_schema.value().valid());
        const auto volumes = InfantryDamageModel::buildVolumes({1U, 1U}, rig.value());
        assert(volumes && volumes.value().size() == InfantryDamageModel::recipes().size());
    }
    {
        for(std::uint8_t style=0U;style<7U;++style){
            const std::string binary=std::string(GENOMES_SOURCE_DIR)+
                "/reference/fixtures/infantry/surface-hair-"+std::to_string(style)+"-high-v1.gnif";
            const auto fixture=genomes::test::infantry_fixture::read(binary,binary+".json");
            const auto genome=InfantryGenome::generate(0U,0.0);GenomeOverrides overrides{};
            if(!overrides.set("face.hairStyleGene",(static_cast<double>(style)+.5)/7.0))return 2;
            const auto phenotype=genome?PhenotypeResolver::resolve(genome.value(),overrides):
                genomes::foundation::Result<PhenotypeArtifact,genomes::foundation::Error>::failure(genome.error());
            const auto rig=phenotype?RigBuilder::build(phenotype.value().body,phenotype.value().face):
                genomes::foundation::Result<SkeletonData,genomes::foundation::Error>::failure(phenotype.error());
            const auto state=EquipmentResolver::resolve(0U,0U);
            const auto fit=state&&phenotype&&rig?EquipmentFitter::build(state.value(),phenotype.value(),rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure({genomes::foundation::ErrorCode::InvalidState,"hair setup"});
            const auto avatar=fit?ReferenceBodySurfaceGenerator::build(fit.value(),rig.value(),kDefaultUniformColor,3U):
                genomes::foundation::Result<ReferenceAvatarSurface,genomes::foundation::Error>::failure(fit.error());
            const auto* positions=fixture.find("body.positions");const auto* indices=fixture.find("body.indices");
            if(!avatar||positions==nullptr||indices==nullptr||positions->element_count!=avatar.value().mesh.vertices.size()*3U||
               indices->element_count!=avatar.value().mesh.indices.size()){
                std::cerr<<"hair fixture shape mismatch style="<<static_cast<unsigned>(style)<<'\n';return 2;}
            for(std::size_t index=0;index<positions->element_count;++index){float expected{};std::memcpy(&expected,positions->bytes.data()+index*sizeof(float),sizeof(float));
                const auto& p=avatar.value().mesh.vertices[index/3U].position;const float actual=index%3U==0U?p.x:index%3U==1U?p.y:p.z;
                if(std::abs(actual-expected)>2e-6F){std::cerr<<"hair position mismatch style="<<static_cast<unsigned>(style)<<" element="<<index<<'\n';return 2;}}
            for(std::size_t index=0;index<indices->element_count;++index){std::uint32_t expected{};std::memcpy(&expected,indices->bytes.data()+index*sizeof(expected),sizeof(expected));
                if(expected!=avatar.value().mesh.indices[index]){std::cerr<<"hair index mismatch style="<<static_cast<unsigned>(style)<<" element="<<index<<'\n';return 2;}}
        }
    }
    {
        constexpr std::array<std::pair<std::string_view, genomes::proc::Seed>, 4U>
            animation_fixtures{{{"0", 0U}, {"8841", 8841U}, {"1003", 1003U},
                                {"1592598566", 0x5EED2026U}}};
        for (const auto& [stem, seed] : animation_fixtures) {
            const std::string binary = std::string(GENOMES_SOURCE_DIR) +
                "/reference/fixtures/infantry/animation-" + std::string(stem) + "-v1.gnif";
            const auto fixture = genomes::test::infantry_fixture::read(binary, binary + ".json");
            const auto genome = InfantryGenome::generate(seed, 0.0);
            const auto phenotype = genome ? PhenotypeResolver::resolve(genome.value()) :
                genomes::foundation::Result<PhenotypeArtifact, genomes::foundation::Error>::failure(
                    genome.error());
            const auto rig = phenotype ? RigBuilder::build(phenotype.value().body,
                                                            phenotype.value().face) :
                genomes::foundation::Result<SkeletonData, genomes::foundation::Error>::failure(
                    phenotype.error());
            const auto animation_equipment=phenotype&&rig?EquipmentResolver::resolve(seed,0U):
                genomes::foundation::Result<EquipmentState,genomes::foundation::Error>::failure(
                    {genomes::foundation::ErrorCode::InvalidState,"animation equipment"});
            const auto animation_fit=animation_equipment&&phenotype&&rig?
                EquipmentFitter::build(animation_equipment.value(),phenotype.value(),rig.value()):
                genomes::foundation::Result<EquipmentFit,genomes::foundation::Error>::failure(
                    {genomes::foundation::ErrorCode::InvalidState,"animation fit"});
            const auto animation_surface=animation_fit?
                ReferenceBodySurfaceGenerator::build(animation_fit.value(),rig.value(),kDefaultUniformColor,3U):
                genomes::foundation::Result<ReferenceAvatarSurface,genomes::foundation::Error>::failure(
                    {genomes::foundation::ErrorCode::InvalidState,"animation surface"});
            const auto* translations = fixture.find("animation.REST.translations");
            const auto* rotations = fixture.find("animation.REST.rotations");
            const auto* morphs = fixture.find("animation.REST.morphWeights");
            const auto* locomotion = fixture.find("animation.REST.locomotion");
            if (!rig || translations == nullptr || rotations == nullptr || morphs == nullptr ||
                locomotion == nullptr ||
                translations->element_count != 600U * kRigBoneCount * 3U ||
                rotations->element_count != 600U * kRigBoneCount * 4U ||
                morphs->element_count != 600U * 4U || locomotion->element_count != 600U * 10U) {
                std::cerr << "REST animation fixture shape mismatch seed=" << seed << '\n';
                return 2;
            }
            const auto valueAt=[](const auto& stream,std::size_t index){float value{};
                std::memcpy(&value,stream.bytes.data()+index*sizeof(float),sizeof(float));return value;};
            for(std::size_t frame=0;frame<600U;++frame)for(std::size_t bone=0;bone<kRigBoneCount;++bone){
                const auto& bind=rig.value().bones()[bone].local_bind;
                const std::array<float,3U> expected_translation{bind.translation.x,bind.translation.y,bind.translation.z};
                const std::array<float,4U> expected_rotation{bind.rotation.x,bind.rotation.y,bind.rotation.z,bind.rotation.w};
                for(std::size_t component=0;component<3U;++component){const auto index=(frame*kRigBoneCount+bone)*3U+component;
                    if(std::abs(valueAt(*translations,index)-expected_translation[component])>2e-5F){
                        std::cerr<<"REST translation mismatch seed="<<seed<<" frame="<<frame<<" bone="<<bone<<" component="<<component<<'\n';return 2;}}
                for(std::size_t component=0;component<4U;++component){const auto index=(frame*kRigBoneCount+bone)*4U+component;
                    if(std::abs(valueAt(*rotations,index)-expected_rotation[component])>2e-6F){
                        std::cerr<<"REST rotation mismatch seed="<<seed<<" frame="<<frame<<" bone="<<bone<<" component="<<component<<'\n';return 2;}}}
            for(std::size_t index=0;index<morphs->element_count;++index)if(std::abs(valueAt(*morphs,index))>2e-6F){
                std::cerr<<"REST morph mismatch seed="<<seed<<" element="<<index<<'\n';return 2;}
            const auto rest_gait=PostureProfile::gait(0.0F,0.0F,phenotype.value().body);
            const std::array<float,10U> expected_locomotion{0.0F,0.0F,static_cast<float>(rest_gait.cycle_m),
                rest_gait.duty,rest_gait.run,rest_gait.sprint,rest_gait.lift_m,
                rest_gait.amplitude,rest_gait.cadence,0.0F};
            for(std::size_t frame=0;frame<600U;++frame)for(std::size_t component=0;component<10U;++component){
                const auto index=frame*10U+component;
                if(std::abs(valueAt(*locomotion,index)-expected_locomotion[component])>2e-6F){
                    std::cerr<<"REST locomotion mismatch seed="<<seed<<" frame="<<frame
                             <<" component="<<component<<" expected="<<valueAt(*locomotion,index)
                             <<" actual="<<expected_locomotion[component]<<'\n';return 2;}}
            struct BipedFixture { const char* name; BipedPreset preset; float actual_speed; };
            const auto& body=phenotype.value().body;
            const std::array<BipedFixture,5U> biped{{
                {"IDLE",BipedPreset::Idle,0.0F},{"WALK",BipedPreset::Walk,1.4F},
                {"RUN",BipedPreset::Run,3.2F},{"CROUCH",BipedPreset::Crouch,0.0F},
                {"CROUCH_WALK",BipedPreset::CrouchWalk,.7F}}};
            const auto controller=LocomotionController::create(body);
            if(!controller){std::cerr<<"locomotion controller creation failed seed="<<seed<<'\n';return 2;}
            const auto verify_prone=[&](const char* state_name,bool moving)->bool{
                const std::string prefix=std::string("animation.")+state_name;
                const auto* targets=fixture.find(prefix+".bipedTargets");
                const auto* limbs=fixture.find(prefix+".limbTargets");
                const auto* rotations_stream=fixture.find(prefix+".targetRotations");
                const auto* channels=fixture.find(prefix+".poseChannels");
                if(targets==nullptr||limbs==nullptr||rotations_stream==nullptr||channels==nullptr)return false;
                auto native=controller.value().initialState();
                if(!controller.value().setFamily(native,LocomotionFamily::Prone,moving))return false;
                native.actual_speed_mps=moving?.35F:0.0F;
                auto animation_result=AnimationSystem::create();if(!animation_result)return false;
                auto animation=std::move(animation_result.value());
                AnimationEntity entity{};entity.semantic_id=3U;entity.skeleton=&rig.value();
                // The JS animation fixture uses the flat contact surface, not
                // the skinned body mesh, for prone hand/foot placement.
                entity.surface=nullptr;
                entity.locomotion=&controller.value();entity.locomotion_state=&native;
                entity.lod.setTier(AnimationLOD::Near);std::array<AnimationEntity,1U> entities{entity};
                constexpr std::array<std::size_t,14U> sampled_bones{
                    0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,14U,15U,16U,17U};
                for(std::size_t frame=0;frame<600U;++frame){
                    if(moving){native.phase+=.35/60.0/controller.value().proneCycleMeters();
                        native.phase-=std::floor(native.phase);}
                    if(!animation.evaluate(entities,frame,1.0F/60.0F))return false;
                    const auto& pose=animation.currentSnapshot().poses.front();
                    const auto& hips=pose.target_bones[boneIndex(BoneId::Hips)].translation;
                    const std::array<float,4U> expected_channels{hips.x/body.height,hips.y/body.height,
                        hips.z/body.height,pose.target_hand_curl};
                    for(std::size_t component=0;component<4U;++component)
                        if(std::abs(valueAt(*channels,frame*4U+component)-expected_channels[component])>2e-5F)return false;
                    for(const auto bone:sampled_bones)for(std::size_t component=0;component<4U;++component){
                        const auto element=(frame*kRigBoneCount+bone)*4U+component;
                        const auto& q=pose.target_bones[bone].rotation;
                        const float actual=component==0U?q.x:component==1U?q.y:component==2U?q.z:q.w;
                        if(std::abs(valueAt(*rotations_stream,element)-actual)>2e-5F)return false;}
                    for(std::size_t side=0;side<2U;++side){
                        const std::array<float,11U> feet{pose.foot_targets[side].x,pose.foot_targets[side].y,
                            pose.foot_targets[side].z,pose.knee_targets[side].x,pose.knee_targets[side].y,
                            pose.knee_targets[side].z,pose.foot_plant[side],pose.foot_support[side],
                            pose.foot_pitch[side],pose.toe_pitch[side],pose.foot_yaw[side]};
                        const std::array<float,11U> arms{pose.hand_targets[side].x,pose.hand_targets[side].y,
                            pose.hand_targets[side].z,pose.elbow_targets[side].x,pose.elbow_targets[side].y,
                            pose.elbow_targets[side].z,pose.hand_plant[side],pose.hand_lift[side],
                            pose.foot_relative[side],pose.ankle_pitch[side],pose.ankle_yaw[side]};
                        for(std::size_t component=0;component<11U;++component){
                            const auto element=frame*22U+side*11U+component;
                            if(std::abs(valueAt(*targets,element)-feet[component])>5e-5F||
                               std::abs(valueAt(*limbs,element)-arms[component])>5e-5F)return false;}
                    }
                    for(std::size_t bone=0U;bone<kRigBoneCount;++bone){
                        const auto base=(frame*kRigBoneCount+bone)*4U;
                        const auto& q=pose.bones[bone].rotation;
                        const double d=static_cast<double>(valueAt(*rotations_stream,base))*q.x+
                            static_cast<double>(valueAt(*rotations_stream,base+1U))*q.y+
                            static_cast<double>(valueAt(*rotations_stream,base+2U))*q.z+
                            static_cast<double>(valueAt(*rotations_stream,base+3U))*q.w;
                        const double en=std::sqrt(static_cast<double>(valueAt(*rotations_stream,base))*valueAt(*rotations_stream,base)+
                            static_cast<double>(valueAt(*rotations_stream,base+1U))*valueAt(*rotations_stream,base+1U)+
                            static_cast<double>(valueAt(*rotations_stream,base+2U))*valueAt(*rotations_stream,base+2U)+
                            static_cast<double>(valueAt(*rotations_stream,base+3U))*valueAt(*rotations_stream,base+3U));
                        const double an=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+
                            static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
                        if(2.0*std::acos(std::clamp(std::abs(d)/(en*an),0.0,1.0))>2e-4)return false;
                    }
                }
                return true;
            };
            for(const auto& state:biped){
                const auto* stream=fixture.find(std::string("animation.")+state.name+".locomotion");
                const auto* posture_stream=fixture.find(std::string("animation.")+state.name+".posture");
                const auto* targets_stream=fixture.find(std::string("animation.")+state.name+".bipedTargets");
                const auto* target_rotations=fixture.find(std::string("animation.")+state.name+".targetRotations");
                const auto* final_rotations=fixture.find(std::string("animation.")+state.name+".rotations");
                const auto* pose_channels=fixture.find(std::string("animation.")+state.name+".poseChannels");
                const auto* foot_goals=fixture.find(std::string("animation.")+state.name+".footGoals");
                if(stream==nullptr||stream->element_count!=600U*10U||posture_stream==nullptr||
                   posture_stream->element_count!=600U*10U||targets_stream==nullptr||
                   targets_stream->element_count!=600U*22U||target_rotations==nullptr||
                   target_rotations->element_count!=600U*kRigBoneCount*4U||final_rotations==nullptr||
                   final_rotations->element_count!=600U*kRigBoneCount*4U||pose_channels==nullptr||
                   pose_channels->element_count!=600U*4U||foot_goals==nullptr||
                   foot_goals->element_count!=600U*6U){std::cerr<<"locomotion fixture shape mismatch state="<<state.name<<'\n';return 2;}
                auto native=controller.value().initialState();
                if(!controller.value().setPreset(native,state.preset,true))return 2;
                auto animation_result=AnimationSystem::create();
                if(!animation_result)return 2;
                auto animation=std::move(animation_result.value());
                AnimationEntity entity{};entity.semantic_id=1U;entity.skeleton=&rig.value();
                // Terrain contact is flat in this fixture, while support
                // points still come from the authored skinned body surface.
                entity.surface=&animation_surface.value().mesh;
                entity.locomotion=&controller.value();entity.locomotion_state=&native;
                entity.lod.setTier(AnimationLOD::Near);
                std::array<AnimationEntity,1U> entities{entity};
                for(std::size_t frame=0;frame<600U;++frame){
                    if(!controller.value().sampleGait(native,state.actual_speed,1.0F/60.0F,false))return 2;
                    if(state.actual_speed>0.0F){native.phase+=state.actual_speed/60.0F/native.cycle_m;
                        native.phase-=std::floor(native.phase);}
                    const std::array<float,9U> expected{native.actual_crouch,native.target_speed_mps,
                        static_cast<float>(native.cycle_m),native.duty,native.run_weight,native.sprint_weight,
                        native.lift_m,native.amplitude,native.cadence};
                    for(std::size_t component=0;component<9U;++component){
                        const float actual=valueAt(*stream,frame*10U+component);
                        if(std::abs(actual-expected[component])>2e-6F){
                            std::cerr<<"locomotion profile mismatch seed="<<seed<<" state="<<state.name
                                     <<" frame="<<frame<<" component="<<component<<" expected="<<actual
                                     <<" actual="<<expected[component]<<'\n';return 2;}}
                    const auto p=controller.value().posture(native);
                    const std::array<float,10U> expected_posture{p.pelvis_pitch,p.hip_y,p.hip_z,
                        p.lower_pitch,p.upper_pitch,p.chest_pitch,p.stance_half,p.foot_z,
                        p.foot_yaw,p.knee_half};
                    for(std::size_t component=0;component<10U;++component){
                        const float actual=valueAt(*posture_stream,frame*10U+component);
                        if(std::abs(actual-expected_posture[component])>2e-6F){
                            std::cerr<<"posture profile mismatch seed="<<seed<<" state="<<state.name
                                     <<" frame="<<frame<<" component="<<component<<" expected="<<actual
                                     <<" actual="<<expected_posture[component]<<'\n';return 2;}}
                    if(!animation.evaluate(entities,frame,1.0F/60.0F))return 2;
                    const auto& pose=animation.currentSnapshot().poses.front();
                    for(std::size_t side=0;side<2U;++side)for(std::size_t component=0;component<3U;++component){
                        const auto& goal=pose.foot_goals[side];
                        const float actual=component==0U?goal.x:component==1U?goal.y:goal.z;
                        if(std::abs(valueAt(*foot_goals,frame*6U+side*3U+component)-actual)>2e-5F){
                            std::cerr<<"biped foot goal mismatch seed="<<seed<<" state="<<state.name
                                     <<" frame="<<frame<<" side="<<side<<" component="<<component
                                     <<" expected="<<valueAt(*foot_goals,frame*6U+side*3U+component)
                                     <<" actual="<<actual<<'\n';return 2;}}
                    if(state.actual_speed==0.0F)for(std::size_t bone=0;bone<kRigBoneCount;++bone){
                        const auto base=(frame*kRigBoneCount+bone)*4U;
                        const auto& q=pose.bones[bone].rotation;
                        const double d=static_cast<double>(valueAt(*final_rotations,base))*q.x+
                            static_cast<double>(valueAt(*final_rotations,base+1U))*q.y+
                            static_cast<double>(valueAt(*final_rotations,base+2U))*q.z+
                            static_cast<double>(valueAt(*final_rotations,base+3U))*q.w;
                        const double en=std::sqrt(static_cast<double>(valueAt(*final_rotations,base))*valueAt(*final_rotations,base)+
                            static_cast<double>(valueAt(*final_rotations,base+1U))*valueAt(*final_rotations,base+1U)+
                            static_cast<double>(valueAt(*final_rotations,base+2U))*valueAt(*final_rotations,base+2U)+
                            static_cast<double>(valueAt(*final_rotations,base+3U))*valueAt(*final_rotations,base+3U));
                        const double an=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+
                            static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
                        const float angle=static_cast<float>(2.0*std::acos(std::clamp(std::abs(d)/(en*an),0.0,1.0)));
                        if(angle>2e-4F){std::cerr<<"biped final rotation mismatch seed="<<seed
                            <<" state="<<state.name<<" frame="<<frame<<" bone="<<bone
                            <<" angle="<<angle<<" expected="<<valueAt(*final_rotations,base)<<','
                            <<valueAt(*final_rotations,base+1U)<<','<<valueAt(*final_rotations,base+2U)<<','
                            <<valueAt(*final_rotations,base+3U)<<" actual="<<q.x<<','<<q.y<<','<<q.z<<','<<q.w
                            <<'\n';return 2;}
                    }
                    const auto& hips=pose.target_bones[boneIndex(BoneId::Hips)].translation;
                    const std::array<float,4U> expected_channels{hips.x/body.height,hips.y/body.height,
                        hips.z/body.height,pose.target_hand_curl};
                    for(std::size_t component=0;component<4U;++component){
                        const float actual=valueAt(*pose_channels,frame*4U+component);
                        if(std::abs(actual-expected_channels[component])>2e-5F){
                            std::cerr<<"biped pose channel mismatch seed="<<seed<<" state="<<state.name
                                     <<" frame="<<frame<<" component="<<component<<" expected="<<actual
                                     <<" actual="<<expected_channels[component]<<'\n';return 2;}}
                    constexpr std::array<std::size_t,14U> sampled_bones{
                        0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,14U,15U,16U,17U};
                    for(const auto bone:sampled_bones){
                        const auto base=(frame*kRigBoneCount+bone)*4U;
                        const RigQuaternion expected_q{valueAt(*target_rotations,base),
                            valueAt(*target_rotations,base+1U),valueAt(*target_rotations,base+2U),
                            valueAt(*target_rotations,base+3U)};
                        const auto& actual_q=pose.target_bones[bone].rotation;
                        const double raw_dot=static_cast<double>(expected_q.x)*actual_q.x+
                            static_cast<double>(expected_q.y)*actual_q.y+
                            static_cast<double>(expected_q.z)*actual_q.z+
                            static_cast<double>(expected_q.w)*actual_q.w;
                        const double expected_norm=std::sqrt(static_cast<double>(expected_q.x)*expected_q.x+
                            static_cast<double>(expected_q.y)*expected_q.y+static_cast<double>(expected_q.z)*expected_q.z+
                            static_cast<double>(expected_q.w)*expected_q.w);
                        const double actual_norm=std::sqrt(static_cast<double>(actual_q.x)*actual_q.x+
                            static_cast<double>(actual_q.y)*actual_q.y+static_cast<double>(actual_q.z)*actual_q.z+
                            static_cast<double>(actual_q.w)*actual_q.w);
                        const float angle=static_cast<float>(2.0*std::acos(std::clamp(
                            std::abs(raw_dot)/(expected_norm*actual_norm),0.0,1.0)));
                        if(angle>2e-4F){std::cerr<<"biped target rotation mismatch seed="<<seed
                            <<" state="<<state.name<<" frame="<<frame<<" bone="<<bone
                            <<" angle="<<angle<<" expected="<<expected_q.x<<','<<expected_q.y<<','<<expected_q.z<<','<<expected_q.w
                            <<" actual="<<actual_q.x<<','<<actual_q.y<<','<<actual_q.z<<','<<actual_q.w<<'\n';return 2;}
                    }
                    for(std::size_t side=0;side<2U;++side){
                        const std::array<float,11U> expected_target{pose.foot_targets[side].x,
                            pose.foot_targets[side].y,pose.foot_targets[side].z,
                            pose.knee_targets[side].x,pose.knee_targets[side].y,pose.knee_targets[side].z,
                            pose.foot_plant[side],pose.foot_support[side],pose.foot_pitch[side],
                            pose.toe_pitch[side],pose.foot_yaw[side]};
                        for(std::size_t component=0;component<11U;++component){
                            const auto element=frame*22U+side*11U+component;
                            const float actual=valueAt(*targets_stream,element);
                            const float target_tolerance=(component==6U||component==7U)?5e-5F:2e-5F;
                            if(std::abs(actual-expected_target[component])>target_tolerance){
                                std::cerr<<"biped target mismatch seed="<<seed<<" state="<<state.name
                                         <<" frame="<<frame<<" side="<<side<<" component="<<component
                                         <<" expected="<<actual<<" actual="<<expected_target[component]<<'\n';return 2;}}
                    }
                }
            }
            {
                const auto* targets=fixture.find("animation.SITTING.bipedTargets");
                const auto* rotations_stream=fixture.find("animation.SITTING.targetRotations");
                const auto* final_rotations=fixture.find("animation.SITTING.rotations");
                const auto* channels=fixture.find("animation.SITTING.poseChannels");
                if(targets==nullptr||rotations_stream==nullptr||channels==nullptr)return 2;
                auto native=controller.value().initialState();
                if(!controller.value().setFamily(native,LocomotionFamily::Seated))return 2;
                auto animation_result=AnimationSystem::create();if(!animation_result)return 2;
                auto animation=std::move(animation_result.value());
                AnimationEntity entity{};entity.semantic_id=2U;entity.skeleton=&rig.value();
                entity.surface=nullptr;
                entity.locomotion=&controller.value();entity.locomotion_state=&native;
                entity.lod.setTier(AnimationLOD::Near);std::array<AnimationEntity,1U> entities{entity};
                constexpr std::array<std::size_t,14U> sampled_bones{
                    0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,14U,15U,16U,17U};
                for(std::size_t frame=0;frame<600U;++frame){
                    if(!animation.evaluate(entities,frame,1.0F/60.0F))return 2;
                    const auto& pose=animation.currentSnapshot().poses.front();
                    if(final_rotations==nullptr)return 2;
                    for(std::size_t bone=0;bone<kRigBoneCount;++bone){
                        const auto base=(frame*kRigBoneCount+bone)*4U;
                        const RigQuaternion expected_q{valueAt(*final_rotations,base),
                            valueAt(*final_rotations,base+1U),valueAt(*final_rotations,base+2U),
                            valueAt(*final_rotations,base+3U)};
                        const auto& actual_q=pose.bones[bone].rotation;
                        const double raw_dot=static_cast<double>(expected_q.x)*actual_q.x+
                            static_cast<double>(expected_q.y)*actual_q.y+
                            static_cast<double>(expected_q.z)*actual_q.z+
                            static_cast<double>(expected_q.w)*actual_q.w;
                        const double expected_norm=std::sqrt(static_cast<double>(expected_q.x)*expected_q.x+
                            static_cast<double>(expected_q.y)*expected_q.y+static_cast<double>(expected_q.z)*expected_q.z+
                            static_cast<double>(expected_q.w)*expected_q.w);
                        const double actual_norm=std::sqrt(static_cast<double>(actual_q.x)*actual_q.x+
                            static_cast<double>(actual_q.y)*actual_q.y+static_cast<double>(actual_q.z)*actual_q.z+
                            static_cast<double>(actual_q.w)*actual_q.w);
                        const float angle=static_cast<float>(2.0*std::acos(std::clamp(
                            std::abs(raw_dot)/(expected_norm*actual_norm),0.0,1.0)));
                        if(angle>2e-4F){std::cerr<<"seated final rotation mismatch seed="<<seed
                            <<" frame="<<frame<<" bone="<<bone<<" angle="<<angle<<'\n';return 2;}
                    }
                    const auto& hips=pose.bones[boneIndex(BoneId::Hips)].translation;
                    const std::array<float,4U> expected_channels{hips.x/body.height,hips.y/body.height,
                        hips.z/body.height,pose.face.hands_relax};
                    for(std::size_t component=0;component<4U;++component)
                        if(std::abs(valueAt(*channels,frame*4U+component)-expected_channels[component])>2e-6F){
                            std::cerr<<"seated pose channel mismatch seed="<<seed<<" frame="<<frame
                                     <<" component="<<component<<'\n';return 2;}
                    for(const auto bone:sampled_bones)for(std::size_t component=0;component<4U;++component){
                        const auto element=(frame*kRigBoneCount+bone)*4U+component;
                        const auto& q=pose.target_bones[bone].rotation;
                        const float actual=component==0U?q.x:component==1U?q.y:component==2U?q.z:q.w;
                        if(std::abs(valueAt(*rotations_stream,element)-actual)>2e-6F){
                            std::cerr<<"seated rotation mismatch seed="<<seed<<" frame="<<frame
                                     <<" bone="<<bone<<" component="<<component<<'\n';return 2;}}
                    for(std::size_t side=0;side<2U;++side){
                        const std::array<float,6U> expected{pose.foot_targets[side].x,pose.foot_targets[side].y,
                            pose.foot_targets[side].z,pose.knee_targets[side].x,pose.knee_targets[side].y,
                            pose.knee_targets[side].z};
                        for(std::size_t component=0;component<6U;++component){
                            const auto element=frame*22U+side*11U+component;
                            if(std::abs(valueAt(*targets,element)-expected[component])>2e-6F){
                                std::cerr<<"seated target mismatch seed="<<seed<<" frame="<<frame
                                         <<" side="<<side<<" component="<<component<<'\n';return 2;}}
                    }
                }
            }
            if(!verify_prone("PRONE",false)||!verify_prone("PRONE_MOVE",true)){
                std::cerr<<"prone target parity mismatch seed="<<seed<<'\n';return 2;
            }
        }
    }
    return 0;
}
