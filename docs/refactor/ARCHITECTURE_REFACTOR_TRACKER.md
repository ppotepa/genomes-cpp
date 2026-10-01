# Architecture refactor tracker (PR00-PR17)

Status: normative for the architecture refactor program.

Baseline commit: `4735977aa8b839c8ef53cd7631d8f15dbc0068f1`

Denominators: **55 decisions / 18 packages / 28 acceptance scenarios**.

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
| R001 | PR01 | jobs | FIX | `JobFence(expected_count)` is one-shot; over-signal fails without mutation; counter and wait predicate share one mutex. | CODE_READY | WORKTREE | NOT_RUN | - | `jobs.fence` covers over-signal, pending preservation and multiple waiters without sleeps. |
| R002 | PR01 | jobs | FIX | Job lifecycle is one mutex-protected `Running/ClosingDrain/ClosingCancel/Stopped` state; submit and close are atomic relative to the queue. | CODE_READY | WORKTREE | NOT_RUN | - | `jobs.lifecycle` covers Drain, CancelPending, rejected post-close submission and final state. |
| R003 | PR01 | jobs | FIX | Shutdown is idempotent and owner-thread-only; worker destruction fails fast; launcher failure rolls back and joins started workers. | CODE_READY | WORKTREE | NOT_RUN | - | `jobs.lifecycle` covers auto workers and deterministic injected-launcher rollback; source review covers owner/worker fail-fast. |
| R004 | PR01 | simulation | FIX | A SystemGraph batch drains every accepted job after the first failure and publishes neither successors nor command buffers. | CODE_READY | WORKTREE | NOT_RUN | - | `simulation.system_graph_batch` covers main-thread failure with active worker and reverse completion worker failure. |
| R005 | PR02 | infantry AI | FIX | Steering measures waypoint and enemy independently; engagement/fire depends only on current target range and visibility contract. | PLANNED | - | NOT_RUN | - | - |
| R006 | PR02 | infantry AI | FIX | Squad contact identity is `SquadKey{side, squad_id}`; no squad is `optional`, never implicit zero. | PLANNED | - | NOT_RUN | - | - |
| R007 | PR02 | infantry/ECS adapter | FIX | Agent stores the full generational `EntityId`; removal centrally clears all sidecars and references. | PLANNED | - | NOT_RUN | - | - |
| R008 | PR02 | Unit Lab | FIX | Shared variation validation accepts finite `0.0..1.75`; legacy controls cannot produce `2.0`. | PLANNED | - | NOT_RUN | - | - |
| R009 | PR03 | buildings | FIX | Building `center` is the center and `extent` is full XYZ size; net room dimensions are positive and axes are consistent. | PLANNED | - | NOT_RUN | - | - |
| R010 | PR03 | buildings | FIX | Room validation covers finite values and checked reservation arithmetic before allocation. | PLANNED | - | NOT_RUN | - | - |
| R011 | PR03 | buildings | FIX | Current site solver accepts only four-point rectangles centered/rotated as requested; other polygons fail explicitly. | PLANNED | - | NOT_RUN | - | - |
| R012 | PR03 | buildings | EXT | `BuildingPartKey` is semantic and independent of append order and seed. | PLANNED | - | NOT_RUN | - | - |
| R013 | PR03 | buildings | EXT | Generator v2 content hash covers resolved spec, structural data and canonical rooms/parts; v1 incompatibility is explicit. | PLANNED | - | NOT_RUN | - | - |
| R014 | PR04 | save | REF | Domain save metadata excludes encoder counts, payload size and checksum; those belong to `EncodedSaveHeader`. | PLANNED | - | NOT_RUN | - | - |
| R015 | PR04 | save | REF | Existing byte layout/schema remains canonical when parity proves identical; encode(decode(bytes)) is byte-identical. | PLANNED | - | NOT_RUN | - | - |
| R016 | PR04 | save | FIX | `WorldSaveLimits` bounds file, collections and working memory with checked sums/products before allocation. | PLANNED | - | NOT_RUN | - | - |
| R017 | PR04 | proc/application | FIX | Resolved seed zero is invalid; application Auto is a distinct state resolved to a persisted nonzero seed. | PLANNED | - | NOT_RUN | - | - |
| R018 | PR04 | I/O | FIX | Atomic-file failures retain I/O categories; only invalid serialization is corrupt data. | PLANNED | - | NOT_RUN | - | - |
| R019 | PR05 | content | FIX | Shared asset resolver rejects absolute/rooted/traversal/noncanonical paths and reparse points before bounded reads. | PLANNED | - | NOT_RUN | - | - |
| R020 | PR05 | mods | FIX | Mod order is stable topological order: dependencies, then ready-node priority, then ID. | PLANNED | - | NOT_RUN | - | - |
| R021 | PR05 | content | REF | Registries validate duplicates/name/hash collisions in a candidate and atomically publish a frozen snapshot. | PLANNED | - | NOT_RUN | - | - |
| R022 | PR05 | foundation/content | EXT | Owning `Diagnostic` carries dynamic path/field/source text; lightweight `foundation::Error` remains static. | PLANNED | - | NOT_RUN | - | - |
| R023 | PR05 | plugins | FIX | Native plugins require user permission plus `trusted_native`; C ABI is exception-safe with rollback and reverse dependency unload. | PLANNED | - | NOT_RUN | - | - |
| R024 | PR06 | content | REF | Content module owns bounded read, manifests, provenance, canonical hashing and frozen snapshots; domains own fields. | PLANNED | - | NOT_RUN | - | - |
| R025 | PR06 | configuration | REF | Configuration flow is defaults/core/profile/topological overrides/allowed CLI, then validate, resolve, canonicalize, fingerprint and freeze. | PLANNED | - | NOT_RUN | - | - |
| R026 | PR06 | configuration | FIX | Core unknown fields and missing references fail; IDs differ from display names; snapshots own stable text independent of JSON DOM. | PLANNED | - | NOT_RUN | - | - |
| R027 | PR06 | configuration | EXT | Typed simulation, presentation and execution hashes derive from canonical values. | PLANNED | - | NOT_RUN | - | - |
| R028 | PR06 | combat AI | DATA | Tactical AI profile moves without tuning to typed core content; runtime stores no parser/DOM. | PLANNED | - | NOT_RUN | - | - |
| R029 | PR07 | simulation | REF | One `SessionSimulationClock` and `TickContext` own tick, fixed dt and frequency; frame time remains presentation-only. | PLANNED | - | NOT_RUN | - | - |
| R030 | PR07 | simulation | REF | Seconds/RPM conversion uses one deterministic ceil-to-next-tick function and preserves 60 Hz behavior. | PLANNED | - | NOT_RUN | - | - |
| R031 | PR07 | gameplay | REF | `BattlefieldRuntime` owns authoritative ECS/infantry/physics/navigation/combat graph; scenario is a fixture of it. | PLANNED | - | NOT_RUN | - | - |
| R032 | PR07 | simulation | REF | Production phases declare complete reads/writes and have exactly one PhysicsWorld step and one weapon-to-damage pipeline. | PLANNED | - | NOT_RUN | - | - |
| R033 | PR07 | gameplay | FIX | Tick failure freezes runtime in Failed, blocks commit/future ticks and retains the last valid presentation snapshot plus diagnostic. | PLANNED | - | NOT_RUN | - | - |
| R034 | PR08 | scenes | REF | Product scenes move to `genomes::game_scenes`; engine runtime retains neutral lifecycle, transitions and snapshot protocol only. | PLANNED | - | NOT_RUN | - | - |
| R035 | PR08 | application | REF | Composition root owns catalogs/runtimes/backends/factories; application router owns product actions. | PLANNED | - | NOT_RUN | - | - |
| R036 | PR08 | runtime | FIX | Scene registration rejects duplicates and freezes before session; missing optional features return `UnavailableFeature`. | PLANNED | - | NOT_RUN | - | - |
| R037 | PR09 | world | REF | Neutral `world_core` owns IDs/coordinates/region/query/save/site request; generation moves to `world_generation`. | PLANNED | - | NOT_RUN | - | - |
| R038 | PR09 | world | FIX | `GridLayout` is the sole cells/samples/spacing/extent source; current map size must be divisible by 8 m. | PLANNED | - | NOT_RUN | - | - |
| R039 | PR09 | world | REF | One immutable `ResolvedWorldArtifacts` revision feeds render/collision/navigation/destruction. | PLANNED | - | NOT_RUN | - | - |
| R040 | PR09 | world render | REF | Mesh compiler consumes resolved building plans and `PartId` draw ranges; it never invokes the generator. | PLANNED | - | NOT_RUN | - | - |
| R041 | PR09 | world generation | REF | Each generation stage has its own SeedPath, version and dependency fingerprint. | PLANNED | - | NOT_RUN | - | - |
| R042 | PR10 | domain catalogs | DATA | Equipment, weapon/ammo, material, AI, world/building and appearance migrate in that order to typed frozen catalogs. | PLANNED | - | NOT_RUN | - | - |
| R043 | PR10 | infantry | FIX | Equipment size derives from data, capacity is validated, spawn resolves WeaponId through the shared catalog, and loaded text owns storage. | PLANNED | - | NOT_RUN | - | - |
| R044 | PR11 | infantry | REF | Model compiler accepts a canonical immutable request and returns shared immutable artifact plus key without preview state. | PLANNED | - | NOT_RUN | - | - |
| R045 | PR11 | proc cache | FIX | Artifact cache requires deep memory usage, distinguishes retained/shared/pinned bytes and does not cache oversize artifacts. | PLANNED | - | NOT_RUN | - | - |
| R046 | PR11 | Unit Lab | FIX | Latest-wins controller keeps one active and one replaceable pending request; revision/request gates publication and scene exit drains. | PLANNED | - | NOT_RUN | - | - |
| R047 | PR12 | UI/application | REF | RmlUi, CLI and tests parse at their boundaries into the same typed command variants; numeric parsing is strict and diagnosed. | PLANNED | - | NOT_RUN | - | - |
| R048 | PR12 | Unit Lab | REF | View model tracks Geometry/Material/Pose/Presentation/UI dirtiness and only rebuilds affected outputs. | PLANNED | - | NOT_RUN | - | - |
| R049 | PR12 | UI/presentation | FIX | Camera, picking and capture use current RmlUi viewport metrics or the last valid rectangle. | PLANNED | - | NOT_RUN | - | - |
| R050 | PR13 | render ABI | REF | Existing 69-bone/four-influence/four-morph ABI is named and versioned `SkinnedLayoutProfileV1` in shared C++/HLSL definitions. | PLANNED | - | NOT_RUN | - | - |
| R051 | PR13 | presentation | FIX | GPU upload retains immutable artifact ownership through submission, retires by fence and publishes revisions atomically after all resources succeed. | PLANNED | - | NOT_RUN | - | - |
| R052 | PR14 | build | REF | Preset matrix, target visibility, self-contained headers, minimal consumers, structural config/RML guards and fixture manifest enforce boundaries. | PLANNED | - | NOT_RUN | - | - |
| R053 | PR15 | appearance | EXT | Manual `inspection-olive` is a validated data-only presentation preset and Material-only Unit Lab command. | PLANNED | - | NOT_RUN | - | - |
| R054 | PR16 | performance | REF | Only measured optimizations ship; execution tuning preserves D1/D2 results and meets the stated target/p95 acceptance gate. | PLANNED | - | NOT_RUN | - | - |
| R055 | PR17 | architecture | REF | Remove only proven-unused bridges/fallbacks, close dependency review, and assign every R row a final disposition. | PLANNED | - | NOT_RUN | - | - |

