# Architecture refactor tracker (PR00-PR17)

Status: normative for the architecture refactor program.

Baseline commit: `4735977aa8b839c8ef53cd7631d8f15dbc0068f1`

Denominators: **55 decisions / 18 packages / 28 acceptance scenarios**.

Current source progress: **45/55 decisions CODE_READY; 10/18 packages complete**.
Verification progress: **0/28 acceptance scenarios verified**.

This tracker records source readiness separately from verification. `CODE_READY`
means that the implementation and source review for a row are complete.
`VERIFIED` requires user-supplied configure, build, CTest, benchmark or GPU
evidence tied to the exact commit in `Verified SHA`. An empty SHA is not
evidence. The implementation states are `PLANNED`, `BASELINE_CONFIRMED`,
`CODE_READY`, `ACCEPTED_DEFERRED`, and `REJECTED`. Verification states are
`NOT_RUN`, `PASS`, and `FAIL`.

## Decision register

| ID | Package | Owner | Kind | Decision / acceptance condition | Implementation | Code SHA | Verification | Verified SHA | Evidence |
|---|---|---|---|---|---|---|---|---|---|
| R001 | PR01 | jobs | FIX | `JobFence(expected_count)` is one-shot; over-signal fails without mutation; counter and wait predicate share one mutex. | CODE_READY | 31217c0 | NOT_RUN | - | `jobs.fence` covers over-signal, pending preservation and multiple waiters without sleeps. |
| R002 | PR01 | jobs | FIX | Job lifecycle is one mutex-protected `Running/ClosingDrain/ClosingCancel/Stopped` state; submit and close are atomic relative to the queue. | CODE_READY | bf46621 | NOT_RUN | - | `jobs.lifecycle` covers Drain, CancelPending, rejected post-close submission and final state. |
| R003 | PR01 | jobs | FIX | Shutdown is idempotent and owner-thread-only; worker destruction fails fast; launcher failure rolls back and joins started workers. | CODE_READY | bf46621 | NOT_RUN | - | `jobs.lifecycle` covers auto workers and deterministic injected-launcher rollback; source review covers owner/worker fail-fast. |
| R004 | PR01 | simulation | FIX | A SystemGraph batch drains every accepted job after the first failure and publishes neither successors nor command buffers. | CODE_READY | 70ba38c | NOT_RUN | - | `simulation.system_graph_batch` covers main-thread failure with active worker and reverse completion worker failure. |
| R005 | PR02 | infantry AI | FIX | Steering measures waypoint and enemy independently; engagement/fire depends only on current target range and visibility contract. | CODE_READY | db10398 | NOT_RUN | - | `infantry.simulation` covers a 2 m waypoint with a 100 m live target. |
| R006 | PR02 | infantry AI | FIX | Squad contact identity is `SquadKey{side, squad_id}`; no squad is `optional`, never implicit zero. | CODE_READY | 47f82bd | NOT_RUN | - | `infantry.simulation` covers same local ID across sides and two no-squad units. |
| R007 | PR02 | infantry/ECS adapter | FIX | Agent stores the full generational `EntityId`; removal centrally clears all sidecars and references. | CODE_READY | 4b705c4 | NOT_RUN | - | `infantry.simulation` covers external destruction and reuse of an index by another entity generation. |
| R008 | PR02 | Unit Lab | FIX | Shared variation validation accepts finite `0.0..1.75`; legacy controls cannot produce `2.0`. | CODE_READY | 3204f38 | NOT_RUN | - | Genome, compiler and Unit Lab boundary tests cover limits and reject 2.0. |
| R009 | PR03 | buildings | FIX | Building `center` is the center and `extent` is full XYZ size; net room dimensions are positive and axes are consistent. | CODE_READY | d12134a | NOT_RUN | - | `buildings.generator` checks full net XYZ room extents. |
| R010 | PR03 | buildings | FIX | Room validation covers finite values and checked reservation arithmetic before allocation. | CODE_READY | d12134a | NOT_RUN | - | `buildings.generator` rejects 2×10, wall 0.2, 16 rooms before reserve. |
| R011 | PR03 | buildings | FIX | Current site solver accepts only four-point rectangles centered/rotated as requested; other polygons fail explicitly. | CODE_READY | f36b571 | NOT_RUN | - | `buildings.generator` covers self-intersecting, concave, collinear, asymmetric and non-four-point sites. |
| R012 | PR03 | buildings | EXT | `BuildingPartKey` is semantic and independent of append order and seed. | CODE_READY | 9951f25 | NOT_RUN | - | `buildings.generator` checks stable identities across resolved seeds. |
| R013 | PR03 | buildings | EXT | Generator v2 content hash covers resolved spec, structural data and canonical rooms/parts; v1 incompatibility is explicit. | CODE_READY | 9951f25 | NOT_RUN | - | `buildings.generator` checks v2 identity/hash and explicit v1 incompatibility. |
| R014 | PR04 | save | REF | Domain save metadata excludes encoder counts, payload size and checksum; those belong to `EncodedSaveHeader`. | CODE_READY | 5e11834 | NOT_RUN | - | `world.save` uses metadata-only model and a private encoder header. |
| R015 | PR04 | save | REF | Existing byte layout/schema remains canonical when parity proves identical; encode(decode(bytes)) is byte-identical. | CODE_READY | 5e11834 | NOT_RUN | - | `world.save` re-encodes decoded data byte-identically. |
| R016 | PR04 | save | FIX | `WorldSaveLimits` bounds file, collections and working memory with checked sums/products before allocation. | CODE_READY | 6f22437 | NOT_RUN | - | `world.save` now has explicit regressions for file, region, entity, destroyed-object and header-size limits. |
| R017 | PR04 | proc/application | FIX | Resolved seed zero is invalid; application Auto is a distinct state resolved to a persisted nonzero seed. | CODE_READY | d686ba7 | NOT_RUN | - | `menu.smoke` covers Auto resolution and explicit-zero rejection; world generator rejects unresolved zero. |
| R018 | PR04 | I/O | FIX | Atomic-file failures retain I/O categories; only invalid serialization is corrupt data. | CODE_READY | 5e11834 | NOT_RUN | - | `world.save` checks missing-file `NotFound` is preserved. |
| R019 | PR05 | content | FIX | Shared asset resolver rejects absolute/rooted/traversal/noncanonical paths and reparse points before bounded reads. | CODE_READY | b69670d | NOT_RUN | - | UI resolver checks canonical roots/reparse points and bounds manifests to 1 MiB. |
| R020 | PR05 | mods | FIX | Mod order is stable topological order: dependencies, then ready-node priority, then ID. | CODE_READY | 56b05e5 | NOT_RUN | - | `UiContentRegistry` uses ready-set topological ordering. |
| R021 | PR05 | content | REF | Registries validate duplicates/name/hash collisions in a candidate and atomically publish a frozen snapshot. | CODE_READY | 76cd265 | NOT_RUN | - | Discovery builds a candidate, rejects duplicate strings and scene-ID hash collisions, then returns it as a snapshot. |
| R022 | PR05 | foundation/content | EXT | Owning `Diagnostic` carries dynamic path/field/source text; lightweight `foundation::Error` remains static. | CODE_READY | 76cd265 | NOT_RUN | - | `UiContentError` owns message/path/field/source; `foundation::Error` remains unchanged. |
| R023 | PR05 | plugins | FIX | Native plugins require user permission plus `trusted_native`; C ABI is exception-safe with rollback and reverse dependency unload. | CODE_READY | 10601b2 | NOT_RUN | - | Plugin manager keeps host API alive through unload and rolls back failed loads. |
| R024 | PR06 | content | REF | Content module owns bounded read, manifests, provenance, canonical hashing and frozen snapshots; domains own fields. | CODE_READY | c69491c | NOT_RUN | - | `content.snapshot` covers bounded read, root-safe resolution, manifest core fields and frozen provenance; Tactical AI and UI registry consume the shared content boundaries. |
| R025 | PR06 | configuration | REF | Configuration flow is defaults/core/profile/topological overrides/allowed CLI, then validate, resolve, canonicalize, fingerprint and freeze. | CODE_READY | e195e39 | NOT_RUN | - | `content.configuration_snapshot` covers deterministic layer ordering, override resolution, canonical hashes and frozen scalar snapshots. |
| R026 | PR06 | configuration | FIX | Core unknown fields and missing references fail; IDs differ from display names; snapshots own stable text independent of JSON DOM. | CODE_READY | e195e39 | NOT_RUN | - | `content.configuration_snapshot` covers core unknown fields, missing references, distinct id/display_name values, owning strings and rejected CLI fields. |
| R027 | PR06 | configuration | EXT | Typed simulation, presentation and execution hashes derive from canonical values. | CODE_READY | 559ba9f | NOT_RUN | - | `foundation.config_hash` covers canonical field ordering and distinct typed hash spaces; Tactical AI uses the simulation hash after parsing and validation. |
| R028 | PR06 | combat AI | DATA | Tactical AI profile moves without tuning to typed core content; runtime stores no parser/DOM. | CODE_READY | ec69179 | NOT_RUN | - | `tactical-ai.json` is strict-loaded in the game composition root; `BattlefieldScenarioConfig` receives only `TacticalAIProfile`; combat and battlefield profile tests cover parity and injection. |
| R029 | PR07 | simulation | REF | One `SessionSimulationClock` and `TickContext` own tick, fixed dt and frequency; frame time remains presentation-only. | CODE_READY | 6f7bb15 | NOT_RUN | - | `simulation.session_clock` covers tick context. |
| R030 | PR07 | simulation | REF | Seconds/RPM conversion uses one deterministic ceil-to-next-tick function and preserves 60 Hz behavior. | CODE_READY | 6f7bb15 | NOT_RUN | - | `simulation.session_clock` covers 60 Hz and alternate-rate ceil conversion. |
| R031 | PR07 | gameplay | REF | `BattlefieldRuntime` owns authoritative ECS/infantry/physics/navigation/combat graph; scenario is a fixture of it. | PLANNED | 41bbf2b | NOT_RUN | - | BattlefieldScene delegates each tick exclusively to BattlefieldRuntime whenever it exists, and battlefield world clients now use canonical `world_core`; full presentation/state extraction remains open. |
| R032 | PR07 | simulation | REF | Production phases declare complete reads/writes and have exactly one PhysicsWorld step and one weapon-to-damage pipeline. | PLANNED | b85ed75 | NOT_RUN | - | Production Battlefield owns the explicit apply→single PhysicsWorld step→sync sequence and declares damage resource writes; authoritative weapon→ballistics→damage evidence remains open. |
| R033 | PR07 | gameplay | FIX | Tick failure freezes runtime in Failed, blocks commit/future ticks and retains the last valid presentation snapshot plus diagnostic. | CODE_READY | e1db796 | NOT_RUN | - | Battlefield blocks future fixed ticks and clears pending commands after graph failure. |
| R034 | PR08 | scenes | REF | Product scenes move to `genomes::game_scenes`; engine runtime retains neutral lifecycle, transitions and snapshot protocol only. | PLANNED | 1d7d1e4 | NOT_RUN | - | Product scene sources live under `engine/game_scenes`; runtime core no longer links world generation, while typed `WorldGenerationConfig`/`ApplicationCommand` ownership remains open. |
| R035 | PR08 | application | REF | Composition root owns catalogs/runtimes/backends/factories; application router owns product actions. | PLANNED | 68af563 | NOT_RUN | - | Application-scene target now owns an immutable built-in scene catalog and routing boundary; complete catalog/backend ownership remains open. |
| R036 | PR08 | runtime | FIX | Scene registration rejects duplicates and freezes before session; missing optional features return `UnavailableFeature`. | CODE_READY | 361b36c | NOT_RUN | - | `menu_smoke` covers duplicate/frozen registration and an unavailable scene that leaves no current scene and reports `UnavailableFeature`; infantry-off builtin registration uses that path. |
| R037 | PR09 | world | REF | Neutral `world_core` owns IDs/coordinates/region/query/save/site request; generation moves to `world_generation`. | PLANNED | f4c5cbb | NOT_RUN | - | Canonical `genomes::world_core` now owns coordinates, region IDs, query snapshots/services, save codec and `BuildingSiteRequest/Resolution`; legacy headers forward while preserving the old world type closure, and generation remains in migration. |
| R038 | PR09 | world | FIX | `GridLayout` is the sole cells/samples/spacing/extent source; current map size must be divisible by 8 m. | CODE_READY | 0803931 | NOT_RUN | - | `WorldScenario` and Battlefield terrain/navigation now derive samples, cells, spacing and origin from the same layout; `world.grid_layout` covers valid bounds and 129/601 rejection, with artifact dimensions asserted in `native_world_assembly`. |
| R039 | PR09 | world | REF | One immutable `ResolvedWorldArtifacts` revision feeds render/collision/navigation/destruction. | CODE_READY | 730705b | NOT_RUN | - | Deterministic/headless Battlefield finalization now calls the same `WorldScenario::compileArtifact` handoff as asynchronous generation, removing the duplicate hardcoded terrain/building path; all consumers remain revision-bound. |
| R040 | PR09 | world render | REF | Mesh compiler consumes resolved building plans and `PartId` draw ranges; it never invokes the generator. | CODE_READY | b939538 | NOT_RUN | - | `world.mesh_compiler` covers missing resolutions and stable part range output; user CTest pending. |
| R041 | PR09 | world generation | REF | Each generation stage has its own SeedPath, version and dependency fingerprint. | CODE_READY | 8edbbe6 | NOT_RUN | - | `integration.world_parity` covers complete deterministic terrain/hydrology/roads/buildings/vegetation stage fingerprints; `WorldScenario` rejects candidates without them. |
| R042 | PR10 | domain catalogs | DATA | Equipment, weapon/ammo, material, AI, world/building and appearance migrate in that order to typed frozen catalogs. | PLANNED | 4d08dc5 | NOT_RUN | - | Destruction materials and weapon definitions remain parity-backed; the ammunition sub-slice now has an owning strict loader, frozen provenance snapshot, deterministic fingerprint and one legacy battlefield profile fixture. Full equipment/ammo/material/AI/world migration remains open. |
| R043 | PR10 | infantry | FIX | Equipment size derives from data, capacity is validated, spawn resolves WeaponId through the shared catalog, and loaded text owns storage. | CODE_READY | 69e90ef | NOT_RUN | - | `EquipmentCatalog::validate()` checks slot/item/loadout identities, capacities, finite values and compatibility; initializer-list overflow fails instead of truncating, and all catalog/loadout/visual text fields now own `std::string` storage. Full external data migration remains tracked in R042. |
| R044 | PR11 | infantry | REF | Model compiler accepts a canonical immutable request and returns shared immutable artifact plus key without preview state. | CODE_READY | ea738c7 | NOT_RUN | - | `infantry.model_compiler` covers canonical legacy-color normalization, artifact-key identity and shared-pointer cache hits; Unit Lab owns latest-wins revision gating. |
| R045 | PR11 | proc cache | FIX | Artifact cache requires deep memory usage, distinguishes retained/shared/pinned bytes and does not cache oversize artifacts. | CODE_READY | 01d170f | NOT_RUN | - | `proc.artifact_cache` covers deep-size-required storage, pinned-entry eviction, retained/shared/external-pinned accounting, oversize bypass and clear. |
| R046 | PR11 | Unit Lab | FIX | Latest-wins controller keeps one active and one replaceable pending request; revision/request gates publication and scene exit drains. | CODE_READY | 3260064 | NOT_RUN | - | `runtime.unit_lab_request_gate` deterministically covers A→B→C replacement, stale completion, promotion, publication and cancel; `UnitLabScene` uses the same gate and drains its job on exit. |
| R047 | PR12 | UI/application | REF | RmlUi, CLI and tests parse at their boundaries into the same typed command variants; numeric parsing is strict and diagnosed. | PLANNED | b49fc8c | NOT_RUN | - | Camera, locomotion, expression, equipment, gene, variation and appearance CLI inputs now use the shared typed parser; full RmlUi parity remains open. |
| R048 | PR12 | Unit Lab | REF | View model tracks Geometry/Material/Pose/Presentation/UI dirtiness and only rebuilds affected outputs. | CODE_READY | e280e5d | NOT_RUN | - | `UnitLabDirtyState` retains the five typed categories; `SetAppearancePreset` marks Material/UI only, and presentation creates a material variant without rerunning model compilation or changing geometry/index buffers. |
| R049 | PR12 | UI/presentation | FIX | Camera, picking and capture use current RmlUi viewport metrics or the last valid rectangle. | CODE_READY | 24e7f84 | NOT_RUN | - | `ui.rml` measures the `unit-viewport` layout at 75/100/150%; `unit_lab` covers normalization and last-valid retention. |
| R050 | PR13 | render ABI | REF | Existing 69-bone/four-influence/four-morph ABI is named and versioned `SkinnedLayoutProfileV1` in shared C++/HLSL definitions. | CODE_READY | 8ddf046 | NOT_RUN | - | T26 source coverage; user D3D12/GPU acceptance pending. |
| R051 | PR13 | presentation | FIX | GPU upload retains immutable artifact ownership through submission, retires by fence and publishes revisions atomically after all resources succeed. | PLANNED | 7b075a0 | NOT_RUN | - | Source guards require complete VB/IB/palette candidates, deferred fence ownership, map-copy failure handling, and publication only after success; runtime/GPU evidence remains open. |
| R052 | PR14 | build | REF | Preset matrix, target visibility, self-contained headers, minimal consumers, structural config/RML guards and fixture manifest enforce boundaries. | CODE_READY | ceda2e6 | NOT_RUN | - | Shared CMake manifest validation checks schema, provenance, unique family IDs, safe roots and formats; public-header consumer sources cover core targets. Full target-closure review remains open. |
| R053 | PR15 | appearance | EXT | Manual `inspection-olive` is a validated data-only presentation preset and Material-only Unit Lab command. | CODE_READY | e280e5d | NOT_RUN | - | Stable ID/schema are declared, strict typed parsing accepts only `inspection-olive`, the feature card records provenance/determinism/cache/error policy, and a presentation test proves geometry/indices remain unchanged. |
| R054 | PR16 | performance | REF | Only measured optimizations ship; execution tuning preserves D1/D2 results and meets the stated target/p95 acceptance gate. | PLANNED | - | NOT_RUN | - | `docs/refactor/PR16_MEASUREMENT_GATE.md` plus the versioned input/schema fixture define the gate; user baseline and benchmark evidence are still required. |
| R055 | PR17 | architecture | REF | Remove only proven-unused bridges/fallbacks, close dependency review, and assign every R row a final disposition. | PLANNED | 754f99c | NOT_RUN | - | The unused `genomes::runtime` aggregate is removed and a target-exact closure guard is active; final dependency review and dispositions remain open. |

