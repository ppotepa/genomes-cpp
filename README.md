# Genomes C++

Native C++ implementation and migration target for Genomes.

## Repository roles

**TARGET:** `ppotepa/genomes-cpp`

This repository contains:

- native C++ engine and game implementation,
- executable implementation manual under `docs/`,
- minimal parity/reference project under `reference/`,
- native tests, benchmarks and CI.

**SOURCE / legacy reference:** `ppotepa/genomes`

The source repository contains the existing JavaScript/browser runtime, procedural prototypes, legacy tests, research and calibration material.

## Manual

Start with:

- `docs/0.0_NATIVE_MANUAL_INDEX.txt`
- `docs/0.1_TARGET_ARCHITECTURE.txt`
- `docs/0.6_REPOSITORY_TOPOLOGY_AND_REFERENCE_PROJECT.txt`
- `docs/MANUAL_STATUS.txt`

Implementation is chapter-driven. A command such as:

`Implement docs/1.1_CPP_CMAKE_BOOTSTRAP.txt in this repository.`

should be sufficient for an implementation agent to execute one verified step.

## Reference project

`reference/` is not a mirror of the private legacy repository.

It contains only versioned manifests, schemas, fixtures and golden outputs required for native parity. Every exported artifact must record its exact SOURCE commit and provenance.

## Core technology direction

- C++20 initially, with selective C++23 adoption only after toolchain baseline changes
- Diligent Engine for rendering/RHI
- Jolt Physics behind a Genomes physics abstraction
- Recast/Detour behind a Genomes navigation abstraction
- custom data-oriented simulation and JobSystem
- generic GPU compute through Diligent
- optional CUDA fast paths only where benchmarks justify them
