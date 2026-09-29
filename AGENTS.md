# Genomes migration handoff

Read `docs/migration/threepp/START.txt`, `POSTEP.txt`, and
`docs/decisions/ADR_THREEPP_PRESENTATION.md` before renderer work.

The accepted target is threepp/GLRenderer with an SDL3-owned OpenGL context.
Diligent remains a temporary separate legacy profile. Old manual references
that mandate Diligent as the final renderer are superseded by the ADR; the
simulation/domain/ownership rules of those documents remain in force.

Current delivered scope is dependency + platform bootstrap, NOT a threepp
Infantry Lab or complete IRenderer. Do not report character migration complete.
The master task IDs remain TP-00-01 through TP-16-03 (88 tasks); preserve them.

The user performs configuration, compilation, tests and visual acceptance.
Prepare code/tests/commands and source reviews, but do not run builds or tests
unless the user explicitly changes that division of work. WAIT_USER is not
VERIFIED. Record exact code and evidence SHAs; never invent GPU results.

Preserve generator outputs and reference fixtures while replacing presentation.
Do not rewrite anatomy, physics, navigation, ECS or AI in the renderer slice.
Do not introduce a second GLAD, GLFW window, Canvas loop or GPU-owning worker.
Public simulation and domain data must not expose threepp types.

Before publishing, reread main and compare files touched by other authors.
Use a non-forced fast-forward. Never reset another author's working tree.
