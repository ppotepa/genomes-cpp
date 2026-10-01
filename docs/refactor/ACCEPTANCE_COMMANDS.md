# Architecture refactor acceptance commands

These commands are the user-owned verification handoff for source baseline
`7b97268` (the session clock, authoritative tick phases, resolved-world revision binding, target closure guard, narrow public-header consumers, infantry-off parser and remaining Unit Lab namespace clients are included). They do not turn source
guards into `VERIFIED`; each result must be recorded with the exact tested SHA
in `ARCHITECTURE_REFACTOR_TRACKER.md`.

## Source-only guards

```powershell
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/architecture_refactor_tracker.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/application_command_boundary_guard.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/scene_ui_guard.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/battlefield_pipeline_guard.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/session_simulation_clock_guard.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/resolved_world_artifact_guard.cmake
cmake -DGENOMES_SOURCE_DIR=$PWD -P tests/native/target_closure_guard.cmake
```

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
