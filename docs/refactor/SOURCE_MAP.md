# Architecture refactor source map

Snapshot: `4735977aa8b839c8ef53cd7631d8f15dbc0068f1` (PR00).

This is the entry map for PR01-PR17 source review. It deliberately excludes
`external/**`, generated build trees and historical `concat.txt` from Genomes
code-debt counts. The checked-in Diligent gitlink may be locally modified and
must never be staged, reset or overwritten by this program.

## Build and target map

| Layer | CMake entry points | Principal targets / executables | Refactor packages |
|---|---|---|---|
| Foundation/services | `engine/{foundation,io,jobs,proc,spatial}/CMakeLists.txt` | `genomes::foundation`, `io`, `jobs`, `proc`, `spatial` | PR01, PR04-PR06, PR11 |
| Neutral toolkit | `engine/{math,geometry,camera,assets}/CMakeLists.txt` | `genomes::math`, `geometry`, `camera`, optional `assets` | guarded, no geometry migration |
| Simulation | `engine/simulation/CMakeLists.txt` | `genomes::simulation` | PR01, PR07, PR16 |
| Platform/input/UI | `engine/{platform,input,ui}/CMakeLists.txt` | `genomes::platform`, `input`, `ui`, optional `ui_rml` | PR05, PR12, PR14 |
| Physics/navigation | `engine/{physics,navigation}/CMakeLists.txt` | `genomes::physics`, `navigation` | PR07, PR09 |
| Presentation | `engine/render/CMakeLists.txt`, `engine/render/{core,graph,gpu_scene,compute,diligent}` | `genomes::render*`, Diligent-only production backend | PR09, PR13, PR16 |
| Runtime | `engine/runtime/CMakeLists.txt` | `genomes::runtime_core`, `genomes::game_scenes`, compatibility aggregate `genomes::runtime` | PR08, PR12 |
| World domains | `engine/world`, `modules/{terrain,hydrology,roads,buildings,world_render}` | `genomes::world_core`, `genomes::world_generation`, aggregate `genomes::world`, and corresponding `genomes::*` libraries | PR03, PR09, PR10 |
| Combat domains | `modules/{combat,weapons,ballistics,infantry,gameplay}` | corresponding `genomes::*` static libraries | PR02, PR06-PR07, PR10-PR11 |
| Destruction | `modules/destruction/CMakeLists.txt` | `genomes::destruction` | PR07, PR09 |
| Applications | `apps/{game,headless,menu,proc_viewer,asset_probe,render_smoke}` | product and diagnostic executables | PR06, PR08, PR12, PR15 |
| Tests/benchmarks | `tests/native`, `tests/infantry_upgrade`, `benchmarks`, `tools/benchmark` | CTest executables, source guards and benchmarks | all packages |

At the baseline, 213 checked-in `add_library`/`add_executable` declarations are
present across project CMake files (including tests, benchmarks and conditional
targets). Target closure remains to be reviewed per package; a declaration
count is not proof of a clean dependency closure.

## Public-header inventory

The baseline contains 227 tracked project headers under engine/module/app
include surfaces. Review ownership by boundary rather than treating all headers
as equally public:

- canonical public surfaces: `engine/**/include/genomes/**` and
  `modules/**/include/genomes/**`;
- backend-local headers: `engine/render/diligent/*.hpp` and implementation
  directories; these must not leak through neutral public APIs;
- application composition headers: `apps/**/*.hpp`; these are consumers, not
  engine API;
- compatibility candidates to audit in PR08/PR09/PR17:
  `engine/runtime/include/genomes/runtime/{BattlefieldScene,BuildingLabScene,MainMenuScene,UnitLabScene,WorldConfigScene,WorldLabScene,BuiltinScenes}.hpp`
  and the broad `modules/world/include/genomes/world/**` surface.

High-risk public contracts and their first review packages:

| Contract | Header root | First package |
|---|---|---|
| Jobs/fence/handles | `engine/jobs/include/genomes/jobs` | PR01 |
| System graph/ticks/ECS | `engine/simulation/include/genomes/simulation` | PR01/PR07 |
| Seeds/cache/generation | `engine/proc/include/genomes/proc` | PR04/PR11 |
| Runtime scenes/commands | `engine/runtime/include/genomes/runtime` | PR08/PR12 |
| World/query/save | `engine/world/include/genomes/world` | PR04/PR09 |
| Buildings | `modules/buildings/include/genomes/buildings` | PR03 |
| Infantry/model compiler | `modules/infantry/include/genomes/infantry` | PR02/PR10/PR11 |
| Combat/weapons/ballistics | `modules/{combat,weapons,ballistics}/include/genomes` | PR06/PR07/PR10 |
| Render ABI/snapshots | `engine/render/**/include/genomes/render` | PR09/PR13 |

