cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(_runtime_dir "${GENOMES_SOURCE_DIR}/engine/runtime")
set(_game_scenes_dir "${GENOMES_SOURCE_DIR}/engine/game_scenes")

file(READ "${_game_scenes_dir}/include/genomes/game_scenes/ApplicationCommand.hpp"
     _application_command)
file(READ "${_runtime_dir}/include/genomes/runtime/Scene.hpp" _scene)
file(READ "${_runtime_dir}/include/genomes/runtime/SceneDirector.hpp" _director)
file(READ "${_runtime_dir}/include/genomes/runtime/ApplicationCommand.hpp"
     _compatibility)
file(READ "${_game_scenes_dir}/include/genomes/game_scenes/WorldConfig.hpp"
     _world_config)
file(READ "${_runtime_dir}/CMakeLists.txt" _runtime_cmake)

if(NOT _application_command MATCHES "namespace genomes::application")
    message(FATAL_ERROR "ApplicationCommand must be owned by genomes::application")
endif()
if(NOT _application_command MATCHES "struct ApplicationCommand")
    message(FATAL_ERROR "application command definition is missing")
endif()
if(NOT _application_command MATCHES "runtime::SceneCommand")
    message(FATAL_ERROR "application command must use the neutral SceneCommand boundary")
endif()
if(_scene MATCHES "#include[ \t]*<genomes/runtime/ApplicationCommand.hpp>" OR
   _scene MATCHES "ApplicationCommandKind")
    message(FATAL_ERROR "neutral Scene.hpp must not depend on product command types")
endif()
if(_director MATCHES "ApplicationCommand" OR
   _director MATCHES "application_command_handler")
    message(FATAL_ERROR "neutral SceneDirector must not name application command payloads")
endif()
if(NOT _compatibility MATCHES "Transitional compatibility include")
    message(FATAL_ERROR "legacy runtime command include must be an explicit compatibility bridge")
endif()
if(NOT _world_config MATCHES "namespace genomes::application")
    message(FATAL_ERROR "WorldGenerationConfig must be owned by the application scene layer")
endif()
if(_scene MATCHES "WorldGenerationConfig|WorldSeedInput|genomes/runtime/WorldConfig")
    message(FATAL_ERROR "neutral Scene.hpp must not depend on world-generation configuration")
endif()
if(_director MATCHES "WorldGenerationConfig|WorldSeedInput|genomes/runtime/WorldConfig")
    message(FATAL_ERROR "neutral SceneDirector must not depend on world-generation configuration")
endif()
if(_runtime_cmake MATCHES "world/include|proc/include|hydrology/include|roads/include")
    message(FATAL_ERROR "runtime_core must not export world-generation include directories")
endif()

foreach(_source
        BattlefieldScene.cpp
        BuildingLabScene.cpp
        BuiltinScenes.cpp
        MainMenuScene.cpp
        UnitLabScene.cpp
        WorldConfigScene.cpp
        WorldLabScene.cpp)
    file(READ "${_game_scenes_dir}/src/${_source}" _scene_source)
    if(NOT _scene_source MATCHES "genomes/game_scenes/ApplicationCommand.hpp")
        message(FATAL_ERROR "${_source} does not include the application command contract")
    endif()
    if(_scene_source MATCHES "commands[.]push[ \t]*\\(")
        message(FATAL_ERROR "${_source} still pushes an untyped aggregate command")
    endif()
endforeach()

message(STATUS "Application command and R034 world-config boundary guard passed")
