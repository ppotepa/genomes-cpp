# Scene UI (MVVM) architecture

RmlUi is the only UI renderer. Scene/domain code publishes a `UiDataModel`; the
RmlUi adapter owns bindings, documents and input translation. A route retains
its own document and model; overlays are mounted after their base route.

`UiDataModel` is renderer-neutral and tracks field state, revision and changed
keys. A write of an equal value is not a change. Fields declare `Live`,
`OnChange` or `Explicit` commit policy. Numeric input is locale-independent,
finite and clamped/quantised against its field limits. Invalid options preserve
the last valid value. Existing command-backed scene values may remain plain
typed scalars; the event boundary preserves their scalar type and publishes
them on the change/click/submit phase so the scene action handler remains the
single domain validation point.

Shared controls live under `mods/core/ui`: text/output/status, buttons,
text/number inputs, select/options, range, checkbox/radio, progress, form and
command bars, panels, scroll containers, tabs, modal overlays, metric/table and
file-picker presentation. The neutral `IFileDialogService` is implemented by
`platform::SdlFileDialogService`; it translates SDL3's asynchronous native
dialog callback to the optional path used by the RmlUi FilePicker.

The manifest is authoritative for document/controller/action namespace. Scene
layouts remain in `mods/core/scenes/<scene>`; they link the common technical
theme rather than copying control state styles.
