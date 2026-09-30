cmake_minimum_required(VERSION 3.21)

project(Genomes VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenomesOptions)
include(GenomesWarnings)
include(GenomesDependencies)
include(GenomesPlatform)
include(GenomesCuda)
include(GenomesThreepp)
include(GenomesToolkitDependencies)

if(GENOMES_ENABLE_JS_REFERENCE_PARITY)
    find_program(GENOMES_NODE_EXECUTABLE NAMES node nodejs REQUIRED)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

genomes_configure_cuda()
genomes_configure_diligent()
genomes_configure_rmlui()
genomes_configure_sdl()
genomes_configure_threepp()
genomes_configure_earcut()

add_subdirectory(engine)
add_subdirectory(modules)
add_subdirectory(apps)
add_subdirectory(tools)

if(GENOMES_ENABLE_JS_REFERENCE_PARITY)
    set(GENOMES_INFANTRY_JS_FIXTURE_DIR
        "${CMAKE_BINARY_DIR}/reference/infantry" CACHE PATH
        "Build-tree output for regenerated infantry JavaScript fixtures")
    set(_genomes_infantry_fixture_commands)
    foreach(_seed IN ITEMS 0 8841 1003 1592598566)
        foreach(_detail IN ITEMS far world high)
            foreach(_equipment IN ITEMS neutral default full)
                list(APPEND _genomes_infantry_fixture_commands
                    COMMAND "${GENOMES_NODE_EXECUTABLE}"
                        "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
                        --mode full-avatar --seed "${_seed}" --detail "${_detail}"
                        --equipment "${_equipment}"
                        --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/avatar-${_seed}-${_detail}-${_equipment}.gnif"
                    COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/avatar-${_seed}-${_detail}-${_equipment}.gnif"
                        "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/full-avatar/avatar-${_seed}-${_detail}-${_equipment}.gnif"
                    COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/avatar-${_seed}-${_detail}-${_equipment}.gnif.json"
                        "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/full-avatar/avatar-${_seed}-${_detail}-${_equipment}.gnif.json")
            endforeach()
        endforeach()
        list(APPEND _genomes_infantry_fixture_commands
            COMMAND "${GENOMES_NODE_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
                --mode animation --seed "${_seed}" --detail high --equipment default
                --count 600
                --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/animation-${_seed}-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/animation-${_seed}-v1.gnif"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/animation-${_seed}-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/animation-${_seed}-v1.gnif.json"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/animation-${_seed}-v1.gnif.json")
    endforeach()
    foreach(_hair_style RANGE 0 6)
        list(APPEND _genomes_infantry_fixture_commands
            COMMAND "${GENOMES_NODE_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
                --mode surface --seed 0 --detail high --equipment neutral
                --hair-style "${_hair_style}"
                --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/surface-hair-${_hair_style}-high-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/surface-hair-${_hair_style}-high-v1.gnif"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/surface-hair-${_hair_style}-high-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/surface-hair-${_hair_style}-high-v1.gnif.json"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/surface-hair-${_hair_style}-high-v1.gnif.json")
    endforeach()
    foreach(_equipment_case IN ITEMS
            "field_cap|head" "patrol_cap|head" "beanie|head" "beret|head"
            "boonie_hat|head" "glasses|face" "goggles|face" "balaclava|face"
            "respirator|face" "scarf|neck" "neck_gaiter|neck"
            "radio_handheld|utility3" "binoculars|utility3" "map_case|leftHip"
            "pouch_medical|leftHip" "pouch_tools|leftHip" "knife|meleeWeapon"
            "grenade|throwable" "sidearm|secondaryWeapon" "pack_medium|back"
            "pack_medical|back" "pack_radio|back" "pack_engineer|back"
            "helmet_light|head" "plate_carrier|torsoArmor" "webbing|chestRig"
            "chest_assault|chestRig" "chest_medical|chestRig" "chest_tools|chestRig"
            "belt_light|belt")
        string(REPLACE "|" ";" _equipment_parts "${_equipment_case}")
        list(GET _equipment_parts 0 _equipment_id)
        list(GET _equipment_parts 1 _equipment_slot)
        list(APPEND _genomes_infantry_fixture_commands
            COMMAND "${GENOMES_NODE_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
                --mode equipment --seed 0 --detail high --equipment neutral
                --slot "${_equipment_slot}" --item "${_equipment_id}"
                --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-${_equipment_id}-0-high-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-${_equipment_id}-0-high-v1.gnif"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/equipment-${_equipment_id}-0-high-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-${_equipment_id}-0-high-v1.gnif.json"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/equipment-${_equipment_id}-0-high-v1.gnif.json")
    endforeach()
    list(APPEND _genomes_infantry_fixture_commands
        COMMAND "${GENOMES_NODE_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
            --mode equipment --seed 0 --detail high --equipment default
            --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-default-0-high-v1.gnif"
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-default-0-high-v1.gnif"
            "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/equipment-default-0-high-v1.gnif"
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/equipment-default-0-high-v1.gnif.json"
            "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/equipment-default-0-high-v1.gnif.json")
    foreach(_weapon_id IN ITEMS knife grenade sidearm carbine rifle marksman_rifle support_gun heavy_support_gun)
        list(APPEND _genomes_infantry_fixture_commands
            COMMAND "${GENOMES_NODE_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/reference/export_infantry_reference.cjs"
                --mode weapon --seed 0 --detail high --equipment neutral
                --weapon "${_weapon_id}"
                --output "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/weapon-${_weapon_id}-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/weapon-${_weapon_id}-v1.gnif"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/weapon-${_weapon_id}-v1.gnif"
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${GENOMES_INFANTRY_JS_FIXTURE_DIR}/weapon-${_weapon_id}-v1.gnif.json"
                "${CMAKE_SOURCE_DIR}/reference/fixtures/infantry/weapon-${_weapon_id}-v1.gnif.json")
    endforeach()
    add_custom_target(genomes_generate_infantry_js_reference
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${GENOMES_INFANTRY_JS_FIXTURE_DIR}"
        ${_genomes_infantry_fixture_commands}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Regenerating pinned JavaScript infantry parity fixture matrix"
        VERBATIM)
    unset(_genomes_infantry_fixture_commands)
endif()

if(GENOMES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/native)
    add_subdirectory(tests/threepp)
endif()

if(GENOMES_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
