# Scene UI / Unit Lab — code review

## Zakres

- osiem tras RmlUi korzystających ze wspólnego theme;
- granica `RML → UiEvent → Scene::handle_ui_action`;
- model piechoty, ekwipunek i prezentacja skinned;
- transport animacji i przejścia locomotion;
- kontrakty responsywnego viewportu oraz testy źródłowe.

## Ustalenia i wdrożenia

1. **Geometria UI** — wspólny theme ma kwadratowe narożniki, zwartą wysokość
   kontrolek, jedną kolumnę etykiet i wspólny układ pól. Brak `border-radius`
   jest wymuszany przez guard.
2. **Formularze** — `.field`, `.range-row`, `.check-row` i komunikaty pomocnicze
   korzystają ze wspólnej osi kontrolki. Unit Lab używa lokalnej kolumny 104dp,
   pozostałe trasy 112dp.
3. **Select popup** — oba warianty drzewa elementów RmlUi (`selectbox` oraz
   `select selectbox`) wymuszają jedną pełną kolumnę opcji. Opcje generowane
   dynamicznie wystawiają natywny `value` przez `data-attr-value`.
4. **Wartości tekstowe** — wartości są publikowane przez `{{ value }}`;
   niebezpieczne `data-bind="text:"` jest zabronione statycznie.
5. **Zdarzenia** — każdy route-level `data-control` ma gałąź `stable_id()` w
   odpowiadającej scenie. Guard obejmuje Unit Lab, World Lab, World Config,
   Building Lab i Settings.
6. **Akcje kliknięć** — każdy `data-action` ze wszystkich tras i galerii jest
   sprawdzany względem pełnej powierzchni routerów scen/aplikacji.
7. **Model piechoty** — asynchroniczne żądania są wersjonowane, stary model
   pozostaje widoczny do poprawnego commit, a fallback viewportu używa tych
   samych breakpointów chrome co RML.
8. **Ekwipunek i broń** — istniejący transportowy ribbon pozostaje zachowany
   dla parity fixture’ów. Dodatkowo katalogowana broń główna jest generowana
   jako pełna geometria i skinnedowana do socketu `WeaponBack` / kości
   `SpineUpper`, więc jest widoczna i podąża za animacją.
9. **Animacja** — UI ma scrub fazy, reset, krok wstecz, krok naprzód, pause i
   speed. `LocomotionController` zachowuje sprężynowane przejścia Walk/Run/
   Crouch; zmiana presetów nie wymusza natychmiastowego teleportu pozy.
10. **FPS w nagłówku** — główna pętla aplikacji liczy wygładzony FPS z czasu
    rzeczywistych klatek i publikuje go jako wspólne pole `fps`; wszystkie
    trasy pokazują odczyt w nagłówku lub overlayu pauzy.
11. **Plain scalar controls** — existing command-backed values such as seed,
    side, toggles and wear remain typed `UiDataModel` scalars where the scene
    owns validation. `UiRuntime::apply_event` preserves their scalar type,
    consumes invalid input, and publishes only on the control commit phase so
    those controls cannot bypass the typed event boundary or become inert.
12. **Explicit operation** — Building Lab accepts the damage amount on slider
    change but keeps the destructive operation explicit behind Apply Damage;
    the action also reads the current `UiDataModel` draft defensively.

## Dowody źródłowe

- `tests/native/scene_ui_guard.cmake` — kontrakty RML/theme/handlerów;
- `tests/native/ui_rml_smoke.cpp` — dropdowny, wartości i osie formularzy;
- `tests/native/unit_lab_smoke.cpp` — viewport, broń, transport animacji i
  publikacja wartości;
- `tests/native/ui_data_model_tests.cpp` — plain scalar event forwarding and
  commit-phase coverage;
- `tests/native/infantry_presentation_tests.cpp` — cache/prototyp/skinning;
- `docs/upgrades/scene-ui/STATUS.md` — aktualny stan odbioru wizualnego.

## Pozostałe bramki odbioru

- kompilacja po aktualnych zmianach;
- CTest runtime/UI/platform;
- wizualny odbiór GPU dla 1280×720 i 1920×1080 przy 75/100/150%;
- sprawdzenie otwartego dropdownu, suwaka oraz pozycji modelu na backendzie
  Diligent/D3D12.

Te bramki są celowo pozostawione do uruchomienia przez właściciela projektu.
