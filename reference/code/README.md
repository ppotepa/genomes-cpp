# Genomes — procedural RTS prototype

Current prototype: **v0.22.0** · building generator **building-6.0.0** · destruction **projectile-4.0.0 + rubble-1.1.0**.

Browser-based procedural RTS laboratory. The runtime uses classic JavaScript scripts and Three.js r128 from a CDN. No build step or local server is required: download the source, extract the complete folder and open `index.html`.

## Labs

Projectile energy, contact-fuzed HE and energy-scaled traces: [contracts](docs/destruction/contracts.md) and [manual acceptance pending](docs/destruction/energy-acceptance.md). Rebuild source hashes with `node tools/rebuild-manifest.cjs`.

- `index.html` — main infantry, animation, equipment and weapon laboratory.
- `building-lab/index.html` — standalone procedural-building laboratory with layered wall assemblies, detailed window/door assemblies, four stair families with exact landings, procedural roof/facade equipment, cutaway inspection and destruction-ready structural components.

Building research: [methods, architecture atlas and annotated sources](building-lab/RESEARCH.md) — groundwork for extending the generator, including tenements, apartment blocks, industrial buildings, roofs, materials and interiors.

Building Lab now uses a validated, renderer-independent BuildingPlan for four European patterns, shared floor plans and geometry, analytical roofs, opening-aware structure/details, exact stair flights/landings and clearance-aware tables/cabinets. [Module/API guide and manual acceptance cases](docs/research/building-generator.md). Older building types remain explicitly experimental.

The four object editors include **DESTRUCTION DEMO**, an FPS test range with seven weapon profiles, shared ballistics, copied targets and local Rapier physics. [Controls, tests and limitations](docs/destruction/README.md), [data provenance](docs/destruction/data-sources.md), [building damage research](docs/destruction/building-damage-research.md).

Building destruction uses bounded wall/slab sections: ballistic AP perforation changes both visible geometry and collision, while broken sections fall as finite rubble. Full-building checks cover all four supported patterns; rendered Chrome checks and CPU benchmarks are documented separately from manual 60 FPS acceptance.

Destruction v3.2 keeps each projectile as a persistent simulated object: ricochets and glancing impacts alter its direction and state, penetrations can deflect the exit trajectory, severely degraded rounds can shed bounded secondary fragments, and entry/exit damage is represented by a bounded tapered channel. HE has pooled flash, dust and ballistic fragments; stopped impacts leave material-specific scars. Structural links can weaken locally and detached rubble receives point impulses. Settled debris is no longer deleted by pool pressure: hero Rapier bodies demote to cheap airborne debris or bake into a persistent material-preserving rubble field with heightfield-first batched static colliders, terrain-aware surfaces, ballistic cover, representation accounting and navigation hooks. [Rubble field architecture](docs/destruction/rubble-field.md) and [effects/stability](docs/destruction/impact-effects-and-stability.md) describe the implementation and limits.

## v0.12 — continuous posture and locomotion

The infantry editor exposes independent posture-depth and requested-speed controls. The standing, walking, running and crouching presets use one continuous biped locomotion family. Prone/crawling and sitting retain their separate support modes.

The project preserves deterministic body/face genomes, procedural equipment, ten infantry loadouts, facial expressions, head look and the existing animation laboratory.

The last 30% of standing speed now blends into a sprint: stronger alternating arm drive, relaxed fingers, inward-facing palms, torso counter-rotation and earlier heel recovery. The RUN preset reaches full sprint; the editor shows its current weight. Foot contacts and gait phase remain continuous. See [implementation and validation](docs/18_SPRINT.txt).

## Layout

- `index.html`, `css/`: main application and editor interface.
- `building-lab/`: standalone procedural-building generator and interactive mockup.
- `js/buildings/`: reusable procedural-building runtime intended for later world integration.
- `js/`: classic-script modules for world, anatomy, animation, equipment and UI.
- `docs/`: versioned design and implementation notes.
- `tests/`: small technical checks; numerical checks are not a substitute for browser/WebGL testing.

An internet connection is required for the pinned Three.js and OrbitControls CDN dependencies. Do not disable browser security or mix files from different releases.
