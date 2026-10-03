# Genomes documentation

This directory contains several different kinds of documentation. They should
not all be read as a single live status document.

The source tree is authoritative for what currently builds and runs. Documents
here define architecture, target contracts, implementation plans, decisions,
migration work and validation evidence.

## Start here

For a new developer:

1. Read the repository [README](../README.md) for build/run instructions and the
   project layout.
2. Read [TARGET_IMPLEMENTATION_CONTRACT.txt](TARGET_IMPLEMENTATION_CONTRACT.txt)
   for the distinction between target architecture and legacy/reference evidence.
3. Read [0.1_TARGET_ARCHITECTURE.txt](0.1_TARGET_ARCHITECTURE.txt) for the
   intended system architecture.
4. Read [0.3_CODING_AND_DEPENDENCY_RULES.txt](0.3_CODING_AND_DEPENDENCY_RULES.txt)
   before changing public boundaries or dependencies.
5. Use [0.0_NATIVE_MANUAL_INDEX.txt](0.0_NATIVE_MANUAL_INDEX.txt) only when you
   need the detailed numbered implementation manual.

For agent-assisted development, also read [../AGENTS.md](../AGENTS.md).

## What each documentation area means

### Numbered manual: `0.x` through `22.x`

The numbered `.txt` files are the implementation manual and target design.

They describe required contracts, algorithms, acceptance criteria and planned
integration steps. They are useful when implementing or reviewing a subsystem,
but they are not a live feature list.

A chapter being authored or marked `READY` means the chapter is specified. It
does **not** by itself prove that the current native implementation is complete,
tested on the current commit or production-ready.

Use [0.0_NATIVE_MANUAL_INDEX.txt](0.0_NATIVE_MANUAL_INDEX.txt) to navigate the
full manual.

### Normative contracts

The most important repository-wide contracts are:

- [TARGET_IMPLEMENTATION_CONTRACT.txt](TARGET_IMPLEMENTATION_CONTRACT.txt) —
  what defines the native target and how reference material may be used;
- [0.1_TARGET_ARCHITECTURE.txt](0.1_TARGET_ARCHITECTURE.txt) — target
  architecture;
- [0.3_CODING_AND_DEPENDENCY_RULES.txt](0.3_CODING_AND_DEPENDENCY_RULES.txt) —
  coding and dependency boundaries;
- [0.4_BENCHMARK_AND_DETERMINISM_POLICY.txt](0.4_BENCHMARK_AND_DETERMINISM_POLICY.txt)
  — benchmark/determinism policy;
- [SOURCE_PROTOTYPE_AND_MOCKUP_CONTRACT.txt](SOURCE_PROTOTYPE_AND_MOCKUP_CONTRACT.txt)
  — how legacy/prototype evidence is classified.
- [infantry-js-parity.md](infantry-js-parity.md)
  — JS mockup purpose and the non-gating status of exact infantry comparisons.

### Architecture diagrams

`architecture/` contains the maintained architecture diagram sources and
exports.

Use these for a high-level view, then verify concrete dependencies against the
current CMake target graph and source tree.

For the simulation order/controller/animation boundary, see
[architecture/ENTITY_CONTROL_FLOW.md](architecture/ENTITY_CONTROL_FLOW.md).

### Decisions: `decisions/`

ADRs record architectural decisions and their reasoning.

Currently relevant migration decisions include:

- [decisions/ADR_LIGHTWEIGHT_TOOLKIT.md](decisions/ADR_LIGHTWEIGHT_TOOLKIT.md)
- [decisions/ADR_INFANTRY_STACK.md](decisions/ADR_INFANTRY_STACK.md)

[decisions/ADR_THREEPP_PRESENTATION.md](decisions/ADR_THREEPP_PRESENTATION.md)
is historical/superseded context. The current production presentation target is
Diligent/D3D12; do not use the old threepp migration material as current build
guidance.

For investigating Windows GPU resets, see
[D3D12_DEVICE_LOSS_DIAGNOSTICS.md](D3D12_DEVICE_LOSS_DIAGNOSTICS.md).

### Migration and upgrade work

`migration/` and `upgrades/` contain focused work-package notes, commands,
acceptance criteria and status files.

For the current lightweight renderer/toolkit and infantry work, the useful
entry points are:

- [migration/lightweight-toolkit/POSTEP.txt](migration/lightweight-toolkit/POSTEP.txt)
- [migration/lightweight-toolkit/KOMENDY.txt](migration/lightweight-toolkit/KOMENDY.txt)
- [upgrades/infantry-stack/STATUS.txt](upgrades/infantry-stack/STATUS.txt)
- [upgrades/infantry-stack/ACCEPTANCE.txt](upgrades/infantry-stack/ACCEPTANCE.txt)

`migration/threepp/` is retained as historical migration evidence and should
not be used as the current renderer plan.

### Validation reports: `reports/`

Reports record evidence for a specific implementation slice or validation run.

They are preferable to prose claims when answering questions such as:

- what was tested;
- what parity fixture was used;
- what remained unverified;
- what commit/work package a result referred to.

Reports are snapshots. A report from an older commit does not prove that the
same test still passes on current `main`.

### Parity and reference evidence

`parity/`, `reference/` and selected reports describe comparisons against
legacy/reference behavior.

The native C++ architecture remains authoritative. Reference material may
supply fixtures, expected invariants and visual evidence; it must not silently
reintroduce legacy runtime architecture.

See [../reference/README.txt](../reference/README.txt).

### Performance documentation

`performance/`, benchmark-related numbered chapters and benchmark reports
describe performance methodology and recorded measurements.

Treat measurements as valid only for the documented build, input and hardware
conditions.

### Status files

Files such as [MANUAL_STATUS.txt](MANUAL_STATUS.txt) and work-package
`STATUS.*` files are dated snapshots.

They are useful for historical progress tracking, but they can lag the source
tree. In particular, roadmap percentages must not be interpreted as a current
automated measure of repository completeness.

## Choosing the right document

| Question | Start with |
| --- | --- |
| How do I build or run the project? | [../README.md](../README.md) |
| What is the intended architecture? | [0.1_TARGET_ARCHITECTURE.txt](0.1_TARGET_ARCHITECTURE.txt) |
| What is normative vs legacy evidence? | [TARGET_IMPLEMENTATION_CONTRACT.txt](TARGET_IMPLEMENTATION_CONTRACT.txt) |
| What are the dependency/coding rules? | [0.3_CODING_AND_DEPENDENCY_RULES.txt](0.3_CODING_AND_DEPENDENCY_RULES.txt) |
| How should a subsystem eventually work? | Relevant numbered manual chapter |
| Why was an architectural choice made? | `decisions/` ADR |
| What is being migrated right now? | Relevant `migration/` or `upgrades/` status |
| What was actually validated? | Relevant `reports/` file plus current tests |
| Where are legacy fixtures/evidence? | `../reference/` |

## Documentation maintenance

Keep the root README stable and operational. Do not turn it into a changelog,
roadmap percentage report or exhaustive list of every implemented class.

When documenting a change:

- put stable architecture/contracts in the appropriate normative document or ADR;
- put implementation-specific acceptance evidence in `reports/`;
- put temporary migration sequencing in `migration/` or `upgrades/`;
- update numbered manual chapters when their target contract changes;
- keep build commands in the root README and `CMakePresets.json` aligned;
- clearly mark superseded documents instead of letting historical instructions
  look current.

Documentation should distinguish **target design**, **current implementation**
and **verified evidence**. Avoid collapsing those three concepts into a single
progress claim.
