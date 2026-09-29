# Infantry JavaScript → C++ parity contract

The authoritative source is `reference/code`, pinned to commit
`da885ca68b2ae63154a004574fed00eb9dfeb458` and Git tree
`50e15a425f00611df1900b3d56c1d382978ec19e`. Runtime and normal builds must
not execute Node or read that directory. JavaScript is a developer-only fixture
generator enabled with `GENOMES_ENABLE_JS_REFERENCE_PARITY=ON`.

Statuses are strict: **parity** means checked-in fixtures pass; **verified**
additionally requires every build/test row and the manual capture matrix. A
stage without its fixture cannot be marked parity.

| Stage | JS source → C++ area | Fixture / tolerance | Gate | Status | Commit |
|---:|---|---|---|---|---|
| 0 | provenance/exporter → reference tooling | manifest + SHA-256, exact | exporter v10 smoke plus 36 matrix, seven hairstyle, 30 equipment, eight weapon and four 600-frame animation GNIF containers; the opt-in CMake target regenerates them into the build tree and byte-compares every GNIF container and JSON manifest with its checked-in fixture | parity | working tree |
| 1 | `infantryGenome.js` → genome/phenotype | genome, `2e-6` | all 84 genes and 86 phenotype fields, 1024 seeds | parity | worktree |
| 2 | `surfaceBuilder.js` → surface builder | GNIF; normals `2e-5`, other floats `2e-6` | groups/tags/materials/morph bounds implemented; connected-face orientation now follows the authoring-number JS path and tailoring uses the explicit `EquipmentFit::mapTorsoYExact(double)`/double profile path without premature float quantization; primitive buffer parity pending | partial | worktree |
| 3 | `infantryRig.js` / `faceAnatomy.js` / `handRig.js` → anatomy/rig | four anatomy + bind fixtures, `2e-6` | 69 bones, exact hierarchy/binds, 19 face levels, 54 Hermite samples and all finger chains | parity | worktree |
| 4 | body/face/hair → avatar surface | three LODs, seven hairstyles | the production compiler consumes one reference generator assembling body plus four morphs. All 36 seed/LOD/equipment fixtures match complete base streams, topology, weights, indices and every morph after reproducing the persistent JS face-section cache and its `Math.sign(±0)` eyelid behavior. Seven dedicated High fixtures additionally prove exact position and index parity for every hairstyle, including bald. The target normal tolerance (`2e-5` versus provisional `1e-3`) remains pending | partial | worktree |
| 5 | equipment/weapons → gear and rigid chunks | `EQUIPMENT/v1` + eight weapon GNIFs | catalog, fitting and all weapon buffers/grips pass; all 36 neutral/default/full matrix fixtures and dedicated fixtures for every gear generator branch match topology, positions, colors, UVs, weights, groups, tags and indices; normal error is provisionally `2e-4` for the matrix and up to `2e-3` on very thin accessory primitives (contract target `2e-5`) | partial | worktree |
| 6 | animator/face/weapon → animation | positions `2e-5`, rotations `2e-4 rad` | four v10 fixtures contain local transforms, morphs, filtered locomotion, posture, biped and limb targets, final `footGoals`, pre-damping target quaternions, the persistent `previousArmRotations` snapshot and pose channels for 600 frames of all nine JS states. Native comparison verifies every bone/morph channel in all 2400 `REST` frames; all biped gait/posture, target and contact-goal channels; all 69 final bone rotations for `IDLE`, `CROUCH` and `SITTING`; and all 69 final rotations for `PRONE`/`PRONE_MOVE` across four seeds × 600 frames. Production reconstructs the previous moving target from `phase - speed / cycle * dt`, applies the post-damping hip reach constraint before contact IK and uses surface-aware support metadata. The remaining moving-biped mismatch is limited to final damped arm/IK transforms and is still pending | partial | worktree |
| 7 | factory/compiler → compiler/presentation | full-avatar manifests, exact keys | cache/cancel/errors | partial — the production compiler now consumes the reference body/face/hair generator, publishes four morphs, removes degenerate production triangles, repairs orphan normals and preserves immutable cached prototypes; request keys, atomic error publication, immutable `lastSuccessful()`, revision-aware cancellation, palette-driven uniform materials, Unit Lab asynchronous revision-aware publication, and Battlefield per-instance palette lookup are covered. Renderer reflection/layout and capture acceptance remain pending | worktree |
| 8 | infantry renderer → GPU renderer | materials/lights `1e-6` | CPU/GPU + reflection | partial | `3d2710f` |
| 9 | Unit Lab → runtime scene | snapshots/captures | 600 frames without errors | partial | `3d2710f` |
| 10 | Battlefield integration | integration fixtures | shared pipeline | partial | `3d2710f` |
| 11 | damage/ragdoll adaptation | native invariants only | damage/ragdoll tests | implemented | `3d2710f` |
| 12 | fixture breadth | catalog hashes + extreme samples | all 36 seed/LOD/gear full-buffer fixtures are checked and parsed; every GNIF stream carries a quantized FNV-1a 64 descriptor hash which the native reader recomputes and verifies; mesh manifests carry position/normal/index extrema and the reader checks position extrema against the buffers; the checked-in 1024-seed genome catalog is parsed and every gene is verified against native generation at `2e-6`. Manual capture review remains pending | partial | worktree |
| 13 | build matrix | build logs | headless, SDL/RmlUi, Null, no-infantry, DX12 | parity — normal Debug passes 78/78; SDL+RmlUi Debug passes 79/79 including `ui.rml_smoke`; DiligentFX Debug and Release each pass 79/79, including `render.diligent_headless`; the no-infantry build completes without the infantry target. Manual graphical capture remains part of stage 14 | worktree |
| 14 | final acceptance | capture matrix | all gates + manual review | pending | — |

