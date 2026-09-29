# ADR: upgrade infantry on Diligent with CPU geometry providers

Status: accepted direction from the owner's clarification, 2026-09-30.
Supersedes `ADR_THREEPP_PRESENTATION.md` as the final renderer decision.
Implementation baseline: `66843b5eae186b416c140c5e6157c1ea64477fc8`.

## Scope

Keep C++20, CMake, SDL3, RmlUi, native jobs/caches and the existing infantry
module. Diligent is the production GPU target. threepp is an optional supplier
of CPU geometry/tools hidden behind the existing geometry adapter. Its GL
renderer remains available only through the explicit experimental profile.
No Creature/Humanoid extraction or new physics/navigation framework is in scope.

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

## Explicit limits

This slice does not restore the entire Diligent adapter or switch the existing
dev-debug/dev-release presets, which still explicitly select THREEPP_GL. The
read source still contains a dynamic-buffer persistence defect in Diligent's
world pass. Reactivation, material/camera/UI/capture integration and final
preset cutover require a separate coherent renderer slice and user acceptance.

The existing threepp build remains broad. Separating a target is not proof of
an OpenGL/GLFW-free link closure. No speed/FPS or visual-parity claim is made.

## References

- Source baseline: https://github.com/ppotepa/genomes-cpp/tree/66843b5eae186b416c140c5e6157c1ea64477fc8
- Pinned meshoptimizer: https://github.com/zeux/meshoptimizer/tree/9e1f07b159d3cb777f1c67ed31fc11fd117986f4
- API/pipeline documentation: https://meshoptimizer.org/
- Uses meshoptimizer. Copyright (c) 2016-2026, Arseny Kapoulkine.
  MIT license retained at `external/meshoptimizer/LICENSE.md`.
