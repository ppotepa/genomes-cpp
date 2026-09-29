# Profile i źródła

Sprawdzenie źródeł: 2026-09-27. Nazwy i rodziny kalibrów pochodzą z materiałów
producentów. Liczby w katalogu oznaczają **parametry modelu gry**; nie są
kompletnymi ani certyfikowanymi tabelami strzelniczymi konkretnych nabojów.

| Profil | Masa kg | Prędkość m/s | Średnica penetratora m | Ładunek HE kg |
|---|---:|---:|---:|---:|
| 556-ball | 0.004 | 915 | 0.00556 | 0 |
| 762-ball | 0.0095 | 840 | 0.00762 | 0 |
| 127-ball | 0.046 | 890 | 0.0127 | 0 |
| 20-ap / he | 0.10 / 0.12 | 1100 / 1050 | 0.014 / 0.020 | 0 / 0.012 |
| 30-ap / he | 0.23 / 0.36 | 1385 / 1080 | 0.015 / 0.030 | 0 / 0.040 |
| 40-ap / he | 0.50 / 0.96 | 1250 / 1000 | 0.022 / 0.040 | 0 / 0.120 |
| 105-ap / he | 3.5 / 15 | 1450 / 730 | 0.028 / 0.105 | 0 / 2.1 |

Masy, prędkości, średnice, ładunki, pojemności testowe, czasy przeładowania,
odrzut i dźwięk są dobranymi presetami gry, nie wartościami przypisywanymi
poniższym publikacjom. AP reprezentuje wyidealizowany penetrator po odrzuceniu
sabotu; nie symuluje fazy rozdzielenia. Prędkość jest zapisana przy zgodnej
broni, a średnica penetratora jest niezależna od kalibru lufy.

