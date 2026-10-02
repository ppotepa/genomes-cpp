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
       NOT _rml MATCHES "type=\"(number|range)\"[^>]*data-(field|attr-data-field)=\"[^\"]+\"[^>]*data-control=")
        message(FATAL_ERROR "bound number/range requires explicit data-field in ${_document}")
    endif()
    if(_rml MATCHES "data-bind=\"text:")
        message(FATAL_ERROR "text bindings must use safe {{ value }} interpolation in ${_document}")
    endif()
    if(NOT _rml MATCHES "[\{][\{][ \\t]*fps[ \\t]*[\}][\}]")
        message(FATAL_ERROR "Route header must expose the shared FPS readout: ${_document}")
    endif()
endforeach()

file(GLOB_RECURSE _production_rcss
    "${GENOMES_SOURCE_DIR}/mods/core/ui/*.rcss"
    "${GENOMES_SOURCE_DIR}/mods/core/scenes/*.rcss")
list(APPEND _production_rcss "${GENOMES_SOURCE_DIR}/mods/core/ui-test.rcss")
foreach(_stylesheet IN LISTS _production_rcss)
    file(READ "${_stylesheet}" _stylesheet_text)
    if(_stylesheet_text MATCHES "border-radius")
        message(FATAL_ERROR "production UI must keep square corners: ${_stylesheet}")
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
foreach(_dead_theme_rule IN ITEMS "button.toggle" ".scroll-panel")
    if(_theme_css MATCHES "${_dead_theme_rule}")
        message(FATAL_ERROR "shared theme retains removed control rule: ${_dead_theme_rule}")
    endif()
endforeach()

file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/unit-lab/screen.rml" _unit_rml)
file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/unit-lab/screen.rcss" _unit_css)
file(READ "${GENOMES_SOURCE_DIR}/mods/core/ui-test.rcss" _ui_test_css)
foreach(_dead_unit_rule IN ITEMS ".unit-field" ".toggle")
    if(_unit_css MATCHES "${_dead_unit_rule}")
        message(FATAL_ERROR "Unit Lab retains removed control rule: ${_dead_unit_rule}")
    endif()
endforeach()
if(_ui_test_css MATCHES "[.]scroll-panel")
    message(FATAL_ERROR "UI gallery retains removed scroll-panel rule")
endif()
file(GLOB_RECURSE _scene_rml "${GENOMES_SOURCE_DIR}/mods/core/scenes/*.rml")
foreach(_document IN LISTS _scene_rml)
    file(READ "${_document}" _document_text)
    if(_document_text MATCHES "data-attr-(selected|disabled)=")
        message(FATAL_ERROR "Boolean RmlUi attributes must use data-attrif-* in ${_document}")
    endif()
    string(REGEX MATCHALL "<main([ >])" _main_elements "${_document_text}")
    list(LENGTH _main_elements _main_count)
    if(_main_count LESS 1)
        message(FATAL_ERROR "Route is missing its main landmark: ${_document}")
    elseif(_main_count GREATER 1)
        message(FATAL_ERROR "Route has more than one main landmark: ${_document}")
    endif()
    string(REGEX MATCHALL "<summary([ >])" _summary_elements "${_document_text}")
    string(REGEX MATCHALL "<summary([ >])[^<]*<button[^>]*data-disclosure[= ][^>]*>[^<]*</button>[^<]*</summary>"
           _disclosure_buttons "${_document_text}")
    list(LENGTH _summary_elements _summary_count)
    list(LENGTH _disclosure_buttons _disclosure_count)
    if(_summary_count GREATER _disclosure_count)
        message(FATAL_ERROR "Disclosure summary must own a data-disclosure button: ${_document}")
    endif()
    string(REGEX MATCHALL "<option[^>]*>" _document_options "${_document_text}")
    foreach(_option IN LISTS _document_options)
        if(NOT _option MATCHES "(^|[ ])value=|data-attr-value=")
            message(FATAL_ERROR "Route option must expose an explicit value: ${_document}: ${_option}")
        endif()
    endforeach()
