# Runtime settlement architecture

`BuildingPlan` is the source of truth for world buildings. `BuildingRuntime`
keeps that plan, a logical `BuildingCollisionProxy`, and a
`BuildingWorldRepresentation`; it creates the complete `buildGeometry()` model
lazily when a building enters `WORLD`. The same detailed `BuildingInstance` is
reused while the building moves between LODs, and the proxy remains available
as a fallback if detailed construction fails.

World rendering uses a shared box geometry and cached materials for `FAR`.
`WORLD` hides that proxy and shows the detailed model, including facade details,
stairs, roof elements and furniture. Furniture stays in the plan for navigation
and destruction metadata and is also rendered by the close-range model.
Damage chunks hide matching components in both representations and retain that
state across LOD changes.

Logical projectile broadphase is handled by `BuildingSpatialIndex`, followed by
local-space queries on `BuildingCollisionProxy`. `BuildingDamageChunkManager`
materializes only deterministic wall/slab/roof chunks after a hit. Virtual
support anchors protect the active boundary until an adjacent logical support
is activated and destroyed.

Ownership is explicit: the Battlefield owns runtimes and the representation
manager; a runtime owns its representation, collision proxy, and damage chunk
manager; `WorldDestructionHost` owns the shared queue and physics/rubble
services. Battlefield disposal releases these in reverse order.

## Ballistics integration

Gameplay code should create the authoritative projectile world with
`battlefield.createBallisticsWorld(options)`. It shares the host
`MaterialModel` and uses `WorldBallisticsBridge` for logical broadphase. A
swept segment that misses materialized parts queries the building index,
activates only the touched AP chunk (or local HE region), flushes that
activation, and repeats the exact material trace. Building Lab continues to
construct `BallisticsWorld` directly, so its detailed path is unchanged.
