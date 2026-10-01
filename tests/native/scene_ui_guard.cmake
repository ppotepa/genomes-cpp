cmake_minimum_required(VERSION 3.21)
if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE _sources
    "${GENOMES_SOURCE_DIR}/apps/*.cpp"
    "${GENOMES_SOURCE_DIR}/apps/*.hpp"
    "${GENOMES_SOURCE_DIR}/engine/runtime/*.cpp"
    "${GENOMES_SOURCE_DIR}/engine/runtime/*.hpp"
    "${GENOMES_SOURCE_DIR}/engine/ui/*.cpp"
    "${GENOMES_SOURCE_DIR}/engine/ui/*.hpp")
foreach(_source IN LISTS _sources)
    file(READ "${_source}" _text)
    if(_text MATCHES "context[.]ui[.]add|UiWidget|frame[.]widgets|ui[.]widgets|document_for_scene|set_builtin_text")
        message(FATAL_ERROR "legacy scene UI boundary in ${_source}")
    endif()
endforeach()

file(GLOB_RECURSE _documents "${GENOMES_SOURCE_DIR}/mods/core/scenes/*.rml")
foreach(_document IN LISTS _documents)
    file(READ "${_document}" _rml)
    string(FIND "${_rml}" "href=\"ui/theme.rcss\"" _theme)
    string(FIND "${_rml}" "href=\"screen.rcss\"" _scene)
    if(_theme LESS 0 OR _scene LESS 0 OR _theme GREATER _scene)
        message(FATAL_ERROR "shared theme must precede scene RCSS in ${_document}")
    endif()
    if(_rml MATCHES "type=\"(number|range)\"[^>]*data-control=\"[^\"]+\"" AND
       NOT _rml MATCHES "type=\"(number|range)\"[^>]*data-field=\"[^\"]+\"[^>]*data-control=")
        message(FATAL_ERROR "bound number/range requires explicit data-field in ${_document}")
    endif()
endforeach()

file(READ "${GENOMES_SOURCE_DIR}/mods/core/ui/theme.rcss" _theme_css)
foreach(_required IN ITEMS "selectvalue" "selectarrow" "selectbox" "sliderprogress"
                           "option:hover" "option:checked" "option:disabled"
                           ".field" ".range-row" ".check-row" ".tabs" ".value-chip")
    string(FIND "${_theme_css}" "${_required}" _found)
    if(_found LESS 0)
        message(FATAL_ERROR "shared theme misses ${_required}")
    endif()
endforeach()

file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/unit-lab/screen.rml" _unit_rml)
foreach(_required IN ITEMS "scene.open-building-lab" "scene.open-world-lab" "disabled=\"disabled\""
                           "Crouch Walk" "max=\"1.75\"" "unit.genome-clear")
    string(FIND "${_unit_rml}" "${_required}" _found)
    if(_found LESS 0)
        message(FATAL_ERROR "Unit Lab misses ${_required}")
    endif()
endforeach()

# T11/T28: the Unit Lab's RmlUi controls must have one typed-command adapter.
# The document remains a string boundary, but scene code must not dispatch a
# control by independently parsing its value or rebuilding geometry here.
file(READ "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/UnitLabCommandParsing.hpp" _unit_commands)
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/UnitLabScene.cpp" _unit_scene)
foreach(_control IN ITEMS "unit.variation" "unit.camera" "unit.locomotion"
                          "unit.expression" "unit.equipment-item" "unit.genome")
    string(FIND "${_unit_rml}" "data-control=\"${_control}\"" _rml_control)
    if(_rml_control LESS 0)
        message(FATAL_ERROR "Unit Lab RmlUi control missing: ${_control}")
    endif()
    string(FIND "${_unit_commands}" "control == \"${_control}\"" _typed_control)
    if(_typed_control LESS 0)
        message(FATAL_ERROR "Unit Lab typed RmlUi adapter missing: ${_control}")
    endif()
endforeach()
string(FIND "${_unit_scene}" "parseUnitLabRmlCommand" _rml_adapter_use)
if(_rml_adapter_use LESS 0)
    message(FATAL_ERROR "Unit Lab scene does not use the RmlUi typed-command adapter")
endif()
string(FIND "${_unit_scene}" "parseUnitLabCommand(" _legacy_parser_use)
if(NOT _legacy_parser_use LESS 0)
    message(FATAL_ERROR "Unit Lab scene still parses RmlUi values through the CLI adapter")
endif()

if(NOT EXISTS "${GENOMES_SOURCE_DIR}/engine/ui/include/genomes/ui/UiDataModel.hpp" OR
   NOT EXISTS "${GENOMES_SOURCE_DIR}/engine/ui/include/genomes/ui/UiEvent.hpp" OR
   NOT EXISTS "${GENOMES_SOURCE_DIR}/engine/ui/include/genomes/ui/UiServices.hpp")
    message(FATAL_ERROR "neutral UI MVVM contracts are missing")
endif()

file(GLOB_RECURSE _styles "${GENOMES_SOURCE_DIR}/mods/core/*.rcss")
foreach(_style IN LISTS _styles)
    file(READ "${_style}" _css)
    if(_css MATCHES "box-shadow[ \t]*:" OR
       _css MATCHES "(^|[;{])[ \t]*(filter|backdrop-filter|mask-image|transform)[ \t]*:" OR
       _css MATCHES "border(-top|-right|-bottom|-left)?[ \t]*:[^;{}]*solid")
        message(FATAL_ERROR "unsupported RmlUi effect or border shorthand in ${_style}")
    endif()
endforeach()

file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/main-menu/screen.rml" _menu_rml)
if(NOT _menu_rml MATCHES "autofocus")
    message(FATAL_ERROR "main menu must autofocus Battlefield")
endif()
if(_menu_rml MATCHES "World configuration" OR _menu_rml MATCHES "Native D3D12")
    message(FATAL_ERROR "main menu contains a removed competing entry/status")
endif()
file(READ "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/MainMenuScene.hpp" _menu_header)
if(_menu_header MATCHES "MainMenuEntry::WorldConfig|WorldConfig,")
    message(FATAL_ERROR "MainMenuEntry still exposes a separate WorldConfig item")
endif()
