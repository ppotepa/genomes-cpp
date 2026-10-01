# Scene UI / RmlUi status

Baseline inspected: `e9358c0dc78ceb33957fea49c7503d23da35f49a` (2026-09-30).
Implementation is currently an uncommitted working-tree slice; `HEAD` remains
that baseline SHA, so acceptance must record a later commit SHA after review.
The worktree was already dirty, including the Diligent submodule; those changes
were preserved and were not staged, reset or absorbed.

Source-ready in this slice:

| Area | State | Evidence |
|---|---|---|
| Neutral model | CODE_READY | `UiDataModel` tracks dirty keys/revisions, field state, commit policy and finite number/options validation. |
| Typed UI vocabulary | CODE_READY | `UiEvent` and `UiCommand` declare phase, route revision and scalar payload. |
| Manifest lookup | CODE_READY | `UiContentRegistry` resolves a route from its stable scene id. |
| Route lifecycle | CODE_READY | `UiRuntime` keeps a controller/model instance per route and tears down only removed suffixes. |
| Rml bindings | CODE_READY | per-route data models, scalar/list bindings and dirty updates are mounted from the route stack. |
| Input | CODE_READY | edit/navigation keys, typed input/change/submit events and capture release are implemented. |
| Scene ViewModels | CODE_READY | Main Menu, World Config, Battlefield, Building Lab and Unit Lab publish model fields; Settings owns session controls. |
| Control gallery/theme | CODE_READY | `mods/core/ui/theme.rcss`, templates and `ui-test.rml`. |
| File picker service | CODE_READY | `IFileDialogService` remains renderer-neutral; `SdlFileDialogService` uses SDL3 native open/save dialogs and tests can inject `NullFileDialogService` or a fake. |
| World Lab route | CODE_READY | separate menu route with an explicit prototype disclaimer, session-only fields and import/export actions; `runtime.world_lab_smoke` covers navigation and draft state. |
| Legacy bridge | CODE_READY | runtime scenes and Diligent UI pass no longer consume `UiWidget` or screen hit-tests. |

The control-to-test-to-scene matrix is maintained in
[`CONTROL_MATRIX.md`](CONTROL_MATRIX.md).

UI polish package denominator: 8/8 source slices are prepared in the working
tree: shared theme, event/format contracts, stable RmlUi bindings, Unit Lab,
World/Building labs, Settings/menu/overlays, and regression/source guards.
World Lab remains explicitly session-only; Terrain, Fauna and Vehicle Lab are
shown as unavailable rather than backed by fabricated domain controls.

Verification for the final polish edits: `git diff --check`, the CMake static
guard, CodeGraph synchronization/status and source review. Earlier neutral UI
work recorded clang++ syntax checks, but the final worktree has not been
compiled after these edits.
Configure, build, CTest, Debug/Release and GPU/visual acceptance were not run
by the agent and remain for the user at the final chosen SHA.

Source readiness: 8/8 = 100% CODE_READY in the uncommitted worktree.
Visual acceptance: 0/18 = 0% VERIFIED. The 18 cases are the eight routes at
1280x720/150%, the eight routes at 1920x1080/100%, and two additional Unit Lab
passes covering the remaining resolution/scale combinations with an open
dropdown and an actively dragged slider. Debug/Release configure, build,
CTest, GPU and visual confirmation remain user-run gates at one exact SHA.