endforeach()
if(EXISTS "${GENOMES_SOURCE_DIR}/mods/core/ui/controls.rml")
    message(FATAL_ERROR "Unused controls.rml must not be restored; ui-test.rml is the gallery")
endif()
foreach(_required IN ITEMS "scene.open-building-lab" "scene.open-world-lab" "disabled=\"disabled\""
                           "Crouch Walk" "max=\"1.75\"" "max=\"0.99\"" "unit.genome-clear")
    string(FIND "${_unit_rml}" "${_required}" _found)
    if(_found LESS 0)
        message(FATAL_ERROR "Unit Lab misses ${_required}")
    endif()
endforeach()
string(REGEX MATCHALL "<option[^>]*>" _unit_options "${_unit_rml}")
foreach(_option IN LISTS _unit_options)
    if(NOT _option MATCHES "(^|[ ])value=|data-attr-value=")
        message(FATAL_ERROR "Unit Lab option must expose an explicit value: ${_option}")
    endif()
endforeach()

# Infantry Lab HUD contract: one scroll owner, semantic landmarks, foldable
# domains, and the form classes used by the local stylesheet.
foreach(_landmark IN ITEMS "id=\"unit-header\"" "id=\"unit-rail\""
                           "id=\"unit-toolbar\"" "id=\"unit-viewport\""
                           "id=\"unit-inspector\"" "class=\"unit-inspector-scroll\"")
    string(FIND "${_unit_rml}" "${_landmark}" _landmark_found)
    if(_landmark_found LESS 0)
        message(FATAL_ERROR "Unit Lab landmark missing: ${_landmark}")
    endif()
endforeach()
foreach(_semantic IN ITEMS "<header" "<nav" "<main" "<aside" "<section"
                          "<details" "<summary" "class=\"field\""
                          "class=\"check-row\"" "<output")
    string(FIND "${_unit_rml}" "${_semantic}" _semantic_found)
    if(_semantic_found LESS 0)
        message(FATAL_ERROR "Unit Lab semantic/form contract missing: ${_semantic}")
    endif()
endforeach()
foreach(_scroll_contract IN ITEMS "display: block" "flex: 1" "min-height: 0dp"
                                  "overflow-y: auto" "pointer-events: auto"
                                  "scrollbarvertical")
    string(FIND "${_unit_css}" "${_scroll_contract}" _scroll_found)
    if(_scroll_found LESS 0)
        message(FATAL_ERROR "Unit Lab scroll contract missing: ${_scroll_contract}")
    endif()
endforeach()
if(_unit_css MATCHES "[.]unit-inspector-scroll[^}]*overflow-y:[^}]*overflow-y:")
    message(FATAL_ERROR "Unit Lab declares multiple overflow-y owners on the main scroll container")
endif()
if(NOT _unit_css MATCHES "[.]unit-viewport[^}]*background-color:[ ]*transparent")
    message(FATAL_ERROR "Unit Lab viewport must stay transparent so RmlUi does not cover the GPU scene")
endif()
foreach(_foldable IN ITEMS "Rendering and debug" "Apparel, armor, attachments and weapons"
                           "Root, body and face genes" "Rig hierarchy" "Diagnostics")
    string(FIND "${_unit_rml}" "${_foldable}" _foldable_found)
    if(_foldable_found LESS 0)
        message(FATAL_ERROR "Unit Lab foldable section missing: ${_foldable}")
    endif()
endforeach()
foreach(_control IN ITEMS "unit.seed" "unit.side" "unit.tab"
                          "unit.detail" "unit.camera" "unit.equipment-item"
                          "unit.variation" "unit.genome" "unit.weight"
                          "unit.locomotion" "unit.animation-speed" "unit.animation-transition" "unit.phase"
                          "unit.expression" "unit.expression-intensity")
    string(FIND "${_unit_rml}" "data-control=\"${_control}\"" _contract_found)
    if(_contract_found LESS 0)
        message(FATAL_ERROR "Unit Lab data-control contract missing: ${_control}")
    endif()