Kinds: `REF` preserves behavior/contracts while moving ownership, `FIX` requires
a counterexample regression, `DATA` requires old/new parity, and `EXT` creates a
new explicitly versioned capability or contract.

## Package register

| Package | Scope | Decisions | Implementation | Code SHA | Verification | Verified SHA |
|---|---|---:|---|---|---|---|
| PR00 | Baseline, tracker and source map | R001-R055 | CODE_READY | 6bdbe74 | NOT_RUN | - |
| PR01 | Jobs and SystemGraph lifetime | R001-R004 | CODE_READY | bf46621 | NOT_RUN | - |
| PR02 | Infantry state and Unit Lab input | R005-R008 | CODE_READY | 3204f38 | NOT_RUN | - |
| PR03 | Building generator contract | R009-R013 | CODE_READY | 2194765 | NOT_RUN | - |
| PR04 | Bounded canonical save | R014-R018 | CODE_READY | 6f22437 | NOT_RUN | - |
| PR05 | Packages and native plugins | R019-R023 | CODE_READY | 76cd265 | NOT_RUN | - |
| PR06 | Typed profile infrastructure | R024-R028 | CODE_READY | e195e39 | NOT_RUN | - |
| PR07 | Session clock and combat pipeline | R029-R033 | PLANNED | 41bbf2b | NOT_RUN | - |
| PR08 | Product scenes outside runtime | R034-R036 | PLANNED | 1d7d1e4 | NOT_RUN | - |
| PR09 | World core and resolved artifacts | R037-R041 | PLANNED | f4c5cbb | NOT_RUN | - |
| PR10 | Domain catalog migration | R042-R043 | PLANNED | 4d08dc5 | NOT_RUN | - |
| PR11 | Model compiler, cache, latest-wins | R044-R046 | CODE_READY | 55e4c59 | NOT_RUN | - |
| PR12 | Unified typed command path | R047-R049 | PLANNED | 10c9c6e | NOT_RUN | - |
| PR13 | Versioned GPU profile and ownership | R050-R051 | PLANNED | 7b075a0 | NOT_RUN | - |
| PR14 | CMake, presets, guards and hygiene | R052 | CODE_READY | ceda2e6 | NOT_RUN | - |
| PR15 | Data-only feature pilot | R053 | CODE_READY | 0e7ba51 | NOT_RUN | - |
| PR16 | Measurement-led optimization | R054 | PLANNED | - | NOT_RUN | - |
| PR17 | Migration closure | R055 | PLANNED | 754f99c | NOT_RUN | - |

