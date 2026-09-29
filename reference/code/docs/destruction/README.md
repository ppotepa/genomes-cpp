# DESTRUCTION DEMO

## Current contracts

Detailed contract reference: [contracts.md](contracts.md).
HE fragmentation and ricochet comparison: [ballistics calibration](ballistics-calibration.md).

Destruction contracts are versioned independently of rendering. Projectile snapshots use `projectile-state-3`; trace records use `projectile-trace-3`; generated component assemblies use `material-assembly-1`; and adapter metadata uses `building-adapter-2`. Compatibility fields including `direction`, `axis`, `energy`, `layers`, `material`, `projectileId`, and `shotId` remain available.

Energy, HE and the trace renderer are implemented and awaiting [manual acceptance](energy-acceptance.md). Projectile-state and trace regression suites cover their numerical and trace contracts; browser acceptance remains pending.

Trace records distinguish `traceId`, `shotId`, and `parentTraceId` for child fragments. Assemblies carry `assemblyId`, `physicalSolidId`, a local material frame, ordered layers, and deterministic material-field metadata. Contacts expose ordered layer intervals and moving-target relative velocity. Projectile snapshots retain quaternion orientation and angular velocity. These values are game-model approximations, not certified real-world penetration data.

Przycisk w edytorach piechoty, roślin, skał i Building Lab otwiera scenę
z kopią aktualnego celu. Reset odtwarza uchwycony obiekt, a wyjście przywraca
edytor. Rapier 3D compat 0.19.3 jest lokalnym IIFE z wbudowanym WASM.
Uruchamianie przez `file://` nie wymaga serwera; Three.js edytorów korzysta z CDN.

## Obsługa

- **Graj / wznów** przechwytuje mysz; **Esc** wstrzymuje symulację.
- WASD, Shift, Ctrl i Spacja: ruch, sprint, kucanie i skok.
- LPM strzela, PPM przybliża celowanie. Amunicja **∞**, bez przeładowania;
  każda broń zachowuje swój rytm ognia. Strzelec jest niewrażliwy.
- Dystans początkowy 5–1000 m jest liczony od przedniej granicy celu.
  HUD pokazuje bieżącą odległość do środka obiektu.
- Diagnostyka pokazuje tor pocisku, energię resztkową, trafiony element,
  liczbę pocisków, hero/cheap/baked gruzu i oczekujących przebudów.
- Debugger śladów jest otwarty domyślnie. Przycisk **Debugger śladów** lub
  **F2** przełącza panel i widoczność śladów. Panel zachowuje 10 ostatnich
  zaakceptowanych strzałów; wybór **OFF / Ostatni / 10** filtruje rysowane
  tory, a **Fragmenty** pokazuje niezależne ślady odłamków widocznych grup.
  **Historia HE** ogranicza widoczne grupy HE do 1 (domyślnie), 3 lub 10.
  Skala energii jest procentowa dla każdego pocisku lub logarytmiczna 0–5 MJ.
  Lista pokazuje amunicję, stan końcowy i kontakty z energiami.
- Profil słabszego sprzętu ogranicza rozdzielczość i cząstki wizualne.
  Nie zmienia balistyki ani budżetów materiału i gruzu.

## Balistyka pocisku v4.0

Pocisk zachowuje trwały stan przez cały lot: identyfikator strzału, masę, efektywną średnicę, oś, quaternion orientacji, prędkość kątową, tensor bezwładności, integralność, deformację i stabilność. Rykoszet nie resetuje naboju; zmienia energię, kierunek, orientację i dalszą aerodynamikę deterministycznie z seeda, geometrii i miejsca kontaktu. Silnie zdegradowany pocisk może zrzucić część masy albo rozpaść się na wtórne pociski z osobnymi `traceId`, zachowując wspólny `shotId` i `parentTraceId`.

