# Genomes C++

Final native C++ implementation of Genomes, informed by the earlier prototype.

## Repository roles

**TARGET:** `ppotepa/genomes-cpp`

This repository contains:

- native C++ engine and game implementation,
- executable implementation manual under `docs/`,
- minimal reference-validation project under `reference/`,
- native tests, benchmarks and CI.

**REFERENCE EVIDENCE:** `ppotepa/genomes`

The JavaScript repository supplies prototypes, test scenarios, research and
calibration evidence. It does not define native runtime architecture or require
legacy compatibility modes.

## Manual

Before continuing the infantry translation, read the
[2026-09-28 diagnostic audit](docs/reports/INFANTRY_TRANSLATION_AUDIT_2026-09-28.md).
It records known geometry, deformation and validation gaps; passing native
invariant tests does not yet establish Infantry Lab parity.

Start with:

- `docs/0.0_NATIVE_MANUAL_INDEX.txt`
- `docs/0.1_TARGET_ARCHITECTURE.txt`
- `docs/TARGET_IMPLEMENTATION_CONTRACT.txt`
- `docs/SOURCE_PROTOTYPE_AND_MOCKUP_CONTRACT.txt` (non-normative evidence audit)
- `docs/architecture/TARGET_ARCHITECTURE.svg`
- `docs/architecture/TARGET_ARCHITECTURE.drawio.xml`
- `docs/0.6_REPOSITORY_TOPOLOGY_AND_REFERENCE_PROJECT.txt`
- `docs/MANUAL_STATUS.txt`

Implementation is chapter-driven. A command such as:

`Implement docs/1.1_CPP_CMAKE_BOOTSTRAP.txt in this repository.`

should be sufficient for an implementation agent to execute one verified step.

## Reference project

`reference/` is not a mirror of the private legacy repository.

It contains only versioned manifests, schemas, fixtures and selected reference
outputs used to validate TARGET requirements. Every imported artifact records
its provenance, but reference data cannot select production algorithms.

## Core technology direction

- C++20 initially, with selective C++23 adoption only after toolchain baseline changes
- Diligent Engine for rendering/RHI
- Jolt Physics behind a Genomes physics abstraction
- Recast/Detour behind a Genomes navigation abstraction
- custom data-oriented simulation and JobSystem
- generic GPU compute through Diligent
- optional CUDA fast paths only where benchmarks justify them

## Current native slice

The repository now contains the first renderer-independent vertical slice:

- `genomes_foundation` provides stable IDs and build information;
- `genomes_runtime` provides scene lifecycle, command-based transitions and the
  main-menu and deterministic world-configuration scenes;
- `genomes_simulation` provides a fixed 60 Hz clock with rational accumulation,
  bounded catch-up and dropped-time accounting, plus an `EntityStore` facade
  backed by the authoritative archetype/chunk SoA storage;
- `genomes_simulation` also provides integer tick cadence policies with stable
  per-entity phase staggering and authoritative relevance tiers;
- `genomes_simulation` now includes generational archetype/chunk storage,
  compiled required/optional/excluded queries and structural component moves;
- `genomes_simulation` also exposes a declarative `SystemGraph` with fixed
  simulation phases, read/write hazard edges, cycle diagnostics and shared
  JobSystem execution;
- `genomes_simulation` provides append-only local structural command buffers
  with semantic ordering, create tokens, conflict handling and deterministic
  commit into `WorldEcs`; `SystemGraph` assigns one exclusive buffer per
  system invocation and exposes it through `SystemContext`;
- `genomes_jobs` provides one shared sleeping worker pool, completion handles,
  worker-assisted waits and drain/cancel shutdown modes;
- `genomes_jobs` also exposes stable `BatchRange`/`parallelFor` partitioning
  so bulk systems share one deterministic batching contract;
- `genomes_proc` provides versioned `SeedPath` derivation and deterministic PCG32
  streams, immutable-after-freeze generator registration and a thread-safe,
  bounded typed immutable-artifact cache with LRU-style eviction;
