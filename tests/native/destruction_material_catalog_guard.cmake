cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(header "${GENOMES_SOURCE_DIR}/modules/destruction/include/genomes/destruction/Material.hpp")
set(source "${GENOMES_SOURCE_DIR}/modules/destruction/src/MaterialAssembly.cpp")
set(test "${GENOMES_SOURCE_DIR}/tests/native/destruction/material_solid_tests.cpp")
set(fixture "${GENOMES_SOURCE_DIR}/reference/fixtures/destruction/material_catalog_v1.json")
foreach(required IN ITEMS "${header}" "${source}" "${test}" "${fixture}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing destruction material catalog source: ${required}")
    endif()
endforeach()

file(READ "${header}" header_text)
file(READ "${source}" source_text)
file(READ "${test}" test_text)
file(READ "${fixture}" fixture_text)

foreach(required IN ITEMS "FrozenContentSnapshot" "SimConfigHash" "contentSnapshot()"
                           "fingerprint()" "static MaterialCatalog" "load(")
    string(FIND "${header_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Destruction material catalog header misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "readContentText" "ContentSnapshotBuilder" "materialFingerprint"
                           "catalog.freeze()" "invalid material catalog entry fields"
                           "catalog.fingerprint_ = materialFingerprint")
    string(FIND "${source_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Destruction material catalog loader misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "MaterialCatalog::load" "contentSnapshot()" "fingerprint()"
                           "density_kg_m3" "penetration_work_j_m3")
    string(FIND "${test_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Destruction material catalog test misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "genomes.destruction.material-catalog.v1" "destruction.materials"
                           "concrete" "penetration_work_j_m3" "response")
    string(FIND "${fixture_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Destruction material catalog fixture misses ${required}")
    endif()
endforeach()

message(STATUS "Destruction material catalog source contract inspected")