Kinds: `REF` preserves behavior/contracts while moving ownership, `FIX` requires
a counterexample regression, `DATA` requires old/new parity, and `EXT` creates a
new explicitly versioned capability or contract.

## Package register

| Package | Scope | Decisions | Implementation | Code SHA | Verification | Verified SHA |
|---|---|---:|---|---|---|---|
| PR00 | Baseline, tracker and source map | R001-R055 | CODE_READY | WORKTREE | NOT_RUN | - |
| PR01 | Jobs and SystemGraph lifetime | R001-R004 | PLANNED | - | NOT_RUN | - |
| PR02 | Infantry state and Unit Lab input | R005-R008 | PLANNED | - | NOT_RUN | - |
| PR03 | Building generator contract | R009-R013 | PLANNED | - | NOT_RUN | - |
| PR04 | Bounded canonical save | R014-R018 | PLANNED | - | NOT_RUN | - |
| PR05 | Packages and native plugins | R019-R023 | PLANNED | - | NOT_RUN | - |
| PR06 | Typed profile infrastructure | R024-R028 | PLANNED | - | NOT_RUN | - |
| PR07 | Session clock and combat pipeline | R029-R033 | PLANNED | - | NOT_RUN | - |
| PR08 | Product scenes outside runtime | R034-R036 | PLANNED | - | NOT_RUN | - |
| PR09 | World core and resolved artifacts | R037-R041 | PLANNED | - | NOT_RUN | - |
| PR10 | Domain catalog migration | R042-R043 | PLANNED | - | NOT_RUN | - |
| PR11 | Model compiler, cache, latest-wins | R044-R046 | PLANNED | - | NOT_RUN | - |
| PR12 | Unified typed command path | R047-R049 | PLANNED | - | NOT_RUN | - |
| PR13 | Versioned GPU profile and ownership | R050-R051 | PLANNED | - | NOT_RUN | - |
| PR14 | CMake, presets, guards and hygiene | R052 | PLANNED | - | NOT_RUN | - |
| PR15 | Data-only feature pilot | R053 | PLANNED | - | NOT_RUN | - |
| PR16 | Measurement-led optimization | R054 | PLANNED | - | NOT_RUN | - |
| PR17 | Migration closure | R055 | PLANNED | - | NOT_RUN | - |

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
