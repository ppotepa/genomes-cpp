# Scene UI control matrix

The matrix records the owning contract and the source-level smoke coverage.
Visual acceptance is still a user responsibility at the final SHA.

| Control/contract | Neutral or adapter test | Scene usage |
|---|---|---|
| Scalar field, dirty revision, validation | `ui.data_model` | World Config, Settings, Unit Lab, Building Lab |
| Option list and dynamic rows | `ui.data_model`, `ui.rml_smoke` | World Config, World Lab, Building Lab |
| Live range + output | `ui.data_model`, `ui.rml_smoke` | World Config, Unit Lab, World Lab |
| On-change number/select | `ui.data_model`, `runtime.world_lab_smoke` | World Config, Unit Lab, World Lab |
| Explicit command/damage | `runtime.unit_lab_smoke`, source review | Building Lab, Unit Lab |
| Checkbox/radio and disabled/hidden guard | `ui.data_model`, `ui.rml_smoke` | Settings, Unit Lab, World Lab |
| Modal overlay/focus/capture | `ui.rml_smoke`, `ui.route_lifecycle` | Pause, Settings |
| Metric/list/table/progress | `ui.rml_smoke` | Battlefield, Building Lab, Unit Lab |
| File picker | `ui.services` fake; SDL implementation source | World Lab prototype import/export |
| Legacy boundary/static guard | `ui.scene_guard` | all migrated scenes |

The shared catalogue is in `mods/core/ui/controls.rml` and the technical state
theme is in `mods/core/ui/theme.rcss`; scene RCSS files only provide layout.
