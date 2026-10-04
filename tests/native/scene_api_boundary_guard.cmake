if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE SCENE_FILES
    "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/*.hpp"
    "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/*.cpp")
set(FORBIDDEN_SCENE_TOKENS
    "std::thread"
    "std::jthread"
    "parallelForAndWait"
    "WorldEcs"
    "EntityStore"
    "AnimationSystem"
    "SystemExecutionPlan"
    "InfantryMassBattleRuntime"
    "battlefield_runtime_->advance"
    "battlefield_runtime_->fixedUpdate"
    "battlefield_runtime_->submit"
    "battlefield_runtime_->query"
    "mass_battle_runtime_->advance"
    "mass_battle_runtime_->fixedUpdate"
    "mass_battle_runtime_->submit"
    "mass_battle_runtime_->query")
foreach(FILE_PATH IN LISTS SCENE_FILES)
    file(READ "${FILE_PATH}" CONTENT)
    foreach(TOKEN IN LISTS FORBIDDEN_SCENE_TOKENS)
        string(FIND "${CONTENT}" "${TOKEN}" POSITION)
        if(NOT POSITION EQUAL -1)
            message(FATAL_ERROR "scene API boundary violation: ${TOKEN} in ${FILE_PATH}")
        endif()
    endforeach()
endforeach()
