# ADR: threepp presentation on the native Genomes engine

Decision: accepted by the project owner in the migration conversation.
Implementation state: first bootstrap slice; runtime acceptance is pending.
Baseline: `188dc2aac34821f2a21cec3eb66ac5049f3ccf60`. First code commit: `25a661f343509f676e60d67be69846b40e3cc339`.

## Scope

The final 3D presentation direction is threepp/GLRenderer, not two concurrently
developed game renderers. SDL3 remains the one owner of the window, event queue,
OpenGL context and buffer swap. Initial desktop profile: OpenGL 3.3 core, depth
24, stencil 8, double buffering. Windows is the immediate acceptance platform;
Linux and other platforms require their own evidence. No Vulkan/FSR/DLSS/audio,
GLFW frontend or threepp editor is enabled in this slice.

The pinned revision is `ad9571cbcbb5e27c4dd582d4810fb0b235534f60`. It is a reviewed source pin, not a guarantee
of compilation, runtime compatibility or performance on the user's machine.
Only `ThreeppGlBootstrap.cpp` may use the reviewed private LoadGlad header.

## Boundaries

Keep native ECS, JobSystem, generation seeds, anatomy, physics, navigation,
combat, save/load and immutable presentation data. threepp Object3D is not an
entity store. Domain modules do not own GL resources. Renderer resource cleanup
runs while the SDL context is current on its owning thread.

Phase A presents the existing generated data, without silently changing the
reference arrays. Phase B consolidates duplicated geometry operations and
introduces the selected threepp primitives through a narrow geometry provider.
Do not rebuild a second Three.js inside Genomes. Do not use a preauthored human
mesh to replace the procedural anatomy requirement.

## Transitional composition

`HEADLESS`, `THREEPP_GL`, `DILIGENT_LEGACY` are configuration-time choices.
The legacy game remains unchanged. THREEPP_GL currently builds only
`genomes_threepp_bootstrap`: a context lifecycle probe without simulation,
character geometry or UI. This deliberately avoids advertising a partial game.
The actual GameApplication factory and existing run scripts are still open
items TP-01-04/05 and TP-15-01/02. They switch after the relevant adapter gates.

Before implementing the full GPU adapter, collect the G01/G02 bootstrap
acceptance described in KOMENDY.txt. This is not final G02 acceptance of game
jobs, scene destruction and UI; those portions remain tracked as DOING.

## Existing documents

This decision supersedes only the final-renderer choice in
`docs/0.1_TARGET_ARCHITECTURE.txt`, `docs/0.3_CODING_AND_DEPENDENCY_RULES.txt`
and `docs/TARGET_IMPLEMENTATION_CONTRACT.txt`. Their other rules remain.
Full editorial synchronization is not completed; TP-00-03 stays DOING.
Diligent-specific compute paths must have an explicit replacement or approved
CPU fallback before removal. Diligent sources and release scripts are not
removed by this change.

## Verification

Code and configuration were prepared and reviewed, not built or executed by
the agent. The user supplies logs and captures tied to the tested commit.
No FPS claim, visual parity claim, or successful toolchain result is implied.