Każdy fizyczny komponent przekazuje `assemblyId`, `physicalSolidId`, lokalną ramę materiału i uporządkowane warstwy. Pocisk przechodzi przez nie sekwencyjnie: energia jest aktualizowana po każdej warstwie, a jawna szczelina `void` dodaje drogę, lecz nie opór. Techniczne podziały tego samego `physicalSolidId` nie tworzą dodatkowego interfejsu. Usunięto dawny uniwersalny mnożnik AP. Reakcja powierzchniowa i uszkodzenie rdzenia mogą więc różnić się w tej samej ścianie.

Pola materiałowe są próbkowane w lokalnych współrzędnych komponentu. Klucz próbki zawiera seed, `physicalSolidId`, `assemblyId`, komórkę lokalną i kanał; obrót budynku obraca ramę, ziarno i kierunki zbrojenia bez zmiany wartości próbki. Prędkość kontaktu ruchomego komponentu wynosi `linearVelocity + angularVelocity × (contactPoint - centerOfMass)`, a solver używa prędkości względnej pocisk–cel.

Uszkodzenia powierzchni nie korzystają już z jednego kwadratowego wycięcia. Pochłonięta energia jest dzielona na kruszenie/plastykę, front damage, radial stress, rear damage i ejecta. Perforacja ma wąski fizyczny kanał, ale krawędź wejścia i wyjścia jest dodatkowo erodowana bez mnożenia rigid bodies. Cegła utrzymuje wirtualne komórki/spoiny i lokalne osłabienie kolejnych trafień; beton/skała dostają strefę pęknięć/rear scab, stal rozróżnia dent/plug/petal, a drewno orientuje wyrwanie i niewielką deflekcję względem włókien. `exitPoint` steruje tylnym VFX. Jest to nadal uproszczony model gry, nie analiza FEM.

Indeks kolizji został rozdzielony na statyczny budynek i ruchomy gruz. Aktualizacja pozycji gruzu przebudowuje tylko drzewo ruchome; zmiana geometrii lub konstrukcji przebudowuje oba. Reverse-index połączeń `support → dependents` usuwa pełne skanowanie konstrukcji przy każdym odłamku. Swept-sphere używa taniego broad-phase płaszczyzn i dokładnego testu trójkątów tylko przy krawędzi/narożniku. Odcinek lotu jest adaptacyjny (do 12 m dla głównego pocisku), a zdarzenia `flight` są wyłączane poza diagnostyką. Diagnostyka demo pokazuje rykoszety, deflekcje, rozpady, stan pocisku oraz liczbę przebudów BVH.

## Budynki

[Analiza przyczyn i research](building-damage-research.md) opisuje diagnozę,
źródła oraz zakres uproszczeń. [Ubytki, efekty i stabilność](impact-effects-and-stability.md)
opisują dalsze poprawki reakcji materiałów, wybuchów oraz wiszących fragmentów.
Większe ściany, stropy i dachy są przygotowywane
jako sekcje około 1,6 m: do 16 na komponent i do 1024 dodatkowych sekcji celu.
Limity mogą pozostawić większe sekcje. Połacie zachowują skośną bryłę;
ich kolizja nie wypełnia poddasza prostopadłościanem.

AP zużywa energię na rzeczywistej drodze przez materiał. Pocisk może przejść
przez kolejne ściany z energią resztkową. Przebicie usuwa materiał, tworząc
otwór w siatce i w colliderze. Cienka blacha i krucha cegła korzystają z różnych
reakcji materiału; kaliber penetratora jest oddzielony od kalibru lufy.
Pocisk przechodzący przez istniejącą dziurę nie uszkadza ponownie pustej przestrzeni.

Zatrzymany pocisk może zostawić wnękę: metal wgniecenie, drewno wyrwany
fragment, a cegła i beton krater. Próg wnęki metalu i drewna korzysta z
`strength`, średnicy i normalnej składowej zdeponowanej energii. Krater ściany
pozostawia do czterech wypukłych części, bez tworzenia osobnej bryły Rapiera
dla każdego drobnego odprysku. Drobne odłamki HE kumulują obrażenia,
lecz nie wycinają osobnej wnęki przy każdym kontakcie.

