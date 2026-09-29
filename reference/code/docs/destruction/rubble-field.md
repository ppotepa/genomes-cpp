# Persistent Rubble Field v1.1

RubbleField changes destruction material representation instead of deleting settled debris when the Rapier pools fill.

## Lifecycle

building section -> HERO debris (Rapier rigid body) -> CHEAP airborne (simple gravity) -> BAKED rubble (persistent tile volume + static collider).

A hero body may remain sleeping while it rests on an elevated slab. Ground-level or pile-supported sleeping bodies become bake candidates after 60 fixed ticks (about one second). All candidates in the same sync are deposited first, dirty rubble colliders are rebuilt once, and only then are the old rigid bodies removed. This avoids a one-frame collision hole.

Pool pressure no longer means lost building material. If the 128 awake-body budget is full, new debris uses the cheap-airborne path. If the cheap pool is also full, material is deposited directly into the field. Convex-hull failures for microscopic remnants become micro-rubble volume.

## Spatial representation

Default parameters:
- cell: 0.5 x 0.5 m;
- tile: 4 x 4 m (8 x 8 cells);
- maximum retained tiles: 256;
- hero bodies: 512 retained / 128 awake;
- cheap airborne records: 1024;
- settled near-detail chunk instances: 1024.

Each cell stores solid volume and per-material volume. Height is derived from solid volume, cell area and a gameplay packing factor. Brick, concrete, wood, steel, rock and other materials can coexist in one tile. Hard tile overflow never discards volume: the newest deposit keeps its requested world-space tile whenever capacity can be freed, while an older low-volume tile is merged into its nearest neighbor. With a one-tile budget, deposits clamp to that tile instead of modulo-wrapping into unrelated local cells.

A local slope-relaxation pass transfers material to neighboring cells while preserving material composition and total solid volume. This is a cheap pile approximation, not granular-body physics.

## Physics

Dirty tiles produce one static surface collider per tile. The preferred representation is a Rapier heightfield built from the same grid. If heightfield creation is unavailable or rejected at runtime, the system falls back to a low-poly trimesh and finally to a conservative cuboid. Collider rebuilds are batched, and each tile records the collider mode used for diagnostics.

Cheap airborne debris uses gravity and the rubble/ground height instead of one Rapier body per piece. Packet fragments emitted from rubble hits use one pooled InstancedMesh; larger demoted building pieces can retain their existing visual mesh while using the cheap motion path.

## Rendering

The field has a low-poly grid mound per tile plus one shared instanced chunk mesh for near detail. Chunk transforms are regenerated from deterministic tile/cell state, and near-detail chunks sample the local cell material mixture rather than inheriting one tile-wide material. The visual root cancels the copied target transform because rubble geometry and physics are stored in world coordinates.

## Ballistics and explosions

MaterialModel.trace combines building/dynamic-volume hits with a rubble tile query. The rubble query first uses the tile grid as a broad phase; only segments that overlap a non-empty tile sample the height field.

A rubble tile exposes a stable synthetic ballistic part with dominant material, material mixture, packing factor and porous penetration resistance. A hit can move local field volume, immediately redeposit part of it, and emit a bounded amount as cheap airborne packets. Logical cell volume is updated immediately; physical/visual tile rebuilds are deferred to the next physics sync.

HE can excavate nearby tiles and re-eject a bounded fraction of their solid volume as cheap airborne rubble. That material settles back into the field later.

## Navigation hooks

RubbleField exposes heightAt(x,z), surfaceHeightAt(x,z), pileSlopeAt(x,z), slopeAt(x,z), movementCostAt(x,z), coverAt(x,z) and sampleNavigation(x,z). A base-height provider lets the pile sit on nonflat terrain. Movement cost uses rubble-only slope so existing terrain incline is not charged twice, while collision normals and ballistic surface tests use terrain plus pile height. The destruction-demo character controller already scales movement by rubble movement cost. The cover value is a hook for future RTS AI.

## Conservation invariant

For collapse-only scenarios the intended invariant is: hero solid volume + cheap airborne solid volume + baked rubble solid volume ~= detached source solid volume. `DestructionPhysics.representationAccounting()` reports all three buckets plus per-material totals, and `RubbleField.accounting()` reports baked volume/materials/tiles directly.

The dedicated rubble tests require near-exact conservation for pure field operations. The Rapier benchmark also checks representation volume for its synthetic debris-collapse scenario.

## Performance intent

The expensive phase is temporary. A collapse should naturally move from many awake bodies to few/no bodies and a small number of static rubble tiles.

The previous docs/destruction/benchmark.json predates this field and must not be treated as a measurement of the current rubble architecture. The updated benchmark harness reports hero bodies, cheap airborne records, rubble tiles/volume, demotions and bakes. Regenerate the canonical JSON on a machine with the pinned Three/Rapier dependencies before comparing CPU results.

## Known simplifications

- The rubble grid is heightfield-oriented and can follow a supplied base terrain height, but it still does not preserve caves, overhangs or arbitrary elevated rubble shelves.
- Cheap airborne debris does not get full debris-vs-debris rigid-body contact.
- Packing, slope and porous ballistic coefficients are gameplay parameters, not measured engineering data.
- One rubble tile is a simplified surface collider rather than hundreds of individual brick/stone colliders.
- The field belongs to the copied destruction target. Integration with the persistent strategic world is a separate world-lifecycle step.
