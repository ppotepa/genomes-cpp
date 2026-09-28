# Fast viability plan — native Genomes

Cel tej fazy: w możliwie krótkim czasie uzyskać uruchamialny przekrój
`menu -> world -> battlefield -> render`, a dopiero potem wracać do pełnych
testów zgodności, benchmarków i hardeningu. Rozdziały `READY` traktujemy jako
listę wymagań, nie jako 128 osobnych zadań.

## Zasady wykonania

- Priorytetem jest kod, kompilacja i ręczne uruchomienie aplikacji.
- Pełny CTest, golden images i formalne parity gates są odłożone.
- Każdy agent kończy pracę minimalnym smoke buildem własnego targetu.
- Żaden agent nie może wprowadzić obowiązkowej zależności CUDA, Diligent ani
  SDL do profilu headless/CPU.
- API między torami ma być backend-neutralne; typy bibliotek pozostają w
  adapterach.
- Każdy agent zapisuje krótką notatkę: zmienione pliki, komenda build/run,
  znane ograniczenia i następny handoff.

## Agent A — graphical viability / main menu

Zakres:

- uruchomić `genomes_game` z SDL3 i Diligent w profilu Windows;
- domknąć `GameApplication`, `SceneDirector` i scenę menu jako prawdziwe okno;
- podłączyć wejście: wybór `Battlefield`, `UnitLab`, `BuildingLab`, `WorldConfig`;
- zachować CPU/headless fallback przy braku SDL/Diligent;
- dostarczyć jeden ręczny scenariusz: start aplikacji, kliknięcie/klawisz,
  zmiana sceny, powrót do menu.

Nie robić teraz: pełnego shader parity, pixel-perfect screenshotów, instalatora.

Handoff: działający executable i zapisane opcje uruchomienia.

## Agent B — procedural world viability

Zakres:

- spiąć terrain, hydrology, roads, city/settlement, parcels, buildings,
  vegetation i streaming w jeden `WorldScenario`;
- zapewnić jeden seed i jeden request tworzący obserwowalny świat;
- podłączyć cache/async generation tylko tam, gdzie nie blokuje pierwszej sceny;
- wykorzystać istniejący `WorldStreamer`, `WorldSave` i `ProcViewer`;
- wyświetlić co najmniej teren, drogę, budynek i roślinność w battlefieldzie.

Nie robić teraz: pełnej hydrologicznej dokładności, wieloregionowej persystencji,
optymalizacji 4096².

Handoff: `WorldScenario::startNew(seed)` oraz snapshot/render artifact używany
przez scenę battlefield.

## Agent C — simulation/combat viability

Zakres:

- złożyć fixed tick, ECS/chunk storage, infantry, weapons, ballistics,
  destruction i AI w jeden minimalny scenariusz 25×25;
- doprowadzić przepływ: spawn -> perception -> intent -> fire -> projectile ->
  impact -> damage -> death;
- używać istniejących broadphase, LOS, tactical AI, squad signals i AI pipeline;
- wystawić komendę `startBattlefieldScenario()` dla aplikacji i headless;
- zapewnić ograniczoną liczbę jednostek i prostą kamerę/debug overlay.

Nie robić teraz: pełnej wielowątkowej optymalizacji, ragdoll fidelity i dużych
scenariuszy destrukcji.

Handoff: jedna funkcja uruchamiająca bitwę oraz snapshot stanu dla renderera.

## Agent D — render/Diligent/compute viability

Zakres:

- domknąć Diligent backend, render graph, GPU scene i presentation extraction;
- podłączyć proceduralnie generowane meshe, materiały i instancje do battlefieldu;
- użyć CPU culling/presentation jako fallbacku, a compute/HLSL jako optymalizacji;
- utrzymać `FieldAtlas`, visibility batch, influence field, flow field i backend
  selector jako opcjonalne ścieżki;
- CUDA interop pozostawić capability-gated — brak zgodnego urządzenia oznacza
  automatyczny fallback.

Nie robić teraz: obowiązkowego CUDA, pełnego async-compute scheduling, shader
permutation explosion.

Handoff: jedna klatka battlefieldu renderowana w Diligent oraz log capabilities.

## Agent E — platform/build/release viability

Zakres:

- naprawić presety `dev-debug`, `dev-release`, `GENOMES_ENABLE_CUDA=AUTO/OFF`;
- domknąć `rundev` i `run-release`, tak aby w razie potrzeby konfigurowały i
  przebudowywały projekt;
- usunąć duplicate/missing CMake targets po pracy innych agentów;
- przygotować minimalny pakiet: executable, DLL/runtime, shadery i fixtures;
- uruchomić smoke command na Windows bez pełnego Visual Studio IDE.

Nie robić teraz: podpisywania, sklepowego instalatora, CI matrix dla wszystkich
platform.

## Agent F — deferred production hardening

Zakres po uzyskaniu działającego okna:

- MemoryTelemetry i wysokie limity;
- thread-scaling gate;
- GPU performance gate;
- pełna macierz stress benchmarków;
- crash recovery, cache regeneration i failure injection;
- packaging/release acceptance.

Ten tor nie blokuje pierwszego uruchomienia. Może pracować równolegle, ale jego
zmiany nie mogą destabilizować Agentów A–D.

## Kolejność i bramka viability

1. Agent E stabilizuje konfigurację i build.
2. Agent A uruchamia okno/menu.
3. Agent B dostarcza świat do sceny.
4. Agent C dostarcza symulację bitwy.
5. Agent D podłącza render battlefieldu.
6. Agent F rozpoczyna hardening dopiero po ręcznym przejściu scenariusza.

Minimalna bramka sukcesu tej fazy:

```text
rundev.ps1
  -> opens main menu
  -> selects battlefield
  -> generates deterministic seeded world
  -> shows terrain/buildings/vegetation
  -> runs at least one visible unit interaction
  -> returns to menu or exits cleanly
```

Jeżeli GPU/SDL nie jest dostępne, ten sam scenariusz musi przejść w
`genomes_headless` i `genomes_proc_viewer`; nie traktujemy braku konkretnego
backendu jako blokady architektury.

## Raportowanie postępu

Po każdym zamkniętym torze raportujemy:

- `viability`: `not started / compiling / runnable / blocked`;
- procent całego roadmapu — orientacyjny, nie liczba testów;
- pierwszy uruchamialny artefakt;
- jeden konkretny blocker, jeśli istnieje.

Formalne testy i parity wracają dopiero po przejściu bramki viability.