## GNIF v1

Files are little-endian. The 12-byte header is magic `GNIF`, `uint32` version,
and `uint32` stream count. Each stream has a `uint16` UTF-8 name length, name,
`uint8` scalar type (`1=Float32`, `2=Float64`, `3=Uint16`, `4=Uint32`), three
reserved zero bytes, `uint64` element count, then tightly packed data. A JSON
manifest next to the binary records request parameters, provenance, structure,
groups, tags, materials, and expected byte sizes.
Each stream descriptor also stores a quantized FNV-1a 64 hash; the native
fixture reader recomputes it before exposing the stream.

Full fixtures cover seeds `0`, `8841`, `1003`, and `0x5EED2026`, all three
LODs, and neutral/default/full gear. Broad fixtures store counts, bounds,
groups, extrema, and quantized FNV-1a 64 hashes. Checked-in fixtures,
`reference/code`, and Node scripts must never be installed or packaged.

## Audit checkpoints

### 0.4–0.5 `previousArmRotations` fixture channel

- Status before: animation fixtures predated the diagnostic snapshot channel.
- Change: promoted `previousArmRotations` to the animation GNIF schema and
  regenerated all four 600-frame fixtures through the common exporter.
- Files: `reference/code/tools/export-infantry-model.cjs`, the four
  `reference/fixtures/infantry/animation-*-v1.gnif` containers and manifests,
  and the native parity comparator.
- Fixture: every state contains `frameCount × 69 × 4` Float32 elements.
- Test: `infantry.fixture_reader` and `infantry.reference_parity` pass.
- Build: `genomes_generate_infantry_js_reference` regenerates and compares the
  complete checked-in matrix byte-for-byte.
- First difference: none after regeneration.
- Maximum error: within the existing animation contract; moving final arm/IK
  parity remains tracked by stage 6.
- Remaining: finish moving damping and final IK comparison.
- Status after: stage 0 remains `parity`; stage 6 remains `partial`.

### 2.1–2.4 strict body-normal probe

- Status before: body normals used the provisional `1e-3` gate.
- Change: retained the binary64 hip authoring coordinate through
  `BodyPhenotype`/`EquipmentFit`, and made the Float32 accumulation sequence
  explicit in `ReferenceSurfaceBuilder`.
- Files: `BodyPhenotype.hpp`, `EquipmentFit.hpp`, `PhenotypeResolver.cpp`,
  `GearGenerator.cpp`, `ReferenceSurfaceBuilder.cpp`, and the parity diagnostic.
- Fixture: `avatar-0-high-default.gnif`.
- Test: the normal gates are now `2e-5`; the negative probe fails on the
  tailoring normal as required.
- Build: normal Debug compiles; 77/78 tests pass, with only
  `infantry.reference_parity` exposing this strict mismatch.
- First difference after the exact bind-point and reference-height fixes:
  seed `0`, High/default, `body.normals[18732]`, tag `head`, expected
  `-0.287462`, actual `-0.287419`.
- Maximum error: approximately `4.3e-5`; the reported vertex position is
  bit-identical while adjacent face positions still differ by isolated ULPs.
- Remaining: reproduce the final JS face-shell trigonometric/normal
  accumulation path before changing the strict `2e-5` gate.
- Status after: stages 2 and 4 remain `partial`.

Follow-up: the jacket profile now also retains exact mapped Y and radius
values, and the reference builder uses the exact generated height. The strict
first difference is unchanged, so this is diagnostic progress rather than a
closed gate.
