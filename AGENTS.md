# Genomes C++ Agent Guide

## Goal

Work with the smallest useful context, the smallest coherent diff, and the
smallest validation scope.

This is a C++20 project using CMake + Ninja. Genomes-owned code is compiled
with strict warnings and warnings-as-errors by default.

Production presentation is Diligent/D3D12. Renderer-independent work should
remain usable in HEADLESS profiles.

## Repository map

- `engine/` — reusable renderer-independent engine infrastructure.
- `modules/` — game/domain modules such as infantry, buildings, combat,
  destruction, ballistics, weapons, roads and hydrology.
- `apps/` — executable composition roots and development applications.
- `tests/native/` — native tests and source/architecture guards.
- `reference/` — normative manifests, schemas and pinned validation fixtures.
- `docs/` — architecture, ADRs, migration notes and implementation contracts.
- `external/` — pinned third-party submodules.
- `tools/` — validation, reference and benchmark tooling.

## Context and navigation

Minimize repository reads.

1. Use CodeGraph first for:
   - symbol discovery,
   - callers/callees,
   - inheritance and dependencies,
   - impact analysis,
   - locating the implementation behind an interface.

2. Use text search only when appropriate:
   - exact strings,
   - macros,
   - CMake options/targets,
   - config values,
   - log messages,
   - comments or documentation.

3. After locating a symbol, read only the smallest useful source range.
   Do not read an entire large file when a function/class-sized range is enough.

4. Before changing a public interface, use CodeGraph to inspect downstream
   callers and relevant tests.

5. Inspect the nearest `CMakeLists.txt` only when target ownership or
   dependencies are unclear.

Do not perform broad recursive repository scans unless necessary.

### Expensive paths

Never read or search `concat.txt` unless the user explicitly requests it.

Do not recursively inspect:

- `build/`
- `external/`
- large `reference/fixtures/` trees

unless the task specifically requires them.

`external/` contains pinned submodules. Treat them as third-party source, not
normal Genomes code.

Use an existing `compile_commands.json` when useful for C++ semantic analysis.
Do not create or reconfigure a build tree solely for code discovery.

## Build profile selection

Use committed CMake presets. Do not invent a new build configuration unless the
task requires one.

Prefer the cheapest profile compatible with the change:

- `headless-core-debug`
  - default for renderer-independent engine, simulation and CPU/domain work,
  - no Diligent, SDL, RmlUi, assets or CSG,
  - tests enabled,
  - benchmarks disabled.

- `headless-infantry-upgrade`
  - renderer-independent infantry work.

- `headless-infantry-meshopt`
  - infantry work specifically involving meshoptimizer preparation.

- `dev-debug`
  - SDL/Diligent/RmlUi, windowed game or production presentation work.

- `dev-diligent`
  - Diligent production-candidate work without meshoptimizer.

- `toolkit-full-debug`
  - only when assets, CSG or full lightweight-toolkit functionality is needed.

Do not use Release builds, benchmarks, GPU acceptance or the full test suite as
the first feedback loop.

## Build and test workflow

Unless the user explicitly asks the agent to execute builds/tests, the user
performs configure, compilation, CTest, benchmarks and GPU acceptance.

The agent should prepare exact commands, code changes and regression tests.

When execution is requested or available:

1. Do not reconfigure if the existing build tree is compatible.
2. Build only the affected target first.
3. Run only directly relevant tests first.
4. Expand validation only when the change justifies it.

Preferred commands:

```text
cmake --preset <preset>

cmake --build --preset <preset> --target <target>

ctest --preset <preset> -R <test-regex> --output-on-failure
```

Use:

```text
ctest --preset <preset> -N
```

when the exact CTest name is unknown.

For the complete windowed development application, the repository also provides:

```powershell
.\run-dev.ps1 -BuildOnly
```

Prefer a direct target build over `run-dev.ps1` when only one library/test target
needs validation.

Run the full CTest suite only after targeted tests pass, or when the change is
cross-cutting.

## Terminal output and token usage

Keep tool output compact.

When RTK or another compact-output wrapper is available, prefer it for verbose
build, test, git and search commands.

On failure:

1. identify the first actionable compiler/test diagnostic,
2. inspect only the relevant surrounding output,
3. follow secondary errors only if the first error does not explain the failure.

Do not paste or repeatedly read complete build logs.

`build-error.log` may contain diagnostics from the most recent windowed build;
inspect the relevant failure region rather than loading the whole file when it
is large.

## C++ rules

