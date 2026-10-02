# Genomes C++

Genomes is a native C++20 implementation of a deterministic procedural game
runtime. The repository contains the engine, domain/gameplay modules, native
applications, tests, benchmarks, reference fixtures and the design documents
used to evolve the implementation.

The project is under active development. The code in this repository is the
runtime source of truth. The older JavaScript prototype is retained only as
reference evidence for selected behavior, fixtures and visual comparisons.

## Current shape

The codebase is split into a renderer-independent core and optional presentation
adapters.

- Engine code lives in `engine/`.
- Game/domain systems live in `modules/`.
- Executables and development tools live in `apps/` and `tools/`.
- Native validation lives in `tests/native/` and `benchmarks/`.
- The production presentation path is Diligent + D3D12.
- Headless profiles build core simulation/domain code without the graphics stack.
- Physics and navigation expose backend-neutral interfaces with deterministic CPU
  implementations; Jolt/Recast are target adapters described by the design
  manual, not current production dependencies.

The numbered files under `docs/` describe architecture, contracts and the
implementation roadmap. They are not a substitute for checking the current
source tree: a documented target or a `READY` chapter does not imply that the
corresponding production implementation is complete.

## Requirements

Common development requirements:

- Git with submodule support
- CMake 3.21+
- Ninja
- a C++20 compiler

For the current Windows windowed build you also need the Windows SDK shader
tools used by the D3D12/Diligent path.

Node.js is only required for the optional JavaScript reference-parity workflow.

Initialize pinned third-party dependencies before the first build:

```bash
git submodule update --init --recursive
```

Third-party revisions are committed as git submodules under `external/`.
Normal CMake configuration does not replace them with arbitrary downloaded
versions.

## Quick start

### Headless development

Use the headless preset for engine, simulation and most domain work that does
not require the renderer:

```bash
cmake --preset headless-core-debug
cmake --build --preset headless-core-debug
ctest --preset headless-core-debug --output-on-failure
```

For a faster edit/build/test loop, build the affected target and run only the
relevant tests:

```bash
cmake --build --preset headless-core-debug --target <target>
ctest --preset headless-core-debug -R <test-regex> --output-on-failure
```

List available tests with:

```bash
ctest --preset headless-core-debug -N
```

### Windows game

The normal windowed development profile uses SDL3 and Diligent/D3D12:

```powershell
.\run-dev.ps1
```

Useful variants:

```powershell
.\run-dev.ps1 -BuildOnly
.\run-dev.ps1 -NoBuild
.\run-release.ps1
```

The game executable also accepts focused development scene arguments such as
`--unit-lab`, `--building-lab` and `--battlefield`.

Equivalent explicit CMake commands are:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --target genomes_game
ctest --preset dev-debug --output-on-failure
```

A failed windowed configure/build launched through the repository script writes
compiler/configuration diagnostics to `build-error.log`.

## CMake presets

`CMakePresets.json` is the authoritative list. The presets most commonly used
during development are:

| Preset | Intended use |
| --- | --- |
| `headless-core-debug` | Renderer-independent engine/domain development and tests |
| `headless-infantry-upgrade` | Headless infantry work |
| `headless-infantry-meshopt` | Headless infantry work with meshoptimizer enabled |
| `dev-debug` | Windowed SDL3 + Diligent/D3D12 development |
| `dev-diligent` | Diligent-focused debug build without meshoptimizer |
| `toolkit-full-debug` | Optional assets/CSG/toolkit functionality |

Release, benchmark and GPU-acceptance configurations should normally be used
after the focused debug loop, not as the first validation step.

## Repository layout

| Path | Purpose |
| --- | --- |
| `engine/` | Core math, geometry, jobs, simulation, spatial queries, runtime, world, terrain, rendering interfaces and platform abstractions |
| `modules/` | Domain/gameplay modules: infantry, combat, ballistics, weapons, buildings, destruction, roads, hydrology and world presentation |
| `apps/` | `genomes_game`, headless bootstrap, menu, procedural viewer and render smoke executables |
| `tests/native/` | C++ tests plus CMake source/architecture guards |
| `benchmarks/` | Focused CPU/render/domain benchmark programs |
| `config/` | Versioned runtime/performance configuration and baseline data |
| `shaders/` | Project shader assets |
| `reference/` | Pinned schemas, fixtures, manifests and selected legacy/reference evidence |
| `docs/` | Architecture, decisions, implementation manual, migration notes and validation reports |
| `external/` | Pinned third-party git submodules |
| `tools/` | Validation, reference export and benchmark support tools |

## Architecture

The project keeps semantic state separate from presentation and backend-specific
objects.

Core design rules:

- deterministic IDs, seed derivation and fixed-step simulation are owned by
  renderer-independent code;
- domain modules depend on engine contracts rather than a graphics API;
- Diligent objects stay behind the render adapter;
- published generation/presentation data is passed through explicit artifacts,
  snapshots or command boundaries;
- parallel work uses shared job-system contracts and deterministic commit/order
  rules where simulation semantics depend on ordering;
- reference JavaScript can supply test evidence, but it does not define native
  module boundaries or production algorithms.

Important implementation areas include:

- `engine/simulation` — ECS/storage, scheduling and fixed-step simulation;
- `engine/jobs` — shared worker/job infrastructure;
- `engine/proc` — deterministic generation utilities and artifact caching;
- `engine/world`, `engine/terrain` — semantic world data and streaming/query
  boundaries;
- `engine/render` — backend-neutral presentation, render graph and GPU-scene
  contracts;
- `engine/render/diligent` — Diligent/D3D12 presentation adapter;
- `modules/infantry` — infantry genome/phenotype, appearance, equipment,
  animation, IK, damage and ragdoll-facing contracts;
- `modules/combat`, `modules/weapons`, `modules/ballistics` — combat
  command flow, weapon behavior and projectile simulation;
- `modules/buildings`, `modules/destruction` — semantic building and damage
  systems.

For deeper architectural navigation, see [docs/README.md](docs/README.md).

## Tests and validation

CTest is the primary native test entry point.

Prefer focused validation while developing:

```bash
ctest --preset headless-core-debug -R infantry --output-on-failure
ctest --preset headless-core-debug -R world --output-on-failure
ctest --preset headless-core-debug -R architecture --output-on-failure
```

`tests/native/` also contains source-level guards for dependency and architecture
boundaries. A runtime test passing does not replace those guards when changing
public headers, module ownership or dependency direction.

Benchmarks live under `benchmarks/` and are enabled only in presets that request
them. Performance changes should be compared against an appropriate benchmark
rather than inferred from implementation complexity.

## Reference material

`reference/` contains small, versioned validation artifacts and selected source
evidence. It is not the production runtime and should not be treated as a second
implementation.

In particular, `reference/code/` contains prototype/reference material. Its own
README describes that prototype and may intentionally refer to JavaScript,
Three.js or historical version numbers.

## Documentation

Start with:

- [Documentation map](docs/README.md)
- [Target implementation contract](docs/TARGET_IMPLEMENTATION_CONTRACT.txt)
- [Target architecture](docs/0.1_TARGET_ARCHITECTURE.txt)
- [Coding and dependency rules](docs/0.3_CODING_AND_DEPENDENCY_RULES.txt)
- [Native manual index](docs/0.0_NATIVE_MANUAL_INDEX.txt)
- [Agent/development guide](AGENTS.md)

Current renderer/infantry migration decisions are recorded separately under
`docs/decisions/`, `docs/migration/lightweight-toolkit/` and
`docs/upgrades/infantry-stack/`.

Historical migration documents remain in the repository for context. They
should not be assumed to describe the current production path.
