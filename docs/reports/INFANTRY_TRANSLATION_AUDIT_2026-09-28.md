# Infantry: translation audit and recovery direction

Recorded: 2026-09-28. This is a diagnostic record of the inspected working tree,
not an implementation-completion report. Recheck findings after code changes.
Target for writes: `D:\Git\genomes-cpp`. Read-only behavioral/visual reference:
`D:\Git\genomes\reference\js`.

## Conclusion

The native soldier is not yet a faithful translation of Infantry Lab. Missing
Three.js services explain only part of the difference: custom anatomy, surface
generation, equipment fitting and animation algorithms were also simplified or
not connected. Adding a geometry library alone will not restore the model.

Do not carry forward the earlier claim of 96% completion. Successful builds,
native invariant tests and a short crash-free run do not establish visual or
behavioral parity. No defensible completion percentage has been established.

## Findings to retain

| Area / source | Observed gap |
| --- | --- |
| `engine/runtime/src/UnitLabScene.cpp` | UnitLab now publishes the shared immutable skinned prototype and hierarchical pose palette. The CPU copy remains as a diagnostic/fallback path. Gear is still generated from simplified native primitives and does not yet match every articulated JS attachment. |
| `engine/render/core/src/SkinnedDeformer.cpp` | Position deformation uses diagonal matrix entries and translation, omitting rotational cross terms. Normals are not transformed by the bone palette. |
| `modules/infantry/src/PhenotypeResolver.cpp` | Landmark scale and face sections have been corrected toward the reference, but the resolver still lacks the full FaceAnatomy correction set and diagnostics. Several phenotype fields remain only partially represented in native output. |
| `modules/infantry/src/InfantryGenome.cpp` | All body and face fields are now sampled, with the reference `varied`/`centred` ordering. RNG seed compatibility and the incomplete override surface still differ from JS. |
| `engine/proc/include/genomes/proc/RandomStream.hpp` | RNG is still PCG32, not the planned Mulberry32. Seed derivation also differs: replacing the algorithm alone does not establish matching JS seeds. |
| `modules/infantry/src/AppearanceCompiler.cpp` | Torso, head and hair now have denser multi-level profiles, separate hand/boot surfaces and front-side opening filtering, but facial feature surfaces, clothing ports/details and style-specific hair remain substantially simpler than JS. |
| `modules/infantry/src/InfantryModelCompiler.cpp` | Request cache key omits equipment overrides and color alpha. Appearance is compiled before equipment fit. Returning the previous model on failure hides the failure reason. |
| `modules/infantry/src/GearGenerator.cpp` | The artifact contract still stores fitted pieces as compact primitives; UnitLab now expands those pieces into rounded panels, pouches, pack details and weapon subparts, but this remains a simplified translation of the JS gear builder. |
| `modules/infantry/src/AnimationSystem.cpp` | Pose starts from local bind transforms while locomotion and face state are stored separately; full animated bone-pose writing is missing. Wiring this system into the scene alone will not restore gait/IK. |
| `engine/render/diligent/DiligentBackend.cpp` | The adapter now has a working D3D12 GPU skinned pass using a mutable SRB, but preview material shading still adds material-ID/actor tint rather than faithfully using all anatomical material regions. Vulkan and manual visual parity remain to be checked. |
| `engine/runtime/src/BattlefieldScene.cpp` | Its separate ordinary-mesh path loses skinning, morphs and gear instead of using the common animated model pipeline. |

These are interacting failures: incorrect phenotype changes the shape; reduced
topology removes features; incorrect deformation changes placement; preview
materials further alter appearance. Fixing only one cannot establish parity.

## Progress after the initial audit

The first recovery slice is now implemented and tested:

- CPU skinning applies the complete column-major 4x4 palette to positions and
  normals, including weighted fallback behavior.
- UnitLab builds each palette as hierarchical `poseWorld * inverseBind`, so a
  bind-pose mesh is invariant and animated local poses can reach the renderer.
- UnitLab uses the existing animation system for locomotion, posture, face and
  eye/jaw controllers; its result is converted into the same palette.
- Jaw rotation is no longer incorrectly sent to the `neckFlex` morph slot.
- A regression test covers rotated positions, rotated normals and zero-weight
  fallback (`render.skinned_deformer`).
- The native genome sampler now assigns every body and face gene in the same
  `varied`/`centred` categories and ordering as the reference generator.
- The native landmark scale was corrected toward the reference anatomy: hips
  near `0.54H`, eyes near `0.935–0.945H`, and face spacing is constrained by
  the jaw instead of using the former low-face coordinates.
- Face, torso, hair, hands and boots now use multiple anatomical levels; front
  opening filtering checks the front half of the profile, and gear details are
  expanded into rounded panels, pouches, pack straps and weapon subparts on
  the same anchor bones.
- The Diligent terrain guard was corrected so a terrain mesh no longer causes
  `draw_meshes()` to return before world, infantry and instance passes. The
  instance filter was also corrected so UnitLab's `Preview` actor is not
  dropped merely because the scene has terrain. The adapter compiles with the
  installed Windows SDK `fxc.exe`.
- Diligent now contains a dedicated skinned infantry GPU pass. It uploads the
  immutable prototype once per revision, evaluates four morph channels and
  four bone influences per vertex, applies the 69-matrix palette and instance
  transform on the GPU, and excludes that actor from the compatibility CPU
  instanced pass. The original D3D12 crash was caused by the static resource
  binding path for this pipeline; using a mutable SRB fixes it. A 30-second
  windowed D3D12 UnitLab smoke run remains alive with GPU skinning active.
  The CPU deformer remains available for headless diagnostics and fallback.

