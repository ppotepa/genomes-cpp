# Feature card: `inspection-olive`

This is the PR15 pilot for a data-only presentation feature. It is manually
selectable and is not part of the default or random appearance selection.

| Field | Contract |
|---|---|
| Owner | Unit Lab presentation adapter |
| Stable identity | `appearance.inspection-olive`, schema version `1` |
| Data | Frozen typed appearance catalog entry targeting the existing `UniformCloth` material region with the inspection olive palette |
| Command | Typed `SetAppearancePreset`; UI boundary uses `set-appearance-preset inspection-olive` |
| Determinism | Preset ID and canonical color are stable; no RNG or generator input changes |
| Cache | Base immutable `SkinnedMeshPrototype` is reused; the material variant receives a derived presentation revision |
| Dirtiness | Material and UI only; geometry, pose and model compiler requests are unchanged |
| Errors | Unknown preset is rejected with an owning parser diagnostic; no partial publication occurs |
| Evidence | `unit_lab_command_parsing`, `infantry_presentation`, `AppearanceCatalog::validate`; CLI now parses the same typed variant and serializes it only at the UI dispatch boundary (source-ready; CTest pending) |
| Compatibility | No ECS, generator, anatomy, semantic vertex numbering or GPU ABI changes |

The pilot changes the catalog/command boundary and the presentation adapter only;
it does not add a new renderer or alter the default appearance.
