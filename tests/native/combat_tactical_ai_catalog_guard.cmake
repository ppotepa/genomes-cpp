cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(header "${GENOMES_SOURCE_DIR}/modules/combat/include/genomes/combat/TacticalAI.hpp")
set(source "${GENOMES_SOURCE_DIR}/modules/combat/src/TacticalAIProfile.cpp")
set(test "${GENOMES_SOURCE_DIR}/tests/native/tactical_ai_tests.cpp")
set(profile "${GENOMES_SOURCE_DIR}/mods/core/profiles/tactical-ai.json")
set(parity_fixture "${GENOMES_SOURCE_DIR}/reference/fixtures/combat_tactical_ai_profiles.json")
foreach(required IN ITEMS "${header}" "${source}" "${test}" "${profile}" "${parity_fixture}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing tactical AI catalog source: ${required}")
    endif()
endforeach()

file(READ "${header}" header_text)
file(READ "${source}" source_text)
file(READ "${test}" test_text)
file(READ "${profile}" profile_text)
file(READ "${parity_fixture}" parity_text)

foreach(required IN ITEMS "TacticalAIProfileSnapshot" "FrozenContentSnapshot"
                           "SimConfigHash" "loadTacticalAIProfile" "fingerprint")
    string(FIND "${header_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Tactical AI profile contract misses ${required}")
    endif()
endforeach()

# The profile loader is the domain owner of the schema.  Keep this source
# contract strict: it must use bounded content reads, reject unknown/missing
# fields, validate the resolved typed values, freeze provenance, and derive a
# typed simulation fingerprint from canonical values.
foreach(required IN ITEMS "readContentText" "ContentSnapshotBuilder"
                           "unknown or missing tactical AI profile fields"
                           "invalid tactical AI profile values" "snapshot_builder).freeze"
                           "makeSimConfigHash" "combat.tactical-ai.v1")
    string(FIND "${source_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Tactical AI profile loader misses ${required}")
    endif()
endforeach()

foreach(required IN ITEMS "tactical-ai-default" "schema_version" "observation_period_ticks"
                           "memory_ticks" "target_switch_ratio" "fire_alignment_cos")
    string(FIND "${profile_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Tactical AI profile data misses ${required}")
    endif()
endforeach()

# The native profile must retain the values exported from the reference AI
# pipeline.  This guard intentionally checks parity landmarks rather than
# introducing a second tuning source.
foreach(required IN ITEMS "simple_combat_default" "integer_tick_stable_phase"
                           "target_order")
    string(FIND "${parity_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Tactical AI parity fixture misses ${required}")
    endif()
endforeach()
foreach(required_value IN ITEMS
        "observation_period_ticks[^0-9]+12"
        "memory_ticks[^0-9]+150"
        "target_switch_ratio[^0-9]+0\\.85"
        "fire_alignment_degrees[^0-9]+4\\.0")
    if(NOT parity_text MATCHES "${required_value}")
        message(FATAL_ERROR "Tactical AI parity fixture misses ${required_value}")
    endif()
endforeach()

foreach(required IN ITEMS "loadTacticalAIProfile" "content.sources.size()"
                           "content.fingerprint" "fingerprint.value"
                           "profile.target_switch_ratio" "profile.fire_alignment_cos")
    string(FIND "${test_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Tactical AI catalog test misses ${required}")
    endif()
endforeach()

message(STATUS "Tactical AI catalog source contract inspected")
