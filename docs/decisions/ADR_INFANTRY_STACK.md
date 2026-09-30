# ADR: upgrade infantry on Diligent with CPU geometry providers

Status: accepted infantry constraints; renderer/toolkit composition superseded
by `ADR_LIGHTWEIGHT_TOOLKIT.md`, 2026-09-30.
Supersedes `ADR_THREEPP_PRESENTATION.md` as the final renderer decision.
Implementation baseline: `66843b5eae186b416c140c5e6157c1ea64477fc8`.

## Scope

Keep C++20, CMake, SDL3, RmlUi, native jobs/caches and the existing infantry
module. Diligent is the production GPU target. Shared CPU math/geometry/camera
utilities come from the lightweight toolkit; no renderer or third-party scene
graph is part of the infantry domain.
No Creature/Humanoid extraction or new physics/navigation framework is in scope.

## Preserved reference implementation

The lightweight toolkit replaces shared math, geometry, camera, and presentation
utilities. It does not replace the infantry reference geometry, rig, morph
targets, parity algorithms, fixtures, tolerances, semantic vertex numbering, or
generator outputs. General-purpose primitives must not be composed into a new
anatomical human. Presentation optimization remains outside the domain generator
and must preserve every reference vertex stream and identity.

## First delivered slice

Separate the Genomes threepp renderer target from enabling its dependency.
Add pinned meshoptimizer 1.3 (`9e1f07b159d3cb777f1c67ed31fc11fd117986f4`)
behind `GENOMES_ENABLE_MESHOPTIMIZER`, initially OFF to preserve existing builds.
Use it at the completed skinned presentation prototype boundary, not in the
reference/domain generator and not during per-frame animation.

Only vertex-cache triangle ordering is enabled in this slice. Ranges are
validated and never crossed. Blend, partial material opacity, partial vertex
alpha, per-instance tint (unknown alpha) and unspecified material semantics preserve their original order.
Vertices are NOT reordered. Weights, morph deltas, UV seams, face tags and all
raw domain vertex IDs therefore retain their exact meaning. No simplification,
welding, quantization, remeshing or overdraw optimizer is enabled.

`ReferenceOrder` remains available for differential tests. The cache/resource
identity includes the pinned algorithm/policy fingerprint. Candidate construction
runs outside the shared cache lock; publication chooses one equivalent owner.
If optional preparation rejects input it logs the reason and returns the current
reference-order model, not stale geometry or a half-written buffer. It is not
cached as a successfully optimized mesh.

## Current lightweight-toolkit boundary

The renderer migration is now represented by `ADR_LIGHTWEIGHT_TOOLKIT.md`.
`dev-debug` and `dev-release` select the Diligent/D3D12 profile; `HEADLESS` is
the CPU-only profile. The production application has no threepp provider or GL
context path. This ADR still governs the infantry domain: reference geometry,
rigs, morphs, fixtures, tolerances, semantic vertex numbering and generator
outputs remain unchanged.

Source readiness is not runtime acceptance. Configure, build, CTest, benchmark,
shader, GPU and visual evidence must be supplied by the user at an exact SHA.

## References

- Source baseline: https://github.com/ppotepa/genomes-cpp/tree/66843b5eae186b416c140c5e6157c1ea64477fc8
- Pinned meshoptimizer: https://github.com/zeux/meshoptimizer/tree/9e1f07b159d3cb777f1c67ed31fc11fd117986f4
- API/pipeline documentation: https://meshoptimizer.org/
- Uses meshoptimizer. Copyright (c) 2016-2026, Arseny Kapoulkine.
  MIT license retained at `external/meshoptimizer/LICENSE.md`.
