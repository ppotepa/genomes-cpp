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
| Runtime | `engine/runtime/CMakeLists.txt`, `engine/game_scenes/CMakeLists.txt` | `genomes::runtime_core`, `genomes::game_scenes`, `genomes::application_scenes` | PR08, PR12 |
| World domains | `engine/world`, `modules/{terrain,hydrology,roads,buildings,world_render}` | `genomes::world_core`, `genomes::world_generation`, and corresponding `genomes::*` libraries | PR03, PR09, PR10 |
| Combat domains | `modules/{combat,weapons,ballistics,infantry,gameplay}` | corresponding `genomes::*` static libraries | PR02, PR06-PR07, PR10-PR11 |
| Destruction | `modules/destruction/CMakeLists.txt` | `genomes::destruction` | PR07, PR09 |
| Applications | `apps/{game,headless,menu,proc_viewer,asset_probe,render_smoke}` | product and diagnostic executables | PR06, PR08, PR12, PR15 |
| Tests/benchmarks | `tests/native`, `tests/infantry_upgrade`, `benchmarks`, `tools/benchmark` | CTest executables, source guards and benchmarks | all packages |

At the baseline, 213 checked-in `add_library`/`add_executable` declarations are
present across project CMake files (including tests, benchmarks and conditional
targets). Target closure remains to be reviewed per package; a declaration
count is not proof of a clean dependency closure.

### PR17 target-closure audit

Source review at `5e204ea` found no in-tree CMake consumer of the
`genomes::runtime` compatibility aggregate. That bridge was removed, while the
runtime core and application-scene aliases remain explicit. The
`genomes::world` aggregate was removed at `d789c5a` after all project CMake
clients were split between `genomes::world_core` and
`genomes::world_generation`. The source guard in
`tests/native/architecture_refactor_tracker.cmake` prevents the aggregate from
returning. This closes the target-link source-review slice only; configure,
build, CTest and external-consumer compatibility are still pending.

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
- compatibility candidates audited in PR08/PR09/PR17:
  product-scene headers now live under
  `engine/game_scenes/include/genomes/game_scenes/` after the `7d4d3b5`
  relocation, while only neutral lifecycle headers remain under runtime;
  the former `BattlefieldScene`, `BuiltinScenes`, `WorldConfigScene` and command/config bridges
  were retired after in-tree impact review at `7d56de8`. The application-owned
  `engine/game_scenes/include/genomes/game_scenes/BuiltinScenes.hpp` is canonical,
  and the broad `engine/world/include/genomes/world/**` compatibility surface.

High-risk public contracts and their first review packages:

| Contract | Header root | First package |
|---|---|---|
| Jobs/fence/handles | `engine/jobs/include/genomes/jobs` | PR01 |
| System graph/ticks/ECS | `engine/simulation/include/genomes/simulation` | PR01/PR07 |
| Seeds/cache/generation | `engine/proc/include/genomes/proc` | PR04/PR11 |
| Runtime scenes/commands | `engine/runtime/include/genomes/runtime` | PR08/PR12 |
| World/query/save | canonical `engine/world/include/genomes/world_core` with compatibility `engine/world/include/genomes/world` | PR04/PR09 |
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
| PR07 | gameplay scenario, simulation graph, physics/combat orchestration | R029-R033 source ownership and pipeline reviewed through `5e204ea`; BattlefieldScenario is now a thin facade and BattlefieldScene has one runtime owner, with CTest/runtime verification pending |
| PR08 | `engine/runtime/src/SceneDirector.cpp`; `engine/game_scenes/src` product scenes and app composition | neutral `SceneCommand`/typed application-command boundary, product namespace/header relocation, CLI call-site migration and authoritative Battlefield handoff reviewed at `871dd88`; application-owned config and catalog routing remain source-reviewed; CTest/runtime verification remains pending |
| PR09 | engine/world plus terrain/hydrology/roads/buildings/world_render sources | `world_core`/`world_generation` target split, canonical core headers, generation link closure and legacy forwarding aliases reviewed at `f4c5cbb`; all in-tree CMake consumers now name the narrow targets, and the former aggregate was removed/guarded at `d789c5a` |
| PR10 | equipment/weapons/material/AI/world/appearance value sources | material (`1ba42a6`), ammo (`4d08dc5`), equipment (`c29b346`), weapon (`82efa74`), Tactical AI (`f675811`), appearance (`52ab4a6`) and world/building profiles (`8908d43`) have parity/source contracts, strict loaders, provenance/fingerprints and frozen runtime ownership; CTest/runtime verification remains pending |
| PR11 | infantry compiler, artifact cache, Unit Lab controller | R044-R046 source slice reviewed at `55e4c59`; CTest registrations and source guards present; user verification pending |
| PR12 | application commands, CLI/RmlUi adapters, viewport/picking/capture | R047-R049 reviewed at `d3bb543`; seven-control RmlUi/CLI parser guard and syntax review complete; CTest/UI verification remains pending |
| PR13 | skinned CPU/HLSL layouts and Diligent resource lifetime | R050-R051 source guards and CPU restoration tests reviewed at `7b075a0`; D3D12/GPU acceptance remains user-owned and unverified |
| PR14 | all project CMake and source/config guards | preset matrix, target visibility, public-header consumers and structural guards reviewed at `ceda2e6`; target configure/build closure remains user verification |
| PR15 | appearance catalog and Unit Lab adapter | inspection-olive pilot catalog reviewed; broader catalog migration UNREVIEWED |
| PR16 | graph/AI scratch, extraction/UI update paths | measurement contract/schema and fixture-consuming infantry benchmarks reviewed at `8908d43`; reports retain raw samples and explicitly show `BASELINE_REQUIRED`; WAIT_BASELINE for user evidence |
| PR17 | compatibility headers, old config paths and fallbacks | product-scene/runtime namespace/header boundary, authoritative Battlefield fallback removal, final CLI migration, disposition register and all in-tree `genomes::world` CMake links reviewed at `a27a06f`/`d789c5a`; external-consumer compatibility and legacy config-path closure remain explicitly deferred |

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
