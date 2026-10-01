if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(game_header "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.hpp")
set(game_source "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.cpp")
set(scene_source "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BattlefieldScene.cpp")
set(director_header "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/SceneDirector.hpp")
set(director_source "${GENOMES_SOURCE_DIR}/engine/runtime/src/SceneDirector.cpp")
set(clock_header "${GENOMES_SOURCE_DIR}/engine/simulation/include/genomes/simulation/SessionSimulationClock.hpp")

foreach(file IN ITEMS game_header game_source scene_source director_header director_source clock_header)
    if(NOT EXISTS "${${file}}")
        message(FATAL_ERROR "Missing session clock source: ${${file}}")
    endif()
    file(READ "${${file}}" ${file}_text)
endforeach()

foreach(required_text IN ITEMS
        "simulation::SessionSimulationClock clock_;"
        "director_.fixed_update(context);"
        "fixed_update(const simulation::TickContext& context)"
        "current_->fixed_update(scene_context, tick_context);"
        "callback(TickContext{tick, fixed_dt, SessionSimulationTickRateHz});")
    set(found FALSE)
    foreach(file IN ITEMS game_header game_source scene_source director_header director_source clock_header)
        string(FIND "${${file}_text}" "${required_text}" position)
        if(NOT position EQUAL -1)
            set(found TRUE)
        endif()
    endforeach()
    if(NOT found)
        message(FATAL_ERROR "SessionSimulationClock contract is missing: ${required_text}")
    endif()
endforeach()

if(game_header_text MATCHES "FixedStepClock clock_")
    message(FATAL_ERROR "GameApplication must not own a second FixedStepClock")
endif()
if(game_source_text MATCHES "director_\\.fixed_update\\(dt\\)")
    message(FATAL_ERROR "GameApplication must pass the full TickContext")
endif()
if(scene_source_text MATCHES "simulation_tick_")
    message(FATAL_ERROR "BattlefieldScene must not own a second simulation tick")
endif()

message(STATUS "SessionSimulationClock source contract inspected")
