if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(source "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldRuntime.cpp")
if(NOT EXISTS "${source}")
    message(FATAL_ERROR "Missing authoritative battlefield source: ${source}")
endif()
file(READ "${source}" battlefield)

# R032/R033: the authoritative runtime declares the complete tick boundary,
# including the hand-off into presentation extraction.  Keep these checks
# source-based so they remain useful before the user-owned build matrix runs.
foreach(required_phase IN ITEMS
        "SystemPhase::Navigate"
        "SystemPhase::MoveIntent"
        "SystemPhase::PhysicsCommands"
        "SystemPhase::PhysicsStep"
        "SystemPhase::Commit"
        "SystemPhase::PresentationExtract"
        "battlefield.navigate"
        "battlefield.move-intent"
        "battlefield.physics-commands"
        "battlefield.physics-step"
        "battlefield.commit"
        "battlefield.presentation-extract"
        "infantry_->applyPhysicsCommands();"
        "physics_.step(static_cast<float>(context.fixed_dt));"
        "infantry_->syncPhysicsState();"
        "infantry_->extractPresentation();"
        "const BattlefieldScenarioSnapshot last_good_snapshot = snapshot_;"
        "snapshot_ = last_good_snapshot;"
        "state_ = BattlefieldRuntimeState::Failed"
        "state_ != BattlefieldRuntimeState::Running")
    string(FIND "${battlefield}" "${required_phase}" phase_position)
    if(phase_position EQUAL -1)
        message(FATAL_ERROR
                "BattlefieldRuntime is missing R032/R033 contract: ${required_phase}")
    endif()
endforeach()

if(battlefield MATCHES "combat_flow_.submitFire\\(fire\\)")
    message(FATAL_ERROR "BattlefieldRuntime must submit the WeaponStepOutput FireIntent")
endif()

string(FIND "${battlefield}" "phase = simulation::SystemPhase::Navigate" navigate_phase)
string(FIND "${battlefield}" "phase = simulation::SystemPhase::MoveIntent" move_intent_phase)
string(FIND "${battlefield}" "phase = simulation::SystemPhase::PhysicsCommands"
       physics_commands_phase)
string(FIND "${battlefield}" "phase = simulation::SystemPhase::PhysicsStep" physics_step_phase)
string(FIND "${battlefield}" "phase = simulation::SystemPhase::Commit" commit_phase)
string(FIND "${battlefield}" "phase = simulation::SystemPhase::PresentationExtract"
       presentation_phase)
if(navigate_phase GREATER move_intent_phase OR
   move_intent_phase GREATER physics_commands_phase OR
   physics_commands_phase GREATER physics_step_phase OR
   physics_step_phase GREATER commit_phase OR
   commit_phase GREATER presentation_phase)
    message(FATAL_ERROR
            "BattlefieldRuntime phase order must be Navigate -> PhysicsCommands -> PhysicsStep -> Commit -> PresentationExtract")
endif()

# R032 requires one typed production path. Keep the complete hand-off visible
# in the source so a compatibility hitscan shortcut cannot silently become the
# authoritative implementation again.
foreach(required_text IN ITEMS
        "weapon_handling_.step("
        "infantry_->readWeaponHandlingView"
        "weapon_pose_tasks_[packed_entity] = output.pose"
        "combat_flow_.submitFire(*output.fire)"
        "ballistics_->queueFire(ballistic_request)"
        "ballistics_->advanceFixed(false)"
        "combat_flow_.submitImpact(impact)"
        "combat_flow_.commitDamage()"
        "damage_buffer_.push("
        "combat_.apply(damage_buffer_)"
        "projectile_sources_[projectile_value] = fromPacked(request.source)"
        "fromPacked(contact.trace.semantic_id)"
        "const auto source = projectile_sources_.find(contact.trace.projectile_id)"
        "const simulation::EntityId target_entity = fromPacked(contact.trace.semantic_id)")
    string(FIND "${battlefield}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Battlefield pipeline lost required contract: ${required_text}")
    endif()
endforeach()

if(battlefield MATCHES "WeaponController::tryFire\\(")
    message(FATAL_ERROR "BattlefieldRuntime must not bypass WeaponHandlingSystem")
endif()
if(battlefield MATCHES "emitCombatEvents")
    message(FATAL_ERROR "BattlefieldRuntime must not invoke the fixture combat emitter")
endif()

# T10: the executable regression must observe the authoritative identity
# hand-off, not only aggregate fired/impact counters.
file(READ "${GENOMES_SOURCE_DIR}/tests/native/battlefield_profile_tests.cpp"
     battlefield_profile_test)
foreach(required_test_text IN ITEMS
        "last_shot_source.isValid()"
        "last_impact_source.isValid()"
        "last_impact_target.isValid()")
    string(FIND "${battlefield_profile_test}" "${required_test_text}" test_position)
    if(test_position EQUAL -1)
        message(FATAL_ERROR
                "T10 regression lost explicit EntityId assertion: ${required_test_text}")
    endif()
endforeach()

# The graph declaration is the phase boundary: combat must be registered
# before damage and both phases must be wired to callbacks.
string(FIND "${battlefield}" "combat.phase = simulation::SystemPhase::CombatBallistics"
       combat_phase_position)
string(FIND "${battlefield}" "damage.phase = simulation::SystemPhase::DamageDestruction"
       damage_phase_position)
string(FIND "${battlefield}" "combat.callback = [this](simulation::SystemContext& context)"
       combat_callback_position)
string(FIND "${battlefield}" "damage.callback = [this](simulation::SystemContext&)"
       damage_callback_position)
foreach(position IN ITEMS combat_phase_position damage_phase_position
        combat_callback_position damage_callback_position)
    if(${position} EQUAL -1)
        message(FATAL_ERROR "Battlefield graph is missing the combat/damage phase boundary")
    endif()
endforeach()
if(combat_phase_position GREATER damage_phase_position OR
   combat_callback_position GREATER damage_callback_position)
    message(FATAL_ERROR "Battlefield combat phase must precede damage phase")
endif()

# Physics has one owner and one call site in this production scenario. The
# infantry adapter is restricted to command submission and post-step sync.
string(REGEX MATCHALL "physics_\\.step\\(" physics_step_calls "${battlefield}")
list(LENGTH physics_step_calls physics_step_count)
if(NOT physics_step_count EQUAL 1)
    message(FATAL_ERROR
            "BattlefieldRuntime must contain exactly one authoritative physics_.step call")
endif()
if(battlefield MATCHES "infantry_->stepPhysics\\(")
    message(FATAL_ERROR "Infantry must not own the authoritative physics step")
endif()

# The compatibility scenario is intentionally only a forwarding facade.  Any
# graph/resource implementation here would reintroduce a second owner.
file(READ "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldScenario.cpp"
     scenario_facade)
foreach(forbidden_facade_text IN ITEMS
        "SystemGraph"
        "physics_.step("
        "queueFire()"
        "applyImpactDamage()"
        "EntityStore")
    string(FIND "${scenario_facade}" "${forbidden_facade_text}" facade_forbidden_position)
    if(NOT facade_forbidden_position EQUAL -1)
        message(FATAL_ERROR
                "BattlefieldScenario facade regained authoritative state: ${forbidden_facade_text}")
    endif()
endforeach()

message(STATUS "Battlefield weapon-to-damage source contract inspected")
