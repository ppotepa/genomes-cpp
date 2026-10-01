# ADR: lightweight Genomes CPU toolkit with Diligent/D3D12

Status: accepted, 2026-09-30.

## Decision

Genomes owns a small backend-neutral CPU toolkit split into `genomes::math`,
`genomes::geometry`, and `genomes::camera`. Optional `genomes::assets` imports
bounded static glTF data; optional CSG uses Manifold. Diligent is the sole owner
of GPU resources and D3D12 is the production backend on Windows. HEADLESS is
the only non-rendering backend in this work package. Linux/Vulkan is neither a
gate nor a promised result.

threepp and its GL renderer/provider are removed completely. They are not
replaced by another monolithic framework. meshoptimizer, MikkTSpace, and earcut
are private geometry implementation dependencies. fastgltf and Manifold are
opt-in and cannot enter the default game's link closure.

## Public contracts

Coordinates are right-handed, +Y up, with metres and radians. Matrices are
column-major and multiply column vectors. Projection depth follows D3D `[0,1]`.
Public headers expose no Diligent or third-party toolkit types.

Geometry is neutral SoA data with explicit 16/32-bit indices, submeshes, and
bounds. Reordering vertices requires a complete remap of every stream. Invalid
operations return typed errors rather than an empty mesh. Reference infantry
geometry, rigs, morphs, fixtures, and semantic vertex identities are preserved.

Scenes declare `CameraRequest`; runtime resolves one `ResolvedCamera` used by
rendering, picking, and capture. UI filters input before the camera controller.
Renderers neither process input nor own camera controllers. `SceneDirector`
owns frame metadata and snapshot reset; scenes only replace their payload.

Interactive viewport state and pointer capture belong to runtime's
`ViewportController`, backed by `camera::CameraController`. Input is consumed
once in frame update using the actual frame interval; pointer displacements
are not time-scaled. Focus loss cancels capture without resetting the pose.
Unit Lab camera revisions identify view presets, not model cache keys, so
regenerating equipment preserves the user's orbit and pan. Regression coverage:
`camera.controller`, `camera.viewport_controller`, and `runtime.unit_lab_smoke`.
Compilation, CTest, and GPU acceptance remain owner-run verification.

## Dependency policy

All dependencies are pinned submodules and configure performs no downloads.
No integration changes global warning or ISA flags. Core geometry uses pinned
meshoptimizer `9e1f07b159d3cb777f1c67ed31fc11fd117986f4`, MikkTSpace
`3e895b49d05ea07e4c2133156cfa94369e19e409`, and earcut.hpp
`c68c8835ccff2b7532d31d8fa8dfcf398f629498`. Optional assets use fastgltf
`0d1b67a28c4950ea2deb796702006dcbe31e02b3`; optional CSG uses Manifold
`0edd9d54876f3135e431575214dd6d8a72866fee`.

## Delivery and evidence

The normative tracker is `docs/migration/lightweight-toolkit/POSTEP.txt`.
Source readiness and verification are separate. The user runs configure,
compilation, CTest, benchmarks, and GPU/visual acceptance. No result is reported
as verified without evidence tied to the exact tested commit. Local submodule
changes are never reset, overwritten, or automatically staged.