endforeach()

string(FIND "${_unit_rml}" "data-action=\"unit.regenerate\"" _regenerate_action)
if(_regenerate_action LESS 0)
    message(FATAL_ERROR "Unit Lab quick regenerate action missing")
endif()

# Building Lab keeps amount selection separate from the destructive operation:
# Apply Damage must read the authoritative UI draft before mutating the runtime.
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BuildingLabScene.cpp" _building_scene)
string(FIND "${_building_scene}" "find_field(\"damage_amount\")" _building_damage_draft)
if(_building_damage_draft LESS 0)
    message(FATAL_ERROR "Building Lab Apply Damage must commit the damage UI draft")
endif()

foreach(_action IN ITEMS "unit.back" "unit.regenerate" "unit.camera-reset"
                         "unit.equipment-clear" "unit.genome-reset" "unit.pause"
                         "unit.animation-reset" "unit.animation-step-back"
                         "unit.animation-step-forward")
    string(FIND "${_unit_rml}" "data-action=\"${_action}\"" _action_found)
    if(_action_found LESS 0)
        message(FATAL_ERROR "Unit Lab data-action contract missing: ${_action}")
    endif()
endforeach()

# T28: UI density is a bounded presentation setting. Keep the RmlUi range and
# the application-side clamp aligned at the supported 75/100/150% endpoints.
file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/settings/screen.rml" _settings_rml)
foreach(_scale_contract IN ITEMS "min=\"0.75\"" "max=\"1.5\"" "step=\"0.05\"")
    string(FIND "${_settings_rml}" "${_scale_contract}" _scale_contract_found)
    if(_scale_contract_found LESS 0)
        message(FATAL_ERROR "settings RmlUi scale control misses ${_scale_contract}")
    endif()
endforeach()
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BuiltinScenes.cpp" _builtin_scenes)
string(FIND "${_builtin_scenes}" "std::clamp" _scale_clamp)
string(FIND "${_builtin_scenes}" "0.75, 1.50" _scale_clamp_range)
if(_scale_clamp LESS 0 OR _scale_clamp_range LESS 0)
    message(FATAL_ERROR "application UI scale clamp is not bounded to 0.75..1.50")
endif()

# T11/T28: the Unit Lab's RmlUi controls must have one typed-command adapter.
# The document remains a string boundary, but scene code must not dispatch a
# control by independently parsing its value or rebuilding geometry here.
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/UnitLabCommandParsing.hpp" _unit_commands)
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/UnitLabScene.cpp" _unit_scene)
string(REGEX MATCHALL "data-control=\"unit[.][^\"]+\"" _unit_control_attributes "${_unit_rml}")
foreach(_unit_control_attribute IN LISTS _unit_control_attributes)
    string(REGEX REPLACE "data-control=\"([^\"]+)\"" "\\1"
           _unit_control "${_unit_control_attribute}")
    string(FIND "${_unit_scene}" "stable_id(\"${_unit_control}\")" _unit_handler)
    if(_unit_handler LESS 0)
        message(FATAL_ERROR "Unit Lab data-control has no scene handler: ${_unit_control}")
    endif()
endforeach()
foreach(_control IN ITEMS "unit.variation" "unit.camera" "unit.locomotion"
                          "unit.expression" "unit.equipment-item" "unit.genome"
                          "unit.appearance-preset")
    string(FIND "${_unit_rml}" "data-control=\"${_control}\"" _rml_control)
    if(_rml_control LESS 0)
        message(FATAL_ERROR "Unit Lab RmlUi control missing: ${_control}")
    endif()
    string(FIND "${_unit_commands}" "control == \"${_control}\"" _typed_control)
    if(_typed_control LESS 0)
        message(FATAL_ERROR "Unit Lab typed RmlUi adapter missing: ${_control}")
    endif()
