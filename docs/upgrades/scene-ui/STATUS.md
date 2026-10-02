# Scene UI / RmlUi status

## Compact Material-inspired revision (2026-10-01)

Source changes cover 8/8 routes plus the control gallery. Shared theme owns
form geometry, interaction states, disclosures, readouts and inspector lists;
Unit Lab no longer duplicates those appearances. Labels use sentence case,
disclosure bodies preserve the form axis, and viewport background stays clear.
The gallery becomes one column on narrow logical viewports.

Verification: 9/9 RML documents parsed as XML; action/control/data binding
attributes compared against the pre-edit worktree and preserved. Scoped
git diff --check reported no whitespace errors. These are source checks only.
The extended ui.rml_smoke alignment assertions have not been compiled or run.
Visual acceptance of this revision: 0/8 routes; configure/build/CTest and GPU
review remain user-run. Earlier source-ready counts do not establish visual quality.

User verification commands (existing configured build):

```powershell
cmake --build build/ui-rml --config Debug
ctest --test-dir build/ui-rml -C Debug --output-on-failure -R "^ui[.]"
```

Inspect each route at 1280x720 and 1920x1080, at 75/100/150% density, including
opened dropdowns, expanded groups, long equipment names and keyboard focus.

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
The consolidated source review is maintained in
[`CODE_REVIEW.md`](CODE_REVIEW.md).

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

Unit Lab runtime follow-up: animation scrubbing now evaluates a valid positive
fixed interval, transport state survives model regeneration, and expression/
pause labels publish their updated UI values. Genome slider events keep their
dynamic gene key as an action argument instead of treating it as a static
UiDataModel field. These changes are source-reviewed but still require the
configured build and runtime smoke tests above.

The Rml smoke fixture also exercises the equipment disclosure with 24 slots:
each select must retain one visible option after nested `data-for`/`data-if`
expansion. This is a source-level regression contract for the reported
multi-column/unfiltered dropdown glitch.

The structural guard now applies the explicit-option-value rule to every
production route, not only Unit Lab.

It also validates every route-level `data-control` against its corresponding
scene handler (`World Lab`, `World Config`, `Building Lab`, `Settings` and
`Unit Lab`), preventing inert controls from being introduced by markup-only
changes.

The same guard now validates every `data-action` in route and gallery RML
against the production scene/application router surface.

All production routes now expose the shared `{{ fps }}` readout. The
application composition root updates it from a 250ms rolling frame-time window
(or the deterministic 60 FPS cadence), so scenes do not own duplicate timing
logic.

It also extracts every `unit.*` `data-control` from the RML document and
requires a matching `stable_id()` branch in `UnitLabScene`, preventing a
visually present but inert control from entering the route.

Animation phase, playback speed and face intensity are published as `Live`
fields with bounded ranges, so drag events update the preview continuously
instead of waiting for a range `change` event.

The GPU fallback viewport calculation now uses the same responsive chrome
geometry as Unit Lab RML (52dp rail, 46dp topbar, 44/68dp toolbar, 28dp
caption, and 336/360/380dp inspector breakpoints), preventing the model from
being shifted under the inspector when RmlUi metrics are unavailable.

Select popups now explicitly force a single full-width option column through
both RmlUi element-tree variants (`select selectbox` and `selectbox`). Dynamic
Building Lab options use `data-attr-value`, matching the native select value
contract instead of the unrelated data-model binding attribute.

The Unit Lab presentation now appends the catalogued primary-weapon mesh to the
skinned prototype, transformed into the `WeaponBack` socket and weighted to
`SpineUpper`. The legacy four-vertex transport ribbon remains intact for
fixture parity; the visible weapon follows upper-body animation independently.
`infantry_presentation_tests` now checks this contract independently of the
scene smoke test and is registered as `infantry.presentation` in the native
CTest manifest.

The neutral event boundary also accepts existing command-backed plain scalars
(`seed`, side/toggle state, wear and world-lab values) with type-preserving
conversion. Input is consumed until the declared change/click/submit phase, then the
scene handler receives the committed value; invalid conversions are consumed
without publishing. `ui.data_model` covers this regression. This is source
review only until the owner runs the configured native tests.

Building Lab’s explicit damage slider now commits from the authoritative UI
draft when Apply Damage is pressed, avoiding a stale scene-side value. This is
source-reviewed only until the owner runs the configured native tests.