- Target C++20 unless an explicit toolchain change permits otherwise.
- Keep C++ extensions disabled.
- Preserve existing warning cleanliness.
- Prefer RAII, explicit ownership and value semantics.
- Avoid unnecessary allocations in hot paths.
- Avoid new abstractions unless they remove a concrete duplication or enforce a
  required boundary.
- Do not perform unrelated cleanup while implementing a focused task.
- Preserve existing public APIs unless changing them is required by the task.
- Add/update focused tests for behavioral changes.

For performance work, measure before introducing complexity. Do not add CUDA,
SIMD specialization, caching or parallelism merely because it may be faster.

## Architecture rules

Keep renderer-independent engine/domain code independent of Diligent and SDL.

Diligent types must not leak through generic renderer APIs.

Preserve:

- deterministic simulation behavior,
- stable IDs,
- deterministic seed derivation,
- authoritative semantic data,
- immutable published artifacts where that contract already exists,
- existing module boundaries.

Reference JavaScript is evidence for behavior and parity. It does not define the
native runtime architecture.

Do not replace working architecture with a new framework while solving a local
task.

## Infantry / renderer migration constraints

For changes involving infantry rendering, presentation architecture or graphics
dependencies, first read only these current decision/status documents:

- `docs/decisions/ADR_LIGHTWEIGHT_TOOLKIT.md`
- `docs/migration/lightweight-toolkit/POSTEP.txt`
- `docs/decisions/ADR_INFANTRY_STACK.md`
- `docs/upgrades/infantry-stack/STATUS.txt`

The historical threepp production-renderer path is superseded.

Diligent/D3D12 is the production GPU target. Do not reintroduce threepp,
GLFW or OpenGL as production dependencies.

Keep the existing infantry module.

For the current infantry upgrade work:

- do not start Creature/Humanoid extraction,
- do not replace the ECS,
- do not introduce Magnum, Jolt, Recast or Ozz as part of that work,
- Manifold is allowed only behind the optional CSG boundary defined by the
  toolkit ADR,
- upgrade working pieces instead of restarting the migration,
- do not construct an anatomical human from disconnected library primitives.

Preserve JS fixtures, generator outputs and raw semantic vertex numbering.

Meshoptimizer preparation belongs at the presentation boundary and is performed
once per immutable prototype.

Current meshoptimizer scope is triangle ordering within opaque draw ranges only.

Do not introduce as part of that scope:

- vertex remapping,
- welding,
- quantization,
- LOD generation,
- transparent draw reordering.

Do not claim threepp/GLFW/OpenGL removal until both the static guard and
dependency-closure review support that claim.

Do not delete Diligent or blindly switch presets onto known-broken backend code.

Do not undo the owner's `66843b5` toolchain fixes while reducing the Diligent
dependency.

The old TP roadmap percentages are historical and must not be presented as
completion of the current hybrid/migration work.

## Tests and guards

`tests/native/` contains both executable tests and CMake source/architecture
guards.

Prefer the narrowest relevant CTest regex.

Examples of test namespaces already used by the repository include:

- `architecture.*`
- `camera.*`
- `geometry.*`
- `infantry.*`
- `simulation.*`
- `world.*`
- `ui.*`

When changing a dependency boundary, public header or module composition,
inspect the relevant architecture/source guards as well as runtime tests.

## Git and submodule safety

Preserve concurrent user changes.

Never:

- reset unrelated work,
- force-push,
- overwrite unrelated files,
- silently stage unrelated changes,
- absorb local submodule modifications,
- replace a pinned submodule checkout with another dependency source.

Do not commit or publish changes unless requested.

Before a requested publication:

1. refresh/re-read the current main branch state,
2. preserve concurrent changes,
3. use non-forced fast-forward-safe updates,
4. publish a complete coherent slice.

## Working loop

For a normal implementation task:

1. Identify relevant symbols with CodeGraph.
2. Inspect callers, dependencies and focused tests.
3. Read only required source ranges.
4. Make the smallest coherent change.
5. Add/update the smallest useful regression test.
6. Select the cheapest compatible CMake preset.
7. Build the affected target if execution was requested.
8. Run targeted tests if execution was requested.
9. Expand validation only when justified.
10. Report exactly what remains unverified.

Do not spend context reconstructing repository architecture when CodeGraph or the
existing CMake target graph already answers the question.

## Reporting

Never claim that a build, test, benchmark, GPU capture or acceptance run passed
without direct evidence from that run.

If validation was not executed, say so explicitly.

For completed work report concisely:

- files/areas changed,
- behavior changed,
- targeted validation performed or recommended,
- remaining unverified items.

For multi-part work, progress may be reported as `completed/total`; keep
implementation progress separate from verification status.