## Acceptance scenario register

The scenario IDs are stable. A row can name several concrete CTest cases, but
each final row must cite the exact test name and tested SHA.

| ID | Package | Scenario | Planned evidence | Verification |
|---|---|---|---|---|
| T01 | PR01 | Main-thread callback throws while an accepted worker remains active; run drains it and commits nothing. | deterministic latches/barriers | NOT_RUN |
| T02 | PR01 | Batch completion order is reversed; first observed failure is stable and successors do not run. | deterministic latches/barriers | NOT_RUN |
| T03 | PR01 | Fence rejects over-signal without mutation and wakes multiple waiters exactly once. | jobs CTest | NOT_RUN |
| T04 | PR01 | Drain executes accepted queue; CancelPending cancels queued handles while active work finishes. | jobs CTest | NOT_RUN |
| T05 | PR01 | Nested helping wait works; launcher N failure rolls back; worker shutdown violation is detected before join. | jobs CTest | NOT_RUN |
| T06 | PR02 | Near waypoint plus distant enemy moves but does not engage/fire. | infantry AI CTest | NOT_RUN |
| T07 | PR02 | Same numeric squad on opposing sides and two no-squad units share no contacts. | squad CTest | NOT_RUN |
| T08 | PR02 | Reused ECS index with another generation/type cannot access stale agent sidecars. | infantry/ECS CTest | NOT_RUN |
| T09 | PR07 | Tick results match inline, one-worker and N-worker execution. | simulation CTest | NOT_RUN |
| T10 | PR07 | One EntityId flows command→weapon→ballistics→hit/damage; physics steps once. | integration CTest | NOT_RUN |
| T11 | PR02/PR12 | Variation boundaries and identical UI/CLI/test typed-command effects; invalid numbers fail. | Unit Lab command CTest | NOT_RUN |
| T12 | PR11 | Cache hit shares artifact without deep copy; budget/eviction/pinned accounting is exact. | cache CTest | NOT_RUN |
| T13 | PR11/PR12 | A→B→C publishes only C across success/failure/cancel and avoids unrelated rebuilds. | Unit Lab controller CTest | NOT_RUN |
| T14 | PR11 | Every artifact-affecting request field changes the key; normalized legacy color aliases do not. | compiler CTest | NOT_RUN |
| T15 | PR03 | `2×10`, wall `0.2`, 16 rooms fails safely; valid dimensions use XYZ/full extents. | buildings CTest | NOT_RUN |
| T16 | PR03 | L, self-intersecting, collinear and asymmetric parcels return explicit unsupported/invalid errors. | buildings CTest | NOT_RUN |
| T17 | PR03 | Decoration insertion does not renumber structural PartIds; v2 content hash and v1 incompatibility are stable. | buildings CTest | NOT_RUN |
| T18 | PR09 | Grid sizes 128/600/4096 pass, 129/601 fail; edge coordinates agree with layout. | world CTest | NOT_RUN |
| T19 | PR09 | One revision shares BuildingId/PartId through generation/render/collision/nav; mixed revisions fail. | world integration CTest | NOT_RUN |
| T20 | PR04 | Nonempty canonical save round-trips byte-identically; duplicates and seed zero fail. | save CTest | NOT_RUN |
| T21 | PR04 | Truncation, overflow, checksum/version errors and every resource limit fail before excess allocation. | save CTest | NOT_RUN |
| T22 | PR05 | Dependency order beats priority; ready ties resolve by priority then ID. | mod registry CTest | NOT_RUN |
| T23 | PR05 | Windows rooted/traversal/noncanonical/reparse paths fail before reading outside root. | resolver CTest | NOT_RUN |
| T24 | PR05 | Duplicate/colliding second mod rolls back candidate and preserves prior frozen snapshot. | content CTest | NOT_RUN |
| T25 | PR05/PR08 | Plugin partial load rolls back, host context survives callbacks, unload is reverse dependency order; scene lifecycle errors/cancel cleanly. | plugin/scene CTest | NOT_RUN |
| T26 | PR13 | C++/HLSL ABI constants/layout/version agree; wrong profile and overflow fail; publication is atomic. | CPU CTest + user D3D12/GPU acceptance | NOT_RUN |
| T27 | PR08/PR14 | Minimal runtime links no gameplay/infantry/buildings and infantry-OFF composition reports unavailable. | dependency guard + build | NOT_RUN |
| T28 | PR12/PR14 | Structural RML controls and viewport scales 75/100/150% map correctly without geometry rebuild. | UI CTest | NOT_RUN |

## Baseline findings

- The only jobs test at the baseline is `jobs.core` from
  `tests/native/jobs/job_system_smoke.cpp`; it covers normal submission, a
  nested helping wait and a nominal fence, but none of T01-T05 failure paths.
- No baseline test exercises SystemGraph callback/submit/handle failure while
  another job from the same batch remains active.
- R001-R004 are source-confirmed at the baseline SHA. They are not fixed by
  PR00 and must not be described as verified.
- JS fixtures, generated goldens, semantic vertex numbering, renderer backend,
  ECS and domain behavior are unchanged by PR00.

## Update rules

1. Before changing a row, reread `main` and record the exact code SHA.
2. A FIX first gains a deterministic counterexample test; random sleep is not
   synchronization evidence.
3. Before a commit, review CodeGraph impact, the Diligent gitlink, unintended
   files, relevant source guards, and `git diff --check`.
4. Before publish, fetch `origin/main` and require it to be an ancestor of the
   local HEAD. Never force-push or absorb local submodule changes.
5. Configure, compilation, CTest, benchmarks and GPU acceptance are performed
   by the user. Record failures in ignored `build-error.log`; clear it only
   after a matching successful build.
