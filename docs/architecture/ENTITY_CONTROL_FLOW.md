# Entity control flow

Entity control is a simulation boundary, not a renderer or AI framework. It
lets different entity types share order arbitration and batch-update plumbing
while keeping their movement and action rules domain-specific.

```text
AI / player input / script
          | EntityOrder
          v
simulation order arbitration
          |
          v
EntityController implementation (one entity type)
          | EntityControlCommand
          v
authoritative simulation state / physics commands
          |
          v
render-state extraction -> animation and presentation
```

`EntityOrder` carries entity-neutral intent: kind, target or destination,
priority, source, and issue/expiry ticks. `resolveEntityOrder` chooses a stable
winner by priority, then newest issue tick, then lowest stable source ID. An AI
system decides *what* the entity should attempt; it submits an order and does
not directly edit transforms or animation state.

`simulation::EntityController` consumes read-only entity state and an order,
then writes an `EntityControlCommand` for the caller to commit during the
authoritative simulation phase. Implementations must be deterministic for the
same state, order, and tick context. They do not own entity storage, physics
bodies, or renderer objects. A different domain (vehicle, creature, building)
can implement its own controller without inheriting infantry movement rules.

`infantry::InfantryUnitController` is the infantry implementation. It resolves
movement intent into bounded turning, acceleration, velocity, position, and a
stable action ID. `InfantrySimulation` commits commands to its entity store or
physics command buffer. The mass-battle runtime sends deterministic,
seed-derived orders through the same controller interface and commits its
results after its parallel update phase.

Action IDs are hints to the presentation bridge, not renderer commands. The
bridge maps infantry actions and state to `LocomotionController` transitions
and weapon overlays. Posture changes use non-immediate transitions so standing,
crouching, and going prone blend through the authored animation system. AI and
simulation remain renderer-independent; the bridge can change without changing
the order or controller contracts.

Keep future extensions at the narrowest boundary:

- add decision policies to AI/input systems and express their decisions as
  orders;
- add entity-specific locomotion rules in that type's controller;
- add animation or equipment presentation in the presentation bridge;
- introduce shared entity data only when more than one entity type needs the
  same semantics.

The abstraction is intentionally smaller than a universal `Entity` object
model: ECS storage stays authoritative, and entity types do not need a common
inheritance hierarchy just to share simulation control.

The menu's `Battlefield` entry is the small tactical encounter (currently two
opposing actors); `Infantry Mass Battle` is the seeded crowd simulation
(default 1,000 actors per team) where roaming goals and changing posture/action
variants are exercised. These are separate scene modes, so crowd-style
behavior should be evaluated in the latter rather than inferred from the
two-actor tactical fixture.
