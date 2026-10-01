cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(header "${GENOMES_SOURCE_DIR}/modules/weapons/include/genomes/weapons/WeaponCatalog.hpp")
set(source "${GENOMES_SOURCE_DIR}/modules/weapons/src/WeaponCatalog.cpp")
set(test "${GENOMES_SOURCE_DIR}/tests/native/weapon_catalog_geometry_tests.cpp")
set(fixture "${GENOMES_SOURCE_DIR}/reference/fixtures/weapons_catalog_profiles.json")
foreach(required IN ITEMS "${header}" "${source}" "${test}" "${fixture}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing weapon catalog source: ${required}")
    endif()
endforeach()

file(READ "${header}" header_text)
file(READ "${source}" source_text)
file(READ "${test}" test_text)
file(READ "${fixture}" fixture_text)

foreach(required IN ITEMS "FrozenWeaponCatalog" "FrozenContentSnapshot" "SimConfigHash"
                           "sourceCommit()" "loadWeaponCatalog" "contentSnapshot()")
    string(FIND "${header_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Weapon catalog header misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "readContentText" "ContentSnapshotBuilder" "unknown weapon catalog"
                           "weapon catalog firearm parity mismatch" "result.frozen_ = true"
                           "weaponFingerprint")
    string(FIND "${source_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Weapon catalog loader misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "loadWeaponCatalog" "contentSnapshot()" "fingerprint()"
                           "sourceCommit()" "loaded.size() == 8U")
    string(FIND "${test_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Weapon catalog test misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "weapons-catalog-fixtures-1" "carbine" "heavy_support_gun"
                           "stable-id" "firearm-ammunition" "attachment-landmarks")
    string(FIND "${fixture_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Weapon catalog fixture misses ${required}")
    endif()
endforeach()

message(STATUS "Weapon catalog source contract inspected")