endforeach()
foreach(_scene_adapter IN ITEMS "unit.variation" "unit.camera" "unit.locomotion"
                                "unit.expression" "unit.equipment-item" "unit.genome"
                                "unit.appearance-preset")
    string(REGEX MATCH
        "parseUnitLabRmlCommand[ \t\r\n]*\\([ \t\r\n]*\"${_scene_adapter}\""
        _scene_adapter_match "${_unit_scene}")
    if(NOT _scene_adapter_match)
        message(FATAL_ERROR "Unit Lab scene does not route ${_scene_adapter} through the RmlUi typed-command adapter")
    endif()
endforeach()
string(FIND "${_unit_scene}" "parseUnitLabCommand(" _legacy_parser_use)
if(NOT _legacy_parser_use LESS 0)
    message(FATAL_ERROR "Unit Lab scene still parses RmlUi values through the CLI adapter")
endif()

# Every semantic control declared by a route must have a scene-side handler.
# This catches controls that render correctly but silently disappear at the
# runtime boundary because only their RML markup was added.
set(_route_handler_pairs
    "unit-lab|UnitLabScene.cpp"
    "world-lab|WorldLabScene.cpp"
    "world-config|WorldConfigScene.cpp"
    "building-lab|BuildingLabScene.cpp"
    "settings|BuiltinScenes.cpp")
foreach(_route_handler_pair IN LISTS _route_handler_pairs)
    string(REPLACE "|" ";" _route_handler_parts "${_route_handler_pair}")
    list(GET _route_handler_parts 0 _route_name)
    list(GET _route_handler_parts 1 _handler_name)
    file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/${_route_name}/screen.rml" _route_text)
    file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/${_handler_name}" _handler_text)
    string(REGEX MATCHALL "data-control=\"[^\"]+\"" _route_controls "${_route_text}")
    foreach(_route_control_attribute IN LISTS _route_controls)
        string(REGEX REPLACE "data-control=\"([^\"]+)\"" "\\1"
               _route_control "${_route_control_attribute}")
        string(FIND "${_handler_text}" "stable_id(\"${_route_control}\")" _route_handler)
        if(_route_handler LESS 0)
            message(FATAL_ERROR "Route data-control has no scene handler: ${_route_name}: ${_route_control}")
        endif()
    endforeach()
endforeach()

# The same contract applies to click actions.  Their dispatch may be handled
# by a scene, the built-in scene router, or the application command boundary,
# so search the complete production action surface.
file(GLOB_RECURSE _action_handlers
    "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/*.cpp"
    "${GENOMES_SOURCE_DIR}/apps/game/*.cpp")
set(_action_handler_text "")
foreach(_action_handler IN LISTS _action_handlers)
    file(READ "${_action_handler}" _action_handler_source)
    string(APPEND _action_handler_text "\n${_action_handler_source}")
endforeach()
file(GLOB_RECURSE _action_documents "${GENOMES_SOURCE_DIR}/mods/core/scenes/*.rml")
list(APPEND _action_documents "${GENOMES_SOURCE_DIR}/mods/core/ui-test.rml")
foreach(_action_document IN LISTS _action_documents)
    file(READ "${_action_document}" _action_document_text)
    string(REGEX MATCHALL "data-action=\"[^\"]+\"" _action_attributes
           "${_action_document_text}")
    foreach(_action_attribute IN LISTS _action_attributes)
        string(REGEX REPLACE "data-action=\"([^\"]+)\"" "\\1"
               _action_name "${_action_attribute}")
        string(FIND "${_action_handler_text}" "stable_id(\"${_action_name}\")" _action_handler)
        if(_action_handler LESS 0)
            message(FATAL_ERROR "RML data-action has no production handler: ${_action_name}")
        endif()
    endforeach()
endforeach()