## Configuration and content map

| Source | Current role | Planned ownership |
|---|---|---|
| `CMakePresets.json` | build/test profiles and feature switches | PR14 matrix and boundary guards |
| root/cmake `CMakeLists.txt`, `cmake/*.cmake` | dependency and backend configuration | PR14 closure/visibility review |
| `mods/core/**/*.json` | core data and scene/UI manifests | PR06/PR10/PR15 typed catalogs |
| `mods/core/**/*.rml`, `*.rcss` | RmlUi documents/styles | PR12/PR14 structural validation |
| command-line parsing in apps/runtime | boundary overrides and actions | PR06/PR12 typed commands |
| hardcoded domain defaults in C++ | active values and ABI/algorithm constants mixed together | PR06/PR10 classification and migration |

Runtime configuration must end as `parse → validate → resolve → canonicalize →
fingerprint → freeze`. A JSON DOM is never a runtime domain contract.

## Implementation review queue

`UNREVIEWED` means not yet reviewed for this refactor, not known-bad.

| Package | Primary implementation roots | Baseline review state |
|---|---|---|
| PR01 | `engine/jobs/src`, `engine/simulation/src/SystemGraph.cpp` | R001-R004 reviewed; remainder UNREVIEWED |
| PR02 | `modules/infantry/src`, combat squad/AI sources, Unit Lab sources | UNREVIEWED |
| PR03 | `modules/buildings/src`, world site request users | UNREVIEWED |
| PR04 | world save and `engine/io/src/AtomicFile.cpp`, seed call sites | UNREVIEWED |
| PR05-PR06 | content/mod/plugin/config loaders and registries | UNREVIEWED |
| PR07 | gameplay scenario, simulation graph, physics/combat orchestration | UNREVIEWED |
| PR08 | `engine/runtime/src` split between runtime core and game scenes, app composition roots | runtime split and product router reviewed; full composition ownership remains UNREVIEWED |
| PR09 | engine/world plus terrain/hydrology/roads/buildings/world_render sources | world target split reviewed; remaining consumer/namespace migration UNREVIEWED |
| PR10 | equipment/weapons/material/AI/world/appearance value sources | UNREVIEWED |
| PR11 | infantry compiler, artifact cache, Unit Lab controller | UNREVIEWED |
| PR12 | application commands, CLI/RmlUi adapters, viewport/picking/capture | UNREVIEWED |
| PR13 | skinned CPU/HLSL layouts and Diligent resource lifetime | UNREVIEWED |
| PR14 | all project CMake and source/config guards | UNREVIEWED |
| PR15 | appearance catalog and Unit Lab adapter | UNREVIEWED |
| PR16 | graph/AI scratch, extraction/UI update paths | WAIT_BASELINE |
| PR17 | compatibility headers, old config paths and fallbacks | WAIT_PRIOR_PACKAGES |

## Literal classification K1-K6

| Class | Meaning | Treatment | Examples |
|---|---|---|---|
| K1 | ABI/layout constant | keep in code, name/version and assert | 69 bones, four influences/morphs, struct offsets |
| K2 | algorithm/determinism constant | keep in code unless algorithm version changes | RNG mixers, hash salts, numerical iteration rules |
| K3 | domain tuning/content value | migrate to typed validated catalogs | AI ranges/cadence, equipment/material/world profiles |
| K4 | execution/presentation tuning | typed execution/presentation profile after measurement | batch grain, cache budget, upload thresholds |
| K5 | boundary/resource limit | typed validated limit with checked arithmetic | save bytes/counts, bounded reads, maximum collections |
| K6 | fixture/tolerance/golden constant | preserve as test/reference evidence | JS seeds, parity tolerances, raw semantic vertex IDs |

Classification is semantic: a numeric search result is not debt until its owner
and class are established. Vendor, generated, fixture and historical files are
excluded from K3/K4 migration unless a package explicitly names them.

## Baseline CodeGraph observations

- `.codegraph` is present and was current at the baseline audit: 9,861 indexed
  files, 263,339 nodes and 742,125 edges. Vendor symbols inflate global counts,
  so queries must constrain results to project roots.
- `JobFence` has only the nominal smoke/benchmark consumers in project code.
- `SystemGraph::run` is implemented in one source and consumed through the
  simulation API; PR01 must run `codegraph callers/impact/affected` again after
  rebasing immediately before changes.
- PR17 must use CodeGraph impact/affected before removing any forwarding header
  or broad namespace surface.