- [FN Herstal — machine guns](https://fnherstal.com/en/defence/portable-weapons/machine-guns/): rodziny MINIMI, MAG i M2HB.
- [Rheinmetall — Mittelkaliberwaffen](https://www.rheinmetall.com/de/produkte/waffen-und-munition/mittelkaliberwaffen): MK20 Rh202, 20×139 mm i podana kadencja 1000/min.
- [Northrop Grumman — Mk44](https://www.northropgrumman.com/-/media/wp-content/uploads/Mk44-30mm-Bushmaster-Chain-Gun.pdf): odnośnik z planu; PDF był niedostępny podczas weryfikacji. Liczb tego presetu nie oznaczono jako zweryfikowanych.
- [BAE Systems — Bofors 40 Mk4](https://www.baesystems.com/dam/jcr:52e9d5c7-90c5-41ea-997c-5911b068dcaf): dokument podaje kaliber 40 mm L/70 i 300/min dla opisanego systemu. Preset FPS nie odtwarza całej instalacji Mk4.
- [Elbit — 105 mm series](https://www.elbitsystems.com/tank-ammunition-105mm-series): rodzina amunicji 105 mm. Profile gry nie identyfikują konkretnego wyrobu tej serii.
- [NASA — Drag Equation](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-equation/): postać `D = Cd ρ v² A / 2`. Przyjęto pole czołowe penetratora.
- [Rapier compat](https://rapier.rs/docs/user_guides/javascript/getting_started_js/): lokalny wariant z zakodowanym WASM, inicjalizowany przed przygotowaniem fizyki.

### Odłamki HE i rykoszety: zakres walidacji

| Przypadek | Warunki opublikowane | Punkt odniesienia | Zastosowanie w modelu |
|---|---|---|---|
| SS109 / Hardox 500 | 4 g, prędkości wejściowe 908/901 m/s; płyta nachylona 5°/25°; prędkości po odbiciu 894/809 m/s | Stosunek prędkości 0,985/0,898; energii 0,969/0,806. Tekst opisuje kąty jako nachylenie płyty; pocisk wchodzi poziomo. Prędkość mierzono na płycie świadka średnio 1,8 m dalej. | Wąska kalibracja profilu gry 5,56 mm na stali: natychmiast po kontakcie model daje około 0,987/0,902 prędkości i 0,974/0,814 energii. Nie odwzorowuje odcinka 1,8 m ani Hardox 500. Nie przenosić na inne kalibry lub materiały. [Publikacja, Defence Technology](https://www.sciencedirect.com/science/article/pii/S2214914719312565) |
| HE, cylindryczny ładunek z korpusem | Prędkość zależy od energii właściwej materiału wybuchowego i stosunku masy ładunku do korpusu | Gurney i modele pochodne opisują zależność; nie podają parametrów korpusu i ładunku naszych presetów. | Uzasadnia jawne parametry profilu, ale nie potwierdza dokładnej prędkości żadnego profilu gry. [Model fragmentacji](https://www.sciencedirect.com/science/article/pii/S0734743X17307339) |
| Cienkościenny stalowy korpus, ładunek implozyjny | W opublikowanym eksperymencie maksimum wyniosło 2,22 km/s przy stosunku masy ładunku do korpusu 2,21 | Przykład pokazuje szerokość możliwych wyników; to inna konfiguracja niż pocisk artyleryjski w grze. | Kontekst zakresu, nie bezpośredni cel kalibracji. [Raport eksperymentalny](https://pmc.ncbi.nlm.nih.gov/articles/PMC10456433/) |
| HE 20 / 30 / 40 / 105 mm | Nie znaleziono zgodnego źródła obejmującego jednocześnie masę korpusu, masę i typ ładunku oraz pomiar fragmentów | Brak porównywalnej wartości prędkości i energii. | Parametry profilu pozostają jawnymi przybliżeniami gry. |
| Rykoszet od betonu, cegły i drewna | Brak zgodnego pomiaru dla tych profili amunicji i jednoznacznej definicji kąta | Brak porównywalnej wartości prędkości i energii. | Współczynniki materiałów pozostają parametrami rozgrywki; nie przenosimy wyniku stali. |

Symulator zapisuje `incidence = |direction · normal|`, czyli cosinus kąta względem normalnej; panel pokazuje `acos(incidence)` w stopniach (0° to trafienie prostopadłe, 90° styczne do powierzchni). `outgoingDirection` pozwala odtworzyć kąt odejścia. W publikacji SS109 wartość opisuje tilt płyty; dla poziomego strzału jest to kąt między torem a powierzchnią.

Profile HE deklarują osobno udział energii odłamków, udział masy korpusu, liczbę odłamków i rozrzut mas. Są to gałki modelu gry. Energia przypisana radialnemu ruchowi wynika z jawnego budżetu; ograniczenie puli pomija całe przeciwległe pary i nie zwiększa prędkości zachowanych odłamków.

`Cd=0.28`, transoniczny mnożnik Gaussa, wytrzymałość materiałów, praca
niszczenia, progi rykoszetu, osłabienie za osłoną, energia właściwa HE 4 MJ/kg
oraz współczynniki odporności gatunków skał są parametrami gry.
Profile HE emitują odpowiednio 24/40/64/128 odłamków dla 20/30/40/105 mm.
Jawny profil amunicji przydziela fragmentom 30% masy korpusu i 25% energii
wybuchu do radialnego ruchu. Seedowany rozrzut mas wynosi ±20%; prędkość
odłamka wynika z jego masy oraz energii podzielonej między pełny wzór fragmentów.
To parametry gry, a ich punktowe wartości nie są potwierdzone zgodnym pomiarem.
Gęstość powietrza wynosi domyślnie 1.225 kg/m³, prędkość dźwięku 343 m/s,
grawitacja 9.80665 m/s². Model nie uwzględnia temperatury, Coriolisa,
pełnej precesji, zapalników programowalnych ani chemii spalania. Stan pocisku
uwzględnia natomiast uproszczoną integralność, deformację, stabilność, zmianę
efektywnej średnicy/oporu, deflekcję po kontakcie i ograniczony rozpad na wtórne fragmenty.

## Praca penetracji i niszczenie sekcji

Opór materiału ma postać `F = penetrationWork × bulkResistance(material) × interactionScale(ammo, material) × strengthScale × damageFactor × localWeakness × A`, gdzie `A = π diameter² / 4`, `damageFactor = 1 − 0.65 × min(0.95, damage)`, a lokalne osłabienie pochodzi z nakładających się stref pęknięć / uszkodzonych komórek muru. Nie ma już jednego globalnego mnożnika AP. `interactionScale` jest jawną macierzą modelu gry dla pary klasa amunicji–materiał (`materialImpact.js`). `bulkResistance` służy do strojenia objętościowego oporu kruchego celu bez udawania jednej uniwersalnej przewagi AP. Bieżące wartości są parametrami rozgrywki, nie odwzorowaniem konkretnego naboju lub klasy ochrony. Utrata energii to praca `F × droga`, ograniczona dostępną energią kinetyczną.
Kąt trafienia wpływa na drogę w geometrii, bez zastępowania jej stałą grubością.
Pochłonięta praca jest następnie rozdzielana na budżet lokalny: kruszenie/plastykę, front crater, pęknięcia promieniowe, rear damage i ejecta. Suma tych kanałów nie przekracza energii utraconej przez pocisk; tylko część strukturalna zwiększa uszkodzenie sekcji i połączeń. Ten model zastępczy nie jest rozwiązaniem deformacji penetratora ani tablicą V50.

| Materiał | `strength` Pa | Praca penetracji J/m³ | Próg zniszczenia J/m³ |
|---|---:|---:|---:|
| Cegła | 16 000 000 | 160 000 000 | 40 000 |
| Beton | 45 000 000 | 600 000 000 | 80 000 |
| Stal | 600 000 000 | 10 000 000 000 | 250 000 |
| Drewno | 8 000 000 | 40 000 000 | 12 000 |
| Skała | 100 000 000 | 800 000 000 | 180 000 |

Pole `strength` jest teraz używane przy lokalnej wnęce metalu i drewna.
Dla zatrzymanego pocisku model przyjmuje pracę normalną
`Wn = Eabsorbed × min(1, dot(direction, normal)²)` i próg wnęki
`strength × π × diameter³ / 4`. Poniżej progu powstaje efekt trafienia;
energia nadal zwiększa uszkodzenie sekcji. Praca penetracji pozostaje osobnym
parametrem oporu wzdłuż toru. Uszkodzenie całej sekcji narasta od
zdeponowanej energii podzielonej przez jej początkową objętość, `toughness`
i `strengthScale`.

| Wynik lokalny | Uproszczenie modelu gry |
|---|---|
| `mark` | Ślad, pył lub iskry bez nowego ubytku geometrycznego. |
| `dent` / `splinter` | Zwężająca się wnęka w metalu lub drewnie; rozmiar wynika z pracy normalnej, `strength`, średnicy i lokalnej grubości. |
| `crater` | Kruchy materiał korzysta z pracy zdeponowanej i `toughness`; wnęka nie przechodzi samoczynnie przez całą grubość ściany. |
| `perforation` | Pocisk rzeczywiście przeszedł przez materiał; kanał usuwa geometrię i kolizję. |
| `fracture` / `crushed` | Sekcja rozpada się, odłącza albo końcowy gruz zostaje usunięty. |

Wnęka budynku korzysta z 8–12 płaszczyzn zależnie od materiału, z deterministyczną nieregularnością i wydłużeniem. Kanał pełnej perforacji pozostaje twardo ograniczony do małej liczby wypukłych survivorów; mały sąsiadujący survivor może zostać odłączony jako fizyczny chip, ale musi leżeć przy wlocie/wylocie i mieć mniej niż 10% objętości pierwotnej sekcji. Pełna perforacja ma osobny wąski channel oraz lokalną erozję krawędzi wejścia/wyjścia; w budynkach te chipy nie zwiększają liczby fizycznych convex parts, tylko zmniejszają objętość istniejącego survivora. Cegła ma dodatkowo ograniczoną siatkę wirtualnych komórek/spoin o zasięgu wynikającym ze `stressRadius`, beton i materiały kruche strefy lokalnego stress damage, stal tryby dent/plug/petal, a drewno kierunek włókien wpływający zarówno na kształt ubytku, jak i niewielką deflekcję po penetracji/rykoszecie. Drobne dekoracyjne odpryski pozostają VFX, a wtórne fragmenty samego pocisku mają osobny ograniczony budżet balistyczny. Głębokość lokalnej
wnęki jest ograniczona grubością sekcji: do 85% dla metalu/drewna i do 95%
dla krateru kruchego materiału. Pełna perforacja korzysta z osobnej ścieżki.
Odłamki HE, również po rykoszecie, kumulują obrażenia bez osobnego cięcia
powierzchni dla każdego drobnego kontaktu.

Cięcie ma do dwóch poziomów, pełny podział do trzech fragmentów, a gruz
budynku nie dzieli się dalej. Po wyczerpaniu budżetu płytkie wgniecenia
metalu i drewna mogą pozostać wyłącznie efektem, nadal kumulując obrażenia.
Głębszy wyłom lub przebicie uruchamia odłączenie sekcji. Wszystkie wymienione
progi i kształty są parametrami gry; źródła nie walidują ich liczbowo.
Snapshot strojenia znajduje się w [material-response-calibration.json](material-response-calibration.json). Pokazuje zachowanie aktualnych presetów gry i nie jest tabelą penetracji rzeczywistej amunicji.

[Research budynków](building-damage-research.md) oraz
[analiza ubytków i stabilności](impact-effects-and-stability.md) opisują ograniczenia.

## Podparcie i gruz

Połączenia boczne zużywają dystans równy poziomej odległości między środkami
sekcji. Nominalne limity wynoszą 4 m dla dachu, 3 m dla stropu i 1,2 m dla
ściany lub pozostałego elementu. Dla nienaruszonego wygenerowanego projektu
limit może zostać podniesiony do jego początkowej najkrótszej drogi podparcia
plus 0,25 m. Pionowe połączenie nośne zachowuje już zużyty dystans (`carry`),
co zapobiega jego zerowaniu przez przejście ściana–strop–ściana.
Każde połączenie `child → support` ma dodatkowo integralność 0–1. Trafienie blisko
styku lub impuls wybuchu może ją obniżyć; po zejściu poniżej progu połączenie przestaje
uczestniczyć w grafie podparcia. Stan jest dziedziczony przy dzieleniu sekcji.
To nadal ograniczenie topologii modelu gry, bez obliczania naprężeń lub obciążenia.

## Persistent rubble field

Po osiadaniu gruz może zmienić reprezentację z osobnego rigid body na pole objętościowe. Domyślna komórka ma 0,5 × 0,5 m, tile 4 × 4 m, a limit pola wynosi 256 tiles. Są to parametry wydajnościowe gry.

Pole przechowuje **solid volume** osobno od wysokości sterty. Wysokość wynika z udziału materiałów i parametrycznego packing factor; cegła, beton, drewno, stal i skała mogą współistnieć w jednej komórce. Packing, maksymalne nachylenie sterty, tempo slope-relaxation oraz porowaty mnożnik oporu balistycznego nie pochodzą z tabel inżynierskich i służą wyłącznie stabilnej, czytelnej symulacji gry.

Balistyka rubble nie sumuje setek pojedynczych colliderów. Tile ma syntetyczny profil materiałowy obliczany z mieszaniny i packing, a jego efektywny opór jest niższy niż opór litej ściany z tego samego materiału. Trafienie może przenieść część solid volume między komórkami lub czasowo wyrzucić ją do cheap-airborne pool. HE może analogicznie wzbić ograniczoną część sterty. Docelowym invariantem scenariuszy bez erozji jest zachowanie:
`hero volume + cheap airborne volume + baked rubble volume ≈ detached source volume`.

Sleeping rigid body nie jest bake'owany natychmiast. Elevated debris może pozostać uśpione na stropie i później obudzić się po utracie podpory; ground/pile debris przechodzi do bake dopiero po grace period. Szczegółowa architektura i ograniczenia: [rubble-field.md](rubble-field.md).

Rapier może zachować do 512 brył gruzu, z limitem 128 brył dynamicznych,
które nie śpią. Uśpienie pozostawia bryłę dynamiczną, aby mogła ponownie
zareagować na utratę podparcia. Po kroku fizyki nadmiar obudzonych brył jest
usuwany ze wszystkich reprezentacji; nowy gruz bez miejsca w aktywnej puli
również znika. Ograniczona pula nie zachowuje całego materiału budynku.
[Rapier — Sleeping](https://rapier.rs/docs/user_guides/javascript/rigid_body_sleeping/)

## Efekty wizualne

Efekt korzysta z wyniku symulacji: `lost` opisuje pochłoniętą energię,
`energy` energię resztkową, a `response` i `geometryChanged` skutek trafienia.
Materiał, normalna, kierunek, średnica, rodzaj amunicji i seed sterują pyłem,
iskrami i odpryskami. `radius` i `scarDepth`, jeśli występują, opisują wnękę;
nie zastępują `depth`, które nadal oznacza drogę pocisku w materiale.

Pule mają stałe limity: 16 błysków, 128 chmur, 256 odprysków i 128 śladów;
w niskiej jakości odpowiednio 4/40/80/32. Dzielą jedną proceduralną teksturę
64×64 i najwyżej cztery wywołania renderowania. Nie dodają świateł, cieni ani
brył Rapiera. Niezależny zegar klatek pozwala wygaszać efekt podczas
oczekiwania na przebudowę geometrii. Czasy, kolory i rozrzut efektów są
ustawieniami wizualnymi, bez wpływu na energię i wynik trafienia.

Współdzielenie pul i ograniczanie liczby emiterów odpowiada zaleceniom Epic;
nie oznacza użycia Niagara w tej aplikacji.
[Epic — Scalability and Best Practices](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara)
Przezroczyste chmury mogą wielokrotnie pokrywać te same piksele, więc koszt
GPU zależy również od widoku i powierzchni zajmowanej na ekranie.
[Epic — Measuring Performance](https://dev.epicgames.com/documentation/en-us/unreal-engine/measuring-performance-in-niagara)

Rapier: Apache-2.0; pomocniczy ConvexHull z Three r128: MIT. Licencje
i sumy SHA-256 znajdują się w `js/vendor/`. Odtwarzanie pakietów:
`npm ci --ignore-scripts && npm run bundle` w `tools/destruction`.
Lockfile przypina również zależności narzędzi. Budowanie ConvexHull używa
istniejącego globalnego Three.js, bez dołączania drugiej kopii renderera.
# Caliber strategy calibration

The caliber modules use public terminal-ballistics research as gameplay
checkpoints, not certified penetration tables. The 5.56 mm FMJ checkpoint is a
4.16 g round at approximately 891 m/s in 18 MPa concrete: 25/50 mm can
perforate, 75 mm stops with crater/spall, and repeated focused hits on 100 mm
may perforate only after local weakening. Construction, yaw, angle, reinforcement
and local prior damage are therefore part of every result.
