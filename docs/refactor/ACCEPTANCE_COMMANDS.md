# Architecture refactor acceptance commands

These commands are the user-owned verification handoff for source baseline
`7cb06af` (the session clock, authoritative tick phases, resolved-world revision binding, isolated worker-shutdown violation tests, authoritative Battlefield execution-policy parity test, explicit combat EntityId trace assertions and guard, 75/100/150% UI density coverage, content-registry ordering/rollback and native-plugin load/rollback coverage, target closure guard, narrow public-header consumers, infantry-off parser and remaining Unit Lab namespace clients are included). They do not turn source
guards into `VERIFIED`; each result must be recorded with the exact tested SHA
in `ARCHITECTURE_REFACTOR_TRACKER.md`.

## Source-only guards

After configuring a build tree, run the complete CTest source-labelled set. This
also exercises the public-header consumers and namespace-boundary tests that
are registered alongside the standalone guards:

```powershell
$buildDir = "<configured-build-directory>"
ctest --test-dir $buildDir -L source --output-on-failure
```

The standalone guard inventory is the following 18 scripts. Run this loop when
the source contracts must be checked without a configured/build tree (or when
the CTest label inventory is being audited):

```powershell
$sourceGuards = @(
  "config_boundaries.cmake",
  "architecture_refactor_tracker.cmake",
  "battlefield_pipeline_guard.cmake",
  "session_simulation_clock_guard.cmake",
  "resolved_world_artifact_guard.cmake",
  "composition_root_guard.cmake",
  "target_closure_guard.cmake",
  "application_command_boundary_guard.cmake",
  "ballistics_ammunition_catalog_guard.cmake",
  "weapons_catalog_guard.cmake",
  "combat_tactical_ai_catalog_guard.cmake",
  "appearance_catalog_guard.cmake",
  "building_profile_guard.cmake",
  "pr16_measurement_benchmark_guard.cmake",
  "world_generation_profile_guard.cmake",
  "scene_ui_guard.cmake",
  "infantry_equipment_catalog_guard.cmake",
  "destruction_material_catalog_guard.cmake"
)
foreach ($guard in $sourceGuards) {
  cmake -DGENOMES_SOURCE_DIR=$PWD -P "tests/native/$guard"
  if ($LASTEXITCODE -ne 0) { throw "source guard failed: $guard" }
}
```

The direct loop is authoritative for all 18 guard scripts. `ctest -L source`
additionally covers the source-labelled executable consumers registered by
`tests/native/CMakeLists.txt`.

## Configure, build and CTest

Run each pair and retain stdout/stderr for the tested SHA:

```powershell
$presets = @(
  'headless-core-debug', 'headless-core-release',
  'toolkit-full-debug', 'toolkit-full-release',
  'headless-core-infantry-off',
  'dev-debug', 'dev-release', 'dev-diligent', 'release-diligent'
)
foreach ($preset in $presets) {
  cmake --preset $preset
  if ($LASTEXITCODE -ne 0) { throw "configure failed: $preset" }
  cmake --build --preset $preset
  if ($LASTEXITCODE -ne 0) { throw "build failed: $preset" }
  ctest --preset $preset --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw "CTest failed: $preset" }
}
```

If a build fails, replace the ignored `build-error.log` with the complete
latest stdout/stderr. Clear it only after the matching build succeeds.

## R054 benchmark handoff

Use the `release-diligent` profile and the immutable fixture described in
[`PR16_MEASUREMENT_GATE.md`](PR16_MEASUREMENT_GATE.md). Capture the raw
`samples_us`, semantic hash, median/p95/p99/max and machine/dependency
fingerprints for reference and candidate runs. The current fixture deliberately
reports `BASELINE_REQUIRED`; do not infer a performance pass from source review.

## GPU acceptance

For `dev-diligent` and `release-diligent`, run shader creation, D3D12 capture,
resource-retirement and visual acceptance on the target machine. Record the
driver/compiler/dependency versions and attach the capture to T26/R050-R051;
no GPU result is present in the repository yet.
