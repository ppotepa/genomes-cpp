# Building Lab — generator v6

Generator buduje deterministyczny `BuildingSpec/6`, następnie plan `rts.building-plan/4`, geometrię Three.js i manifest `rts.building-manifest/6`. Nie ma kompatybilności z dawnym genome, layoutem `/3` ani osobną ścieżką rural.

## Moduły

| Moduł | Odpowiedzialność |
|---|---|
| `buildingCatalog.js` | materiały, konstrukcje i katalog pokryć |
| `buildingRandom.js` | nazwane, deterministyczne strumienie seedów |
| `buildingSpec.js` | presety, zakresy i `createBuildingSpec` |
| `buildingFootprints.js` / `buildingPolygon.js` | gramatyka rzutów, walidacja i operacje polygonowe |
| `buildingPrograms.js` / `buildingSolver.js` | RoomGraph i bounded floor solver |
| `buildingAssemblies.js` | ściany, otwory, schody, dach, konstrukcja, nawigacja i meble |
| `buildingPlan.js` | złożenie i walidacja planu `/4` |
| `buildingGeometry.js` | ściany z otworami, stropy z dziurami, schody, dach, meble i metadata destrukcji |
| `proceduralBuildingGenerator.js` | wyłącznie publiczna fasada generatora |

## API

```js
await RTS.Buildings.initializeBuildingBackends();
const generator = new RTS.ProceduralBuildingGenerator();
const spec = generator.createSpec({seed: 240925, presetId: 'FAMILY_HOUSE'});
const plan = generator.createPlan({spec});
const building = generator.generate({seed: 240925, presetId: 'FAMILY_HOUSE'});
building.setVisibleFloor(0);
building.setRoofVisible(false);
building.setNavVisible(true);
const manifest = building.exportManifest();
building.dispose();
```

Dozwolone publiczne grupy generatora to `presets`, `footprintFamilies`, `roomPrograms`, `roofFamilies`, `stairFamilies`, `structuralSystems`, `createSpec`, `createPlan`, `generate` i `createWorldRepresentation`. Awaria straight-skeletonu może wyłącznie dodać jawną diagnozę `SKELETON_BACKEND_FAILED` i deterministyczny compound-region fallback.

## Weryfikacja

```text
node tests/building_v6.cjs
node tests/building_collision_proxy.cjs
node tests/building_damage_chunks.cjs
node tests/building_world_representation.cjs
node tests/building_destruction_v6.cjs
cd tools/destruction && npm test
node tools/destruction/browser-check.cjs  # wymaga lokalnego Playwright + Chrome
```