Sekcja ma najwyżej dwa poziomy lokalnego cięcia. Dalsze przebicie lub głęboki
wyłom odłącza pozostały fragment. Płytkie wgniecenia metalu i drewna mogą po
wyczerpaniu tego limitu pozostawiać tylko efekt, nadal kumulując uszkodzenie.
Pełne rozbicie tworzy do trzech zamkniętych kawałków;
gruz budynku nie dzieli się rekurencyjnie. Dalsze zniszczenie może go usunąć.
Osiągnięcie limitu geometrii nie czyni ściany niezniszczalną.

Kierunkowy graf kontaktów przenosi podparcie od fundamentów przez elementy
nośne. Relacje podparcia mają własną integralność 0–1 i mogą być lokalnie osłabiane przy trafieniu w styk. Strop lub dach nie zawiesza całej ściany od góry, a ściana osłonowa
szkieletu nie zastępuje słupa. Elementy bez podparcia przechodzą do Rapiera. Impuls od trafienia lub wybuchu jest przykładany w rzeczywistym punkcie kontaktu, więc odłączona płyta może dostać również moment obrotowy.
Przerwane nad otworami końcówki ram mogą wisieć przy dachu jako nienośne
elementy. Detale elewacji należą do sekcji i znikają po utracie właściciela.
Połączenia boczne zużywają ograniczoną rozpiętość podparcia. Przejście ze
ściany na strop i ponownie na ścianę zachowuje zużyty dystans, zamiast
umożliwiać dowolnie długi łańcuch wiszących części. Początkowy projekt
otrzymuje minimalny zapas potrzebny do zachowania jego nienaruszonej bryły.
Graf nie rozwiązuje naprężeń, zginania ani przeciążenia pozostałych podpór.

## Persistent Rubble Field v1.1

Odłączony materiał ma trzy reprezentacje kosztowe: hero Rapier debris, cheap airborne debris oraz baked rubble. Pole gruzu używa komórek 0,5 m i kafli 4 m; przechowuje solid volume oraz udział materiałów, a wysokość wynika z packing factor. Lokalny slope relaxation rozprowadza zbyt strome osady bez granular-body simulation.

Rubble tile preferuje jeden statyczny Rapier heightfield collider i low-poly mound; jeśli runtime odrzuci heightfield, używany jest trimesh, a następnie cuboid fallback. Near detail to wspólny InstancedMesh. Ballistyka widzi stertę jako porowatą mieszankę materiałów; AP może przemieścić część volume i wyrzucić kilka cheap packets, a HE może ponownie wzbić materiał. Tile broad-phase sprawia, że pociski niezwiązane ze stertą nie próbkują siatki wysokości.

API pola udostępnia heightAt, surfaceHeightAt, pileSlopeAt, slopeAt, movementCostAt, coverAt i sampleNavigation. Provider wysokości bazowej pozwala użyć tego samego pola na niepłaskim terenie; movementCost liczy tylko dodatkową stertę, a kolizja korzysta z pełnej powierzchni terrain + rubble. Character controller w demo już korzysta z movementCost. Wartości packing/oporu porowatego są parametrami gry, nie danymi inżynierskimi.

## Symulacja i koszty

Krok balistyki wynosi 1/60 s. RK4 uwzględnia grawitację, gęstość powietrza,
wiatr i parametryczny opór transoniczny. Adaptacyjny odcinek lotu dochodzi do 12 m dla głównego pocisku i 6 m dla odłamka, ale jest skracany przez dopuszczalny błąd krzywizny; cały odcinek nadal sprawdza tor wraz z promieniem pocisku. Kontakt zużywa pozostały czas i energię.
Wybuch najpierw próbkuje osłony, następnie stosuje obrażenia, a jego odłamki
lecą przez aktualną geometrię. Parametry materiałów są parametrami gry,
opisanymi w [data-sources.md](data-sources.md).