- `genomes_world` and `genomes_terrain` provide renderer-independent world
  coordinates, stable region IDs, a deterministic semantic `WorldPlan`, a
  separate `CityPlan`/`CityGenerator`, worker-backed world generation tasks
  and an authoritative sampled height field; `WorldRegionStreamer` adds the
  region ownership, immutable artifact cache and ready-result boundary used by
  the battlefield scene;
  `WorldQuerySnapshot` adds immutable multi-region AABB/segment queries with
  explicit Complete/PartialUnloaded/Missing status, terrain sampling and
  stable candidate deduplication;
  `TerrainMeshBuilder` converts sampled terrain into budgeted indexed geometry
  without coupling terrain data to Diligent;
- `genomes_roads` publishes immutable semantic road graphs with stable node and
  edge IDs, canonical centerline endpoints, compact adjacency and region
  translation support; city generation feeds this graph instead of exposing
  render-only road boxes;
- `genomes_hydrology` publishes deterministic optional/forced river paths,
  raster water masks and O(1) water-distance queries as an independent world
  artifact;
- `genomes_physics` exposes solver-neutral bodies, queries and command buffers
  with a deterministic CPU fallback;
- `genomes_navigation` exposes the pathfinding boundary and a deterministic CPU
  grid backend, ready to be replaced by a tiled Recast adapter; the battlefield
  compiles building footprints into blocked cells and infantry keeps a
  bounded, refreshable route through this interface;
- `genomes_compute` exposes a backend-neutral dispatch API whose CPU path uses
  the shared JobSystem;
- `genomes_spatial` provides a deterministic uniform-grid broad phase rebuilt
  from the authoritative entity store; infantry perception uses it instead of
  scanning every live unit for every observer;
- `genomes_infantry` is the first domain module: genome parameters, 5 Hz local
  perception, target selection and data-oriented movement are built on the
  engine entity store; perception batches are dispatched through the shared
  JobSystem when a worker pool is available;
- `genomes_combat` owns the ordered `DamageBuffer` commit phase, so future
  ballistics, weapons and destruction can produce events without mutating
  simulation state from worker jobs;
- `genomes_destruction` provides immutable material catalogs, validated material
  frames and layered physical-solid assemblies with deterministic material
  fields; its stateless impact solver accounts for ordered layer work,
  relative target motion, residual energy, impulses and bounded traversal;
- the destruction module also keeps persistent mixed-material rubble tiles,
  deterministic slope relaxation, dirty-tile batching and a physics adapter
  that stages hero-body, impulse and static-rubble replacement commands;
- `genomes_destruction` emits revisioned, region-partitioned dirty bounds for
  navigation, static queries, physics and presentation, with retryable
  consumer queues and deterministic local coalescing;
- `genomes_buildings` establishes the semantic `BuildingSpec → BuildingPlan →
  BuildingRuntime` contract with stable part IDs, rooms and local damage state;
- the world pipeline publishes a neutral `BuildingSiteRequest`; the buildings
  module resolves it into a `BuildingPlan` plus `BuildingSiteResolution`
  (footprint, entrance and clearance) before mesh compilation;
- `genomes_world_render` compiles world features and procedural building parts
  into an immutable presentation mesh, keeping the world plan and renderer
  backend independent;
- `genomes_weapons` emits cadence-controlled, deterministic shot requests;
- `genomes_ballistics` traces segments and projectiles only through the generic
  physics query boundary, leaving Jolt/CPU solver choice replaceable;
- `genomes_ballistics` also owns versioned projectile state, immutable
  ammunition strategies/catalogs, typed impact transitions, RK4/adaptive flight
  and an optional bounded semantic trace observer;
- `genomes_ballistics` also exposes solver-backed contact resolution and armed-
  contact HE detonation with blast commands, deterministic opposite-pair
  fragments, inherited velocity and whole-pair capacity ledgers;
- `genomes_ballistics` now provides generational chunked projectile storage,
  stable event-key sorting, batched RK flight evaluation over shared worker
  ranges and ordered world segment queries; the scalar integrator remains the
  semantic reference path;