The Diligent-enabled build was verified from the existing
`build/diligent-config` tree. With the installed Windows SDK `fxc.exe` added to
the process PATH, `genomes_render_diligent` compiles and links successfully.
This is compile-level verification plus a 30-second D3D12 GPU smoke run; it
does not replace the required manual visual review or the Vulkan run.

This fixes the renderer-side skinning connection, not the remaining anatomy,
equipment, material-region and reference-parity gaps listed below.

## What the reference actually supplies

- `faceAnatomy.js`: anatomical landmarks, skull/neck profiles, Hermite
  interpolation and relation corrections; normalized coordinates converted to
  world dimensions at the boundary.
- `surfaceBuilder.js`: oriented rings, bridges/caps, tubes and ellipsoids,
  normal hints and winding correction, UV/material groups, normalized top-four
  skin weights and morph attributes.
- `infantrySurface.js`: detailed jacket and limb profiles, shoulder connections,
  hands/fingers, pants, boots and clothing details.
- `infantryHair.js`: anatomical scalp sampling, closed crowns, distinct styles,
  detail levels and equipment masking.
- `infantryFactory.js`: shared skeleton, bind/inverse-bind relationships and
  separation of reusable mesh assets from instance pose.
- `infantryMaterials.js`: material-region behavior and procedural cloth texture.
- `infantryAnimator.js`: actual bone motion, gait targets, contact, IK and weapon
  pose, not merely a phase counter.

Native equivalents are needed for both these custom algorithms and the relevant
Three.js services. No Node, Three.js or bridge is required in the native runtime.
An offline reference exporter for development/testing is a separate concern.

## Verification gaps and required acceptance

`tests/native/infantry_reference_parity_tests.cpp` currently checks native
consistency rather than comparing exported JS results. Appearance tests check
basic counts, flags, weights and determinism, not anatomical fidelity.
`reference/fixtures/infantry/appearance_profiles.json` describes diagnostic
native invariants and treats native topology as authoritative; it must not be
presented as independent proof of JS parity.

Required checks: neutral-pose bind invariance; full-matrix rotation and normal
tests; versioned RNG/domain vectors; all genome fields and overrides; measured
landmarks/bounds against reference samples; 69-bone hierarchy/bind contracts;
real opening topology; UV/material/weight/morph integrity; gear fit; cache keys
and observable atomic-fallback errors; deterministic animated pose snapshots,
posture transitions and IK/contact. Keep manual visual review of UnitLab, not
an automatic golden-image gate. The terrain guard remains until its separate
pass test succeeds. A 30-second runtime smoke test is necessary but insufficient.

## Repair sequence

1. Correct palette/bind-space mathematics and CPU deformation, with invariant tests.
2. Translate versioned genome sampling and phenotype anatomy; explicitly define
   seed compatibility and compare numeric reference samples.
3. Implement reusable native surface-building operations and faithfully translate
   body, face, hair and equipment-fit geometry, preserving semantic attributes.
4. Produce real animated bone poses, facial morphs, IK and ground contact.
5. Render the same data through a GPU skinning/material pass; verify UnitLab
   before reusing the pipeline in Battlefield.

Preserve the existing module contracts rather than adding a second infantry
implementation. Preserve unrelated local changes. Keep immutable artifacts
separate from per-instance animation state. Do not optimize away fidelity work.

## Geometry-library assessment

Candidate review only: no dependencies have been integrated or benchmarked.

- [libigl](https://libigl.github.io/tutorial/): a good optional processing
  toolkit for normals, remeshing, parametrization and deformation, but it is
  not the missing translation of `infantrySurface.js`/`infantryHair.js`.
- [CGAL Polygon Mesh Processing](https://doc.cgal.org/latest/Polygon_mesh_processing/):
  the strongest candidate for robust mesh repair, intersection and remeshing;
  it is too heavy for the hot procedural UnitLab path and should remain an
  offline/tooling dependency unless a concrete boolean or repair problem is
  measured.

- [GLM](https://github.com/g-truc/glm): header-only vectors, matrices and
  quaternions. Candidate mathematical foundation, not an anatomy/mesh generator.
  Integration still needs explicit matrix layout, multiplication order, coordinate
  conventions and shader-boundary tests.
- [par_shapes / par_octasphere](https://github.com/prideout/par): small C libraries
  for parametric/simple surfaces and rounded shapes. Useful candidates for gear
  primitives, not replacements for anatomical profiles or skin/morph generation.
- [PMP](https://github.com/pmp-library/pmp-library): polygon mesh structure and
  processing, including smoothing, subdivision, remeshing and decimation.
  Consider only when a concrete processing need justifies integration.
- [meshoptimizer](https://github.com/zeux/meshoptimizer): mesh optimization and
  simplification after correctness. Preserve/remap every attribute and morph
  stream; position-only welding can destroy seams and deformation semantics.

Recommended starting point: evaluate GLM behind engine-owned types and build
our own small SurfaceBuilder translating the existing reference operations.
Use optional primitives where justified; defer general remeshing and LOD work.
Topology-changing operations need explicit propagation of UVs, regions, bone
weights and morphs and must preserve intentional eye/mouth openings.