BVH zawiera objętości powiększone o ruch w ciągu ticka. Zwykły odcinek lotu
nie przegląda wszystkich części budynku. Zmiany wybuchu i kaskady podparcia
grupują przebudowę indeksu. Adapter grupuje synchronizację colliderów.
Usunięcie części zwalnia jednocześnie jej model, collider i własną siatkę;
zasoby współdzielone z edytorem pozostają nienaruszone.

| Zasób | Limit |
|---|---:|
| Główne pociski | 256 |
| Odłamki balistyczne | 2048 |
| Części budynku | około 3 × liczba początkowa; formuła poniżej |
| Części pozostałych celów | 4096 |
| Kanały penetracji | 8192, do 64 na pierwotną sekcję |
| Zachowane hero-bryły gruzu | 512 |
| Nieśpiące hero-bryły gruzu | 128 |
| Cheap airborne debris | 1024 |
| Rubble tiles | 256 po 4 × 4 m |
| Komórka rubble field | 0,5 × 0,5 m |
| Instancje near-detail sterty | 1024 |
| Błyski wizualne | 16, w niskiej jakości 4 |
| Chmury pyłu i dymu | 128, w niskiej jakości 40 |
| Wizualne odpryski i iskry | 256, w niskiej jakości 80 |
| Ślady trafień | 128, w niskiej jakości 32 |

Budżet budynku to `max(N+1, min(2048, max(256, 3N)))`, gdzie `N` oznacza
początkową liczbę sekcji. Zapobiega wielokrotnemu wzrostowi liczby siatek po
długiej serii. Wyjątkowo duży cel zachowuje swoje początkowe części i miejsce
na podłoże, ale nie dostaje dodatkowego budżetu podziału. Po osiągnięciu
limitu dalsze zniszczenie odłącza sekcję.

Śpiąca bryła może nadal obudzić się po utracie podparcia, ale tylko dopóki jest na konstrukcji. Bryła śpiąca przy ziemi lub istniejącej stercie przez około 1 s staje się kandydatem do bake. W jednej transakcji wszystkie kandydaty przekazują objętość do RubbleField, dirty tile colliders są przebudowywane, a dopiero potem stare rigid bodies są usuwane. Pełny limit 128 aktywnych hero bodies degraduje nowy gruz do cheap-airborne zamiast go usuwać. `representationAccounting()` pokazuje objętość hero / cheap / baked oraz skład materiałowy, co pozwala sprawdzać conservation bez ręcznego sumowania rekordów. Po zapełnieniu cheap pool materiał trafia bezpośrednio do sterty. Szczegóły: [rubble-field.md](rubble-field.md).

Mikroskopijne pozostałości, których natywna otoczka Rapiera nie może reprezentować, są zamieniane na micro-rubble volume przed usunięciem starej reprezentacji. Licznik `physics.metrics.unrepresentableHulls` pozostaje diagnostyką problemu hull, a `rubble.metrics.microDeposits` potwierdza zachowanie materiału.
Kontrola limitu nieśpiących brył obejmuje również przebudowy colliderów,
które mogą obudzić gruz już po kroku fizyki.

Kolejka siatek otrzymuje około 2 ms na klatkę; pojedyncza operacja może trwać
dłużej. Oczekująca przebudowa wstrzymuje symulację bez naliczania długu czasu
rzeczywistego. Demo wykonuje najwyżej dwa ticki w klatce. Pod obciążeniem czas
symulacji może płynąć wolniej; zaakceptowane strzały nie są pomijane.
Koszt samego kroku balistycznego lub Rapiera nadal może przekroczyć 16,7 ms.

Wybuch ma krótki błysk, materiałowy pył i szybkie odpryski. Efekty używają
czterech pul instancji i jednej proceduralnej tekstury 64×64, bez dodatkowych
świateł, cieni ani ciał Rapiera. Aktualizacja odbywa się raz na aktywną klatkę
według czasu renderowania, również gdy przebudowa celu zatrzymuje ticki
symulacji. Wygasłe instancje są usuwane z aktywnej listy; puste pule nie
przesyłają danych na GPU. Ślad znika wraz z jego sekcją, bez odroczonego
śledzenia promieni. Maksimum to cztery wywołania renderowania efektów;
przezroczysty dym oglądany z bliska nadal wymaga pomiaru kosztu GPU.

