# PR16 / R054 — measurement-led optimization gate

Status: **PLANNED**. This commit supplies the measurement contract only. It
does not implement an optimization, establish a performance baseline, or claim
that any benchmark has run.

R054 says that only measured optimizations ship, execution tuning preserves the
D1/D2 results, and the target/p95 gate must be met. The repository currently
has no user-supplied baseline, so the target and allowed regression fields are
intentionally `null` in
[`config/performance/pr16_measurement_inputs.json`](../../config/performance/pr16_measurement_inputs.json).
Those values must not be guessed from a developer machine.

## Inputs

The fixture is validated by
[`pr16_measurement_inputs.schema.json`](../../config/performance/pr16_measurement_inputs.schema.json)
and fixes the inputs that must remain unchanged between reference and candidate
runs:

- Release, `release-diligent`, Diligent/D3D12, validation off;
- the four deterministic, nonzero resolved seeds and the workload-specific counts/resolution;
- three warmups followed by ten measured runs;
- median, p95, p99, max, semantic hash, allocation count and allocated bytes.

The entry-point names identify the intended benchmark sources/targets. The two
infantry source benchmarks are listed as inputs even though their executable
registration is a follow-up implementation task. Adding a new executable or
changing a workload input requires a schema/fixture revision and a new review;
it is not an optimization result.

## D1/D2 acceptance

For each workload, D1 is the correctness prerequisite: the candidate must
produce the same versioned semantic result/hash as the reference fixture. A
different hash, missing sample, incomplete run, changed seed or changed build
profile is a correctness failure and cannot be promoted to a performance
baseline.

D2 is the measured execution result: after D1 passes, compare the candidate's
median and p95 against the user-provided baseline for the exact machine,
compiler, dependency revision, build profile, backend and workload. The gate
is workload-specific; a result from another machine or resolution is
`NO_BASELINE`, not a pass or fail.

The required evidence package is:

1. exact tested commit and clean-worktree report;
2. machine/OS/driver/compiler/dependency fingerprints;
3. immutable copy of the fixture used for the run;
4. raw runs with warmups separated from measured samples;
5. D1 semantic hashes and D2 median/p95/p99/max plus allocation/byte metrics;
6. the user baseline ID and the explicit target/regression values filled into a
   reviewed successor fixture.

Until item 6 exists, the acceptance result is `PLANNED` / `NOT_RUN`. No
execution tuning, mesh reordering, cache policy, batching, LOD, or other
optimization should be merged under R054 on the strength of this fixture.

## User handoff

The user runs the applicable benchmark targets/sources in the documented
Release profile, records the reference run first, and returns the tested SHA
and raw evidence.
Only then may a follow-up change fill `baseline`, `target_p95`, and
`allowed_regression_percent`, review the gate, and move R054 out of `PLANNED`.

This artifact deliberately does not add a CMake target or CTest test: source
readiness and user benchmark verification remain separate in the architecture
tracker.