- `genomes_infantry` now owns an explicit module registration boundary and a
  deterministic genome-to-body/face phenotype artifact with named seed paths,
  override diagnostics and equipment-independent cache identity; the runtime
  can be built with `GENOMES_ENABLE_INFANTRY=OFF`;
- `genomes_infantry` keeps perception spatially indexed and cadence-controlled,
  parallelizes observer batches when workers are available and retains a
  deterministic last-contact memory for short-lived target loss; squad
  contacts are published after the parallel phase and shared on the next
  perception pass without racing agent state;
- `genomes_ui` describes menu documents without owning a platform window;
- `genomes_render` consumes a world presentation snapshot together with the UI
  document through a renderer-independent boundary;
- `genomes_render_core` defines immutable-after-publish presentation metadata,
  a bounded non-blocking `SnapshotExchange` and revision-based
  Added/Updated/Removed extraction; `SceneDirector` publishes through this
  exchange while retaining a compatibility mirror for headless tools;
- `genomes_render_graph` compiles declared render-resource hazards into a
  deterministic pass order with culling, transient lifetimes, alias slots,
  barriers and a DOT diagnostic dump;
- renderer core also exposes Genomes-owned generational resource handles,
  typed backend-neutral resource descriptors, frozen capability fields and a
  fence-keyed deferred-release queue;
- shader/pipeline asset contracts now produce deterministic keys from source,
  include provenance, defines, compiler/backend versions and binding layouts;
- `genomes_render_gpu_scene` keeps a persistent CPU shadow of render instances,
  maps stable semantic IDs to generational slots and emits coalesced dirty
  upload ranges; the Diligent backend now stages those ranges into a persistent
  structured instance buffer and consumes it in a procedural instanced preview
  and dynamic-actor pipeline for the main-menu/world-configuration and
  battlefield scenes; scenes publish immutable vertex/index prototypes and the
  pass batches by mesh/material through a compact slot-remap buffer instead of
  synthesizing one CPU mesh for every unit;
- `genomes_platform` owns the SDL3 window and translates SDL events into the
  engine's semantic input frame;
- `genomes_render_diligent` is an opt-in Vulkan adapter for headless and
  windowed presentation. It is built only with `GENOMES_ENABLE_DILIGENT=ON`,
  owns the first terrain/semantic-world/infantry 3D pass, depth buffer and
  UI/world-map debug passes, advertises instanced-rendering capability to
  scene extraction and never leaks Diligent types through the generic renderer
  API;
- `genomes_headless` verifies the native bootstrap contract and exercises one
  archetype/query path without opening a window;
- `genomes_menu` exercises the main menu with a headless renderer and semantic
  input frames;
- `genomes_game` is the first SDL3 + Vulkan windowed vertical slice, drawing the
  engine-owned main-menu scene (including text and selection state), the unit
  laboratory, the building laboratory with runtime part damage, the world
  configuration scene and a generated battlefield plan after `Start`; the
  `--unit-lab`, `--building-lab` and `--battlefield` arguments select scenes for
  smoke tests and focused development.

Configure and build it with the committed presets:

```text
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug --output-on-failure
```

The Diligent and SDL dependencies remain opt-in so that headless CI and
simulation work do not require a GPU SDK. The convenience scripts configure,
incrementally rebuild and launch the windowed game:

```powershell
.\rundev.ps1
.\run-release.ps1
```

The matching `rundev.cmd` and `run-release.cmd` wrappers provide the same
commands from `cmd.exe`.

Use `-BuildOnly` to configure and build without launching, or `-NoBuild` to
launch an already-built executable. The first build compiles the pinned
third-party sources and can take several minutes; subsequent runs are
incremental.

To configure the Diligent adapter without the SDL window:

```text
cmake -S . -B build/diligent -G Ninja \
  -DGENOMES_ENABLE_DILIGENT=ON \
  -DGENOMES_BUILD_TESTS=OFF
cmake --build build/diligent --target genomes_render_smoke
```