## API

`solid.js`, `materialModel.js` i `ballistics.js` nie wymagają DOM ani Three.js.
Używają jednostek SI i wektorów `[x,y,z]`.

```js
const model = new RTS.MaterialModel([
  {id: 'wall', min: [-2,0,10], max: [2,3,10.25], material: 'brick'}
], 123);
const world = new RTS.BallisticsWorld({model});
world.fire({id:'shooter:1', weapon:'mk44', ammo:'30-ap',
  position:[0,1.6,0], direction:[0,0,1], tick:world.tick, seed:123});
world.step(1 / 60);
world.dispose();
model.dispose();
```

`fire()` sprawdza zgodność broni i naboju, identyfikator, budżet oraz opcjonalny
odcinek kamera–wylot lufy. `hit` podaje energię wejściową, stratę, energię
resztkową i pełną grubość kontaktu. Zawiera również materiał, normalną,
kierunek, średnicę, rodzaj amunicji, seed i wynik zmiany powierzchni
(`response`, `geometryChanged`, opcjonalne `radius` i `scarDepth`). Efekt
trafienia korzysta ze straty `lost`, a nie z energii resztkowej `energy`.
`explosion` przekazuje również materiał, normalną, kierunek i dane pocisku.
`model.remove(part)` usuwa część i jej
powiązania; `model.batch(callback)` grupuje przebudowę indeksu.

`DestructionEffects` przyjmuje `impact(event, low)` i `explosion(event, low)`.
Gospodarz wywołuje `update(renderDt, low, camera, model)` raz na aktywną
klatkę. `reset()` opróżnia pule, a `dispose()` zwalnia ich siatki, materiały
i wspólną teksturę. Parametry efektów nie zmieniają trafień ani energii.

`RTS.DestructionDemo.open({renderer, controls, target, suspend?, resume?})`
przyjmuje adapter `infantry`, `plant`, `rock` albo `building`. `open/reset`
są asynchroniczne, `close` jest idempotentne. Gospodarz wywołuje
`frame(renderer, now)` zamiast własnego renderu, gdy metoda zwróci `true`.
`diagnostics` zawiera liczniki sesji. Nie powstaje druga pętla RAF ani renderer.

## Weryfikacja

```text
cd tools/destruction
npm ci --ignore-scripts
npm run bundle
cd ../..
node tests/destruction_core.cjs
node tests/destruction_projectile.cjs
node tests/destruction_spatial.cjs
node tests/destruction_rubble.cjs
node tests/building_destruction_v6.cjs
node tests/destruction_effects.cjs
node tests/destruction_lifecycle.cjs
node tests/building_v6.cjs
```

- **Core — 33 sprawdzenia:** bilans energii, wielowarstwowe AP, cienka blacha,
  wgniecenia i kratery, rykoszet, osłonięty wybuch, ograniczone podparcie,
  ruchomy gruz, cięcia i budżety; odłamki po rykoszecie zachowują swój typ.
- **Rubble field:** zachowanie objętości, packing/mix materiałów, slope relaxation, tile overflow bez utraty materiału, ballistic cover, AP/HE redistribution oraz nav/cover hooks.
- **Adaptery / Rapier:** rzeczywisty Three/Rapier, zasoby kopii,
  podparcie z ograniczoną rozpiętością, geometria dachów, detale elewacji,
  uśpienie i przebudzenie gruzu, przepełnienie puli, rośliny i ragdoll.
