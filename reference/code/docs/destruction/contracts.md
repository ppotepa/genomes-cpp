# Destruction contracts

The current contracts are `projectile-state-3`, `projectile-trace-3`,
`material-assembly-1`, and `building-adapter-2`.

Projectile snapshots retain quaternion orientation, angular velocity, inertia,
deformation, integrity, stability, and effective diameter. Trace records keep
`traceId`, `shotId`, and `parentTraceId`; fragments are child traces rather than
geometry appended to the primary trace.

Assemblies expose `assemblyId`, `physicalSolidId`, a local material frame,
ordered layers, explicit air gaps, and deterministic material-field
metadata. Layer traversal is sequential. A void contributes path length but no
solid resistance. Technical mesh subdivision sharing a `physicalSolidId` does
not create a new physical interface.

All resistance, penetration, damage, and breakup coefficients are game-model
parameters. They are not certified real-world penetration data, FEM results, or
V50 tables. Local material sampling is seed-stable and rotation-equivariant.

## Energy and contact state

World translational energy is calculated from current mass and full velocity:
`E = 0.5 * mass * dot(v, v)`. Unit directions are separate. Contact inputs use
`v - (targetLinearVelocity + targetAngularVelocity × contactLever)`.
The solver returns full world velocity, world angular velocity, projectile
impulse and target impulse/torque. Inertia uses local Z as `bodyForward` and is
recomputed after mass or diameter changes. Angular velocity is rescaled after
diameter deformation to preserve the solver's rotational energy.

Layer `start`/`end` are distances along `materialFrame.normal` from its world
`origin`, located on the first layer's surface. Mesh-local frames are transformed
by the adapter and follow detached bodies. Missing frames on simple fixtures
default to the positive thinnest bounds axis. Intersections are sorted by distance
along the shot; undeclared gaps and explicit `void`/`air` intervals have zero
material force. Force is pressure times projectile cross section exactly once;
work is force times actual distance. State retains interval progress between ticks.
Contiguous technical sections share the contact; an actual gap ends it. Each
generated pane has its own physical ID, 9 mm glass interval and local frame.

The contact solver is a bounded game approximation, with prescribed target
motion during each traversal. Target impulse is dispatched separately from
damage so moving debris does not also receive the old heuristic impact impulse.
The projectile ledger includes signed flight/rotation/target contributions and
energy assigned to material, fragments and unresolved matter. `balanceError`
reports its residual. It does not measure the complete Rapier scene's energy.
`ground-contact` is a physical end event when an optional horizontal ground plane
is configured; it applies at every horizontal position and produces a trace
contact marker. `tracking-limit` is the configurable observation timeout and
remains the end reason when no physical contact occurs before it.

## HE

Generic HE presets declare `fuze: {mode: 'contact', armed: true}`. A broadened
collision candidate must be checked against the actual convex surface; the fuse
uses its surface point. A finite nose-crush work budget (one quarter diameter)
precedes detonation. Blast uses explosive energy separately; fragments inherit
remaining body velocity. Opposite fragment pairs cancel radial momentum and the
translation/expansion cross term. Capacity limits omit complete pairs without
increasing survivor energy. Omitted/unrepresented energy is recorded explicitly.
Each ammunition entry may expose `fragmentation` (`profile`, `fragmentCount`,
`bodyMassFraction`, `explosiveEnergyFraction`, `massSpread`); legacy `fragments`
remains supported. Optional `ricochetProfile.materials` holds narrow
ammunition/material overrides; the 5.56 mm steel checkpoint does not alter the
shared steel or other ammunition profiles.

Blast can alter solid geometry and supports, and is not routed through the small
fragment remeshing shortcut. `blast-breach` counts geometric through-cuts;
`geometryChanges` also counts fractures and craters. These differ from the body's
`detonated` contact and a kinetic `penetrated` contact. Detached hero and cheap
solid debris remain visible and traceable until replaced by baked rubble or
removed by further damage. Arming mechanisms and delayed/programmed fuzes are
outside this implementation.

## Trace and presentation

Main `traceId` equals `shotId`; child traces have independent IDs and explicit
`parentTraceId`. Every segment has `energyFrom`, `energyTo`, and legacy `energy`
as an alias of `energyTo`; `initialEnergy` belongs to that trace, not its parent.
Material travel is recorded. Compression checks the complete retained polyline,
accumulated direction change and energy interpolation error, and preserves
material/contact boundaries and discontinuities. Finishing a trace is idempotent.

History retains ten groups in main-shot order; adding a fragment does not reorder
groups. HE has a separate visible-group limit of 1 (default), 3 or 10 within that
retained history. “Last” uses the latest main shot and its children. Fragment
visibility filters child records, which are never redrawn in parent geometry.

Camera-facing triangle strips use 1.5–6 CSS pixels at both segment endpoints.
Energy maps green → orange → red. Default scale is `E / initialEnergy`; absolute
scale is `ln(1 + E[J]) / ln(5,000,001)`. Both clamp rendering to [0,1], while
diagnostics retain actual values and indicate overflow. Filters and scale changes
rebuild presentation even after flight; viewport uniforms update during rendering.

Implementation is awaiting the user's [manual acceptance scenarios](energy-acceptance.md).
# ammunition-strategy-1 / material-damage-field-1

`DestructionAmmunitionStrategies` is the calibrated ammunition boundary. Every
registered round has `caliberId`, `strategyId`, `variantId`, `construction` and
`calibration`, while retaining legacy mass, diameter, velocity, kind and HE
fields. Strategies own aerodynamic, contact, material-work and detonation
choices; energy and diameter alone are not a ballistic table.

Each material part owns `damageField` (`material-damage-field-1`): bounded local
cells record crush, cracks, weakness and rear damage, while holes record the
channel and removed volume. `part.damage` remains a diagnostic/load aggregate.
Geometry budgets produce this local representation rather than detaching a
panel. A panel may detach only through structural support failure.
