# Ballistics calibration checkpoints

Values from the simulator below describe the current game profiles. They are
derived from declared preset inputs; they are not measurements of named rounds.
The published experiments do not identify matching casing mass, explosive,
construction, impact geometry, and environment for the four HE presets.

## HE fragments

The model uses 4 MJ/kg as a game energy value, assigns 25% of that energy to
radial fragment motion, assigns 30% of the non-explosive projectile mass to the
fragment set, and spreads mass by a seeded ±20% weighting. Table speeds use the
mean fragment mass before that spread. Energy per fragment is the full pattern
budget divided by the requested count. Inherited projectile velocity is added
separately and opposite radial pairs cancel their added momentum.

| Game profile | Charge mass | Fragment count | Mean fragment mass | Radial energy / fragment | Mean radial speed | Comparable published test |
|---|---:|---:|---:|---:|---:|---|
| 20-he | 0.012 kg | 24 | 1.35 g | 500 J | 861 m/s | None identified; game range only |
| 30-he | 0.040 kg | 40 | 2.40 g | 1,000 J | 913 m/s | None identified; game range only |
| 40-he | 0.120 kg | 64 | 3.94 g | 1,875 J | 976 m/s | None identified; game range only |
| 105-he | 2.10 kg | 128 | 30.2 g | 16,406 J | 1,042 m/s | None identified; game range only |

Published cased-charge studies show that fragment speed depends on explosive
type and charge-to-casing mass ratio. A separate thin shell experiment reported
a maximum of 2.22 km/s at a charge-to-shell ratio of 2.21; its configuration is
not comparable to these presets. These results support treating fragment speed
as a broad configuration-dependent range, not as validation of one preset.
[Cased-charge fragment model](https://www.sciencedirect.com/science/article/pii/S0734743X17307339),
[thin-shell experiment](https://pmc.ncbi.nlm.nih.gov/articles/PMC10456433/).

## Ricochet measurements

The simulator stores the incidence cosine against the surface normal; the
debugger displays its arccosine in degrees (0° is perpendicular and 90° is
tangent to the surface). The paper describes its tested values as ricochet-plate
tilt angles. The simulated incoming path is horizontal, so the plate tilts of
5° and 25° correspond to 5° and 25° from the surface. The steel comparison uses
the 4 g 5.56 mm game profile against the paper's 4 g SS109 rounds. The test used
Hardox 500 at 1300 MPa and measured velocity at a witness plate about 1.8 m after
the ricochet plate; the simulator values are immediately after contact against
the generic game steel profile. This is a useful narrow calibration checkpoint,
not a complete reproduction of the test.

| Projectile / target | Plate tilt | Source speed in → out | Source energy in → out | Source speed / energy ratio | Model speed / energy ratio | Model departure angle | Fit |
|---|---:|---:|---:|---:|---:|---:|---|
| 4 g SS109 / Hardox 500 | 5° | 908 → 894 m/s | 1,650 → 1,598 J | 0.985 / 0.969 | ≈0.987 / ≈0.974 | ≈1.7° from surface | Close at contact; source includes 1.8 m of post-contact flight |
| 4 g SS109 / Hardox 500 | 25° | 901 → 809 m/s | 1,624 → 1,309 J | 0.898 / 0.806 | ≈0.902 / ≈0.814 | ≈6.0° from surface | Close at contact; source includes 1.8 m of post-contact flight |
| HE 20 / concrete, brick, wood | — | — | — | — | — | — | — | No matching measurement identified; gameplay profile |
| HE 30 / concrete, brick, wood | — | — | — | — | — | — | — | No matching measurement identified; gameplay profile |
| HE 40 / concrete, brick, wood | — | — | — | — | — | — | — | No matching measurement identified; gameplay profile |
| HE 105 / concrete, brick, wood | — | — | — | — | — | — | — | No matching measurement identified; gameplay profile |

The model uses a dedicated steel tangent-retention override on the 5.56 mm
profile. Other ammunition and materials retain their existing gameplay
coefficients. No matching velocity and energy data were identified for
ricochets from concrete, brick, or wood, so these remain gameplay-only cases.
The reported departure angle is a model output; the paper's velocity table does
not provide a paired departure-angle measurement for these shots.
[Ricochet quantification experiment](https://www.sciencedirect.com/science/article/pii/S2214914719312565).

The following material sweep shows the game's 5.56 mm ball response at 915 m/s
(1,674 J) for a 5° plate tilt. Values are calculated from the surface-response
coefficients before angular-rotation coupling; they are not measured outcomes.

| Game material | Source comparison | Model speed in → out | Model energy in → out | Model departure from surface | Confidence |
|---|---|---:|---:|---:|---|
| Steel | SS109 / Hardox rows above; unlike target hardness and downstream sensor distance | 915 → ≈903 m/s | 1,674 → ≈1,630 J | ≈1.7° | Medium for narrow velocity checkpoint; low for full test reproduction |
| Concrete | No matching projectile / concrete ricochet measurement | 915 → ≈656 m/s | 1,674 → ≈862 J | ≈0.7° | Gameplay-only approximation |
| Brick | No matching projectile / brick ricochet measurement | 915 → ≈601 m/s | 1,674 → ≈724 J | ≈0.5° | Gameplay-only approximation |
| Wood | No matching projectile / wood ricochet measurement | 915 → ≈583 m/s | 1,674 → ≈681 J | ≈0.3° | Gameplay-only approximation |

At a 25° tilt the current game thresholds do not produce a surface ricochet for
concrete, brick, or wood; the steel case remains within its ricochet range.
Those material outcomes are deliberately left as gameplay behavior because no
comparable measurements were found.

## End of flight

The demo ground is a horizontal plane at y=0 with no horizontal range boundary.
Crossing it emits `ground-contact` and ends the trace for both projectiles and
fragments. A ground-configured world allows up to 120 s of flight; worlds with
no ground retain a 15 s `tracking-limit`. The trace state displays the end
reason.