- **Pełne budynki — 6 sprawdzeń:** cztery wzorce, strzał AP w kompletny model, otwór sprawdzany
  promieniem w materiale, siatce i Rapierze; 13 celowanych AP usuwa wybraną
  sekcję domu testowego, a 105 HE usuwa trafioną ścianę. Bez ręcznego wywoływania
  obrażeń w tym teście.
- **Efekty — 6 sprawdzeń:** skład i czas życia wybuchu, reakcje materiałowe,
  deterministyczny rozrzut, stałe limity, usuwanie śladów oraz zwalnianie zasobów.
- **Cykl życia:** 12 wejść/resetów/wyjść, anulowanie przygotowania, błąd
  ładowania, fokus, Esc/blur i rytm ognia. Reset blokuje wznowienie strzelania,
  a wizualny odrzut nie zmienia fizycznego wylotu. Zegar efektów działa podczas
  przebudowy celu. Renderer jest tutaj atrapą.
- **Obciążenie:** [benchmark.json](benchmark.json) obejmuje pełną drogę CPU
  budynku: balistykę, przebudowy siatek/colliderów, Rapiera i ruchome objętości.
  Liczy faktyczne trafienia i wybuchy, nie tylko wystrzelone pociski.
- **Koszt efektów:** `node tools/destruction/effects-benchmark.cjs` zapisuje
  [osobny raport](effects-benchmark.json): 300 wybuchów i 600 trafień w stal
  przez 30 s symulacji, dla obu poziomów jakości. Obejmuje emisję i aktualizację
  efektów oraz usuwanie śladów, bez GPU, fizyki i dźwięku.

Plik [benchmark.json](benchmark.json) pochodzi sprzed RubbleField v1 i nie jest bieżącym pomiarem tej architektury. Zaktualizowany harness mierzy hero bodies, cheap debris, rubble tiles/volume, demotions i bakes; przed porównaniem wydajności należy ponownie wygenerować canonical benchmark. Historyczne średnie/p95/max pozostają tylko punktem odniesienia. Raport oddziela
detonacje w budynku od dodatkowych wybuchów gruntu i podaje ewentualne resety
celu. Limit aktywnego gruzu należy sprawdzać przez liczbę brył nieśpiących;
cała pula dynamiczna obejmuje również bryły uśpione. Nie należy przenosić
wyników wcześniejszego wariantu geometrii i fizyki na aktualną implementację.
Średnia i p95 bez maksimum nie wystarczają do uznania płynności za odebraną.

Wykonano też automatyczną sesję renderowaną w Chrome przy 1920×1080 przez
`file://`: AP i HE wystrzelono z interfejsu, sprawdzono reset i powrót do edytora.
Trzy dodatkowe cykle wejście/reset/wyjście wróciły do 256 geometrii i jednej
tekstury edytora; jego źródłowy budynek pozostał nienaruszony.
[Raport przeglądarki](artifacts/browser-check.json), [otwór AP z bliska](artifacts/building-ap-detail.png),
[wyrwa po HE](artifacts/building-he.png), [błysk HE](artifacts/building-he-flash.png),
[pył HE](artifacts/building-he-dust.png), [wgniecenie metalu](artifacts/building-steel-dent.png).
Nowe shadery przeszły kompilację, a sprawdzony wybuch korzystał z trzech
aktywnych pul renderowania. To nie jest pomiar 60 FPS na docelowej
karcie graficznej. Skrypt `tools/destruction/browser-check.cjs` wymaga Chrome
oraz Playwright; opcjonalna zmienna `DESTRUCTION_PLAYWRIGHT` wskazuje moduł.
Test podaje lokalne kopie przypiętych skryptów Three zamiast CDN.

Pozostaje ręczny odbiór sterowania i płynności, skrajnych wariantów budynków
oraz pozostałych kategorii celów. Skały korzystają z wypukłych otoczek,
gałęzie i strefy ciała z objętości zastępczych, a sweep ruchu uwzględnia
translację bez pełnego ciągłego obrotu brył. Zakres obejmuje pojedynczy
skopiowany cel; destrukcja podczas walki na całej mapie jest osobnym etapem.