# The application CLI must validate every Unit Lab command through the same
# typed parser before dispatching a UI action. Keep this guard structural: it
# does not claim that a built executable or CTest has run.
file(READ "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.cpp" _game_application)
foreach(_cli_option IN ITEMS "--unitlab-variation" "--unitlab-camera"
                             "--unitlab-locomotion" "--unitlab-expression"
                             "--unitlab-equipment" "--unitlab-gene"
                             "--unitlab-appearance")
    string(FIND "${_game_application}" "${_cli_option}" _cli_option_found)
    if(_cli_option_found LESS 0)
        message(FATAL_ERROR "Unit Lab CLI option missing: ${_cli_option}")
    endif()
endforeach()
string(FIND "${_game_application}" "parseUnitLabCommandLine" _cli_parser_use)
if(_cli_parser_use LESS 0)
    message(FATAL_ERROR "Unit Lab CLI does not use the shared typed-command parser")
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
file(READ "${GENOMES_SOURCE_DIR}/mods/core/mod.json" _core_mod_json)
if(NOT _core_mod_json MATCHES "scenes/infantry-mass-battle")
    message(FATAL_ERROR "core mod manifest must register Infantry Mass Battle")
endif()
file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/infantry-mass-battle/scene.json" _mass_battle_scene)
file(READ "${GENOMES_SOURCE_DIR}/mods/core/scenes/infantry-mass-battle/screen.rml" _mass_battle_rml)
foreach(_mass_battle_contract IN ITEMS "scene.infantry-mass-battle" "builtin.battlefield"
                                       "screen.rml" "screen.rcss" "scene.pause")
    string(FIND "${_mass_battle_scene}" "${_mass_battle_contract}" _mass_battle_found)
    if(_mass_battle_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle manifest misses ${_mass_battle_contract}")
    endif()
endforeach()
foreach(_loading_contract IN ITEMS "scene-loading" "scene_loading_active"
                                   "scene_loading_progress" "scene_loading_message"
                                   "scene_loading_failed")
    string(FIND "${_mass_battle_rml}" "${_loading_contract}" _loading_contract_found)
    if(_loading_contract_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle loading UI misses ${_loading_contract}")
    endif()
endforeach()
string(FIND "${_mass_battle_rml}" "<p class=\"controls\">{{ description }}</p>"
       _mass_battle_controls)
if(_mass_battle_controls LESS 0)
    message(FATAL_ERROR "Infantry Mass Battle HUD must expose the RTS controls")
endif()
foreach(_mass_profile_ui_contract IN ITEMS "{{ profile }}" "{{ animation_mode }}"
                                              "mass-battle.profile-quality"
                                              "mass-battle.profile-balanced"
                                              "mass-battle.profile-stress")
    string(FIND "${_mass_battle_rml}" "${_mass_profile_ui_contract}" _mass_profile_found)
    if(_mass_profile_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle HUD misses ${_mass_profile_ui_contract}")
    endif()
endforeach()
file(READ "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/Scene.hpp" _scene_contract)
foreach(_loading_contract IN ITEMS "SceneLoadingPhase" "Starting" "InProgress"
                                   "Completed" "Failed" "loading_status")
    string(FIND "${_scene_contract}" "${_loading_contract}" _loading_contract_found)
    if(_loading_contract_found LESS 0)
        message(FATAL_ERROR "Scene loading contract misses ${_loading_contract}")
    endif()
endforeach()
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BattlefieldScene.cpp" _battlefield_scene)
string(FIND "${_battlefield_scene}" "if (config.seed == 0U) config.seed = 0x1F4A77U"
       _mass_battle_seed_resolution)
if(_mass_battle_seed_resolution LESS 0)
    message(FATAL_ERROR "Infantry Mass Battle must resolve the automatic zero seed")
endif()
foreach(_mass_loading_stage IN ITEMS "MassBattleLoadStage::Starting"
                                     "MassBattleLoadStage::CreateSimulation"
                                     "MassBattleLoadStage::CompileModel"
                                     "MassBattleLoadStage::Ready")
    string(FIND "${_battlefield_scene}" "${_mass_loading_stage}" _mass_loading_stage_found)
    if(_mass_loading_stage_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle staged loading misses ${_mass_loading_stage}")
    endif()
endforeach()
foreach(_mass_instancing_contract IN ITEMS "mesh.infantry.mass-battle.pose-atlas"
                                          "mass_battle_atlas"
                                          "MassBattlePresentationProfile"
                                          "mass-battle.profile-quality"
                                          "MassBattlePosePhaseCounts"
                                          "massBattlePoseBucket"
                                          "mass_battle_pose_atlas_ready_"
                                          "pixel_height"
                                          "animation_system_"
                                          "MassBattleModelYawOffset"
                                          "Baking animation atlas"
                                          "used_pose_slots"
                                          "mass_battle_pose_meshes_")
    string(FIND "${_battlefield_scene}" "${_mass_instancing_contract}" _mass_instancing_found)
    if(_mass_instancing_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle bounded instancing misses ${_mass_instancing_contract}")
    endif()
endforeach()
if(_battlefield_scene MATCHES "ticks_per_pose_frame|mass_battle_pose_frame_")
    message(FATAL_ERROR "Infantry Mass Battle must not rebuild and upload pose meshes per frame")
endif()
foreach(_mass_camera_contract IN ITEMS "CameraMode::RTS"
                                       "MassBattleRtsCameraRevision"
                                       "minimum.x"
                                       "rts.target_min"
                                       "projection_offset_x = -0.12F")
    string(FIND "${_battlefield_scene}" "${_mass_camera_contract}" _mass_camera_found)
    if(_mass_camera_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle camera framing misses ${_mass_camera_contract}")
    endif()
endforeach()
foreach(_mass_terrain_contract IN ITEMS "presentation_position"
                                        "terrain_->sampleBilinear"
                                        "presentation_position.y")
    string(FIND "${_battlefield_scene}" "${_mass_terrain_contract}" _mass_terrain_found)
    if(_mass_terrain_found LESS 0)
        message(FATAL_ERROR "Infantry Mass Battle terrain placement misses ${_mass_terrain_contract}")
    endif()
endforeach()
string(FIND "${_battlefield_scene}" "mesh_id(feature.kind)" _legacy_world_feature_instance)
if(NOT _legacy_world_feature_instance LESS 0)
    message(FATAL_ERROR "Battlefield must not publish legacy WorldFeature instances without prototypes")
endif()
string(FIND "${_builtin_scenes}" "current_scene != foundation::scene_id(\"scene.infantry-mass-battle\")"
       _mass_battle_pause_route)
if(_mass_battle_pause_route LESS 0)
    message(FATAL_ERROR "Infantry Mass Battle must be allowed to open the pause overlay")
endif()
file(READ "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.cpp" _game_application)
string(FIND "${_game_application}" "model().set(\"fps\"" _fps_publication)
string(FIND "${_game_application}" "if (route_revision!=rml_route_revision_)" _route_mount)
if(_fps_publication LESS 0 OR _route_mount LESS 0 OR _fps_publication GREATER _route_mount)
    message(FATAL_ERROR "FPS must be present in the UI model before an RmlUi route is mounted")
endif()
if(NOT _menu_rml MATCHES "autofocus")
    message(FATAL_ERROR "main menu must autofocus Battlefield")
endif()
if(NOT _menu_rml MATCHES "scene[.]open-infantry-mass-battle")
    message(FATAL_ERROR "main menu must expose Infantry Mass Battle")
endif()
if(_menu_rml MATCHES "World configuration" OR _menu_rml MATCHES "Native D3D12")
    message(FATAL_ERROR "main menu contains a removed competing entry/status")
endif()
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/MainMenuScene.hpp" _menu_header)
if(_menu_header MATCHES "MainMenuEntry::WorldConfig|WorldConfig,")
    message(FATAL_ERROR "MainMenuEntry still exposes a separate WorldConfig item")
endif()
