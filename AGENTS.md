# Genomes infantry upgrade

Read `docs/decisions/ADR_INFANTRY_STACK.md` and
`docs/upgrades/infantry-stack/STATUS.txt` before changing renderer/dependencies.
The previous threepp-as-production-renderer ADR is superseded by the owner's
clarification: Diligent is the production rendering target; threepp supplies
selected CPU geometry/tools behind `genomes::geometry`. GLRenderer is an
explicit experimental/reference adapter, not the final architecture.

Keep the existing infantry module. Do not start Creature/Humanoid extraction,
replace the ECS or add Magnum/Manifold/Jolt/Recast/Ozz to this work package.
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

GENOMES_ENABLE_THREEPP controls the existing dependency/CPU provider.
GENOMES_ENABLE_THREEPP_RENDERER separately controls the experimental GL adapter.
The existing upstream dependency still builds more than CPU geometry: do not
claim GLFW/GL/codecs have been removed without a dependency-closure audit.
Do not delete Diligent or blindly switch presets onto known-broken backend code.
Do not undo the owner's 66843b5 toolchain fixes while shrinking that dependency.

Before publishing, reread main. Preserve concurrent changes, use non-forced
fast-forward updates, and publish complete slices with tests and documentation.
