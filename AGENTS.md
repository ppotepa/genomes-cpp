# Genomes infantry upgrade

Read `docs/decisions/ADR_LIGHTWEIGHT_TOOLKIT.md`,
`docs/migration/lightweight-toolkit/POSTEP.txt`,
`docs/decisions/ADR_INFANTRY_STACK.md` and
`docs/upgrades/infantry-stack/STATUS.txt` before changing renderer/dependencies.
The previous threepp-as-production-renderer ADR and transitional hybrid are
superseded: Diligent/D3D12 is the production GPU target and threepp is removed.

Keep the existing infantry module. Do not start Creature/Humanoid extraction,
replace the ECS or add Magnum/Jolt/Recast/Ozz to this work package. Manifold is
permitted only behind the optional CSG target described by the toolkit ADR.
Upgrade working pieces instead of restarting the migration. Do not construct
an anatomical human from disconnected library primitives.

The user performs configure, compilation, CTest, benchmarks and GPU acceptance.
The agent prepares code, regression tests, commands and source reviews. Never
claim a build/test/capture passed without evidence tied to an exact commit.
Report progress with an explicit denominator and separately report verification.
The old TP roadmap's percentages are historical, not completion of the hybrid.

Preserve JS fixtures, generator outputs and raw semantic vertex numbering.
Optional meshoptimizer preparation is at the presentation boundary, once per
immutable prototype. Current scope is triangle order only, within opaque draw
ranges; no vertex remap, welding, quantization, LOD or transparent reordering.

Do not claim threepp/GLFW/GL has been removed until the static guard and a
dependency-closure review both pass.
Do not delete Diligent or blindly switch presets onto known-broken backend code.
Do not undo the owner's 66843b5 toolchain fixes while shrinking that dependency.

Before publishing, reread main. Preserve concurrent changes, use non-forced
fast-forward updates, and publish complete slices with tests and documentation.
Never reset, overwrite, stage, or otherwise absorb local submodule changes.
