# Genomes C++

Final native C++ implementation of Genomes, informed by the earlier prototype.

## Repository roles

**TARGET:** `ppotepa/genomes-cpp`

This repository contains:

- native C++ engine and game implementation,
- executable implementation manual under `docs/`,
- minimal reference-validation project under `reference/`,
- native tests, benchmarks and CI.

**REFERENCE EVIDENCE:** `ppotepa/genomes`

The JavaScript repository supplies prototypes, test scenarios, research and
calibration evidence. It does not define native runtime architecture or require
legacy compatibility modes.

## Manual

Start with:

- `docs/0.0_NATIVE_MANUAL_INDEX.txt`
- `docs/0.1_TARGET_ARCHITECTURE.txt`
- `docs/TARGET_IMPLEMENTATION_CONTRACT.txt`
- `docs/SOURCE_PROTOTYPE_AND_MOCKUP_CONTRACT.txt` (non-normative evidence audit)
- `docs/architecture/TARGET_ARCHITECTURE.svg`
- `docs/architecture/TARGET_ARCHITECTURE.drawio.xml`
- `docs/0.6_REPOSITORY_TOPOLOGY_AND_REFERENCE_PROJECT.txt`
- `docs/MANUAL_STATUS.txt`

Implementation is chapter-driven. A command such as:

`Implement docs/1.1_CPP_CMAKE_BOOTSTRAP.txt in this repository.`

should be sufficient for an implementation agent to execute one verified step.

## Reference project

`reference/` is not a mirror of the private legacy repository.

It contains only versioned manifests, schemas, fixtures and selected reference
outputs used to validate TARGET requirements. Every imported artifact records
its provenance, but reference data cannot select production algorithms.

## Core technology direction

- C++20 initially, with selective C++23 adoption only after toolchain baseline changes
- Diligent Engine for rendering/RHI
- Jolt Physics behind a Genomes physics abstraction
- Recast/Detour behind a Genomes navigation abstraction
- custom data-oriented simulation and JobSystem
- generic GPU compute through Diligent
- optional CUDA fast paths only where benchmarks justify them
