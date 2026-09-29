# Energia pocisków i ślady — odbiór użytkownika

Status: kod wdrożony, scenariusze poniżej **nieodebrane**. Agent wykonał
wyłącznie przegląd kodu, kontrolę składni JS i kontrolę diff. Nie uruchamiał
testów automatycznych ani browser-check. Wyniki fizyczne i obraz wymagają
ręcznego odbioru.

## Odczyty

W scenie zniszczeń otwórz debugger F2. Wybierz pocisk lub jego odłamek z listy.
Panel podaje rzeczywiste J/kJ, m/s, czas i drogę, energię obrotową, bilans,
wejście/wyjście kontaktów, materiał, komponent, warstwy, kąt i quaternion.
Po rykoszecie pokazuje również czas i drogę od ostatniego odbicia.
`ground-contact` oznacza fizyczny kontakt z płaszczyzną ziemi y=0 i jest
widoczny w historii śladu. Płaszczyzna nie ma granicy zasięgu. W świecie z
ziemią limit obserwacji wynosi 120 s; bez ziemi `tracking-limit` kończy lot po
15 s. Panel historii pokazuje powód zakończenia.

Zielony/pomarańczowy/czerwony oznaczają wyłącznie energię. Szerokość wynosi
1,5–6 pikseli CSS. Domyślnie 100% oznacza energię początkową wybranego pocisku,
także odłamka. Skala absolutna używa `ln(1 + E[J]) / ln(1 + 5 000 000)`;
połowa skali to około 2,24 kJ. Wartości ponad zakresem są sygnalizowane.
Symbole kontaktów: pierścień — przebicie, stożek — odbicie/otarcie,
sześcian — zatrzymanie, ośmiościan — detonacja, płaski pierścień — ziemia.

`materialWork` to praca oporu, `contactLoss` to pozostałe straty kontaktu,
`targetWork` — wkład ruchu celu, `flightWork` — łączna praca grawitacji i oporu,
`rotationWork` — zmiana energii od modelu stabilizacji aerodynamicznej,
`fragmentEnergy` — energia reprezentowanych odłamków, `unrepresentedEnergy`
— energia nieśledzonej części korpusu/odłamków i niezdeponowanej eksplozji.
`explosiveEnergy` i `blastEnergy` rozdzielają źródło wybuchu od jego oddziaływania.
Błąd bilansu jest niezależnie wyliczoną resztą; nie jest zerowany przez korektę.
Bilans dotyczy pocisku i przekazanych budżetów. Nie jest globalnym pomiarem
energii wszystkich ciał Rapiera i usuwanego materiału.

## Scenariusze — każdy oczekuje ręcznego odbioru

| Scenariusz | Czynność i oczekiwany odczyt |
| --- | --- |
| Lot poziomy | Bez wiatru porównaj energię i prędkość z `½mv²`; opór zmniejsza energię. |
| Brak oporu | W niezależnym `RTS.BallisticsWorld` ustaw `environment.density: 0`, `gravity: [0,0,0]`; prędkość i energia pozostają stałe. |
| Opadanie | Przy `density: 0` i grawitacji porównaj wzrost energii z `mgΔh`; spadek energii nie jest wymuszany. |
| Cienka/gruba ściana | Ten sam preset, seed i kąt; porównaj rzeczywistą drogę materiałową, pracę i energię końcową. |
| Kolejne przeszkody | Pocisk z zachowaną energią powinien mieć kilka kontaktów na jednym śladzie. Powtórz z przeciwnej strony. |
| Warstwy | Porównaj obie strony ściany: odwraca się kolejność przedziałów, nie ich położenie. Przerwy mają zerową pracę i niezerowy czas. |
| Dwie szyby | Dwa kontakty po 9 mm szkła, z przelotem przez 8 mm powietrza; żadna szyba nie nalicza całego pakietu. |
| Techniczny podział płyty | Sąsiadujące sekcje jednego `physicalSolidId` mają jeden kontakt fizyczny; rozdzielenie szczeliną tworzy dwa. |
| Rykoszet/otarcie | Ostrzelaj ścianę pod małym kątem; porównaj energie przed/po, ciągłość lotu, czas i drogę po odbiciu. |
| Rykoszet SS109 / stal | Użyj 5,56 mm, stalowej płyty oraz nachylenia 5° i 25°; porównaj prędkość, energię i kąt odejścia z tabelą kalibracji. Uwzględnij, że pomiar źródłowy wykonano 1,8 m za płytą. |
| Pozostałe materiały | Powtórz sweep na betonie, cegle i drewnie; potraktuj wynik jako zachowanie gry, bo tabela nie ma dla nich zgodnych danych. |
| HE: szkło/cegła/beton | Detonacja na rzeczywistej powierzchni, osobny kontakt kinetyczny i energia wybuchu. Sprawdź wyłom kolejnym strzałem oraz widoczność i trafialność gruzu. |
| Profile odłamków HE | Dla 20/30/40/105 mm wybierz ślady odłamków; porównaj rozrzut, masę i energię z profilem oraz sumarycznym budżetem w [tabeli kalibracji](ballistics-calibration.md). |
| Kilka HE | Domyślnie jedna grupa HE z odłamkami. Przełącz 1/3/10, ukryj odłamki, wybierz dziecko i „Ostatni”; nie powinny wystąpić podwójne linie. |
| Historia | Po ponad 10 strzałach znika najstarszy główny strzał z całym drzewem. Późne odłamki nie odnawiają jego pozycji. |
| Filtry po locie | Na pauzie zmieniaj skalę, historię i fragmenty; renderer ma odświeżać się natychmiast. Zmień rozmiar okna i FOV: szerokość pozostaje ekranowa. |
| Niezależność renderowania | Powtórz identyczne strzały przy OFF/ON oraz ograniczonym FPS; porównaj tick, kontakty i energie. |
| Ziemia i timeout | Strzał o dużym kącie powinien zakończyć się płaskim markerem `ground-contact`, także poza dawnym zasięgiem ziemi; bez skonfigurowanej ziemi panel powinien pokazać `tracking-limit`. |
| Reset/zamknięcie | Usuń scenę podczas lotu i po wybuchu; brak śladów, markerów i osieroconych części po ponownym otwarciu. |

Do kontroli izolowanych przypadków można utworzyć `RTS.MaterialModel` z
prostopadłościanami (`id`, `min`, `max`, `material`, `layers`, `materialFrame`),
podłączyć `RTS.ProjectileTraceRecorder(10)` do `RTS.BallisticsWorld`, wywołać
`fire({id, weapon, ammo, position, direction, tick: 0, seed})` i ręcznie wykonywać
`fixedStep()`. Identyfikatory zgodnych presetów są w `RTS.DestructionWeapons`
i `RTS.DestructionAmmo`. Ramę materiałową i warstwy opisują [kontrakty](contracts.md).

Zapalnik HE w demonstracji jest jawnie uzbrojony i kontaktowy. Nie symuluje
mechanizmu uzbrajania, zwłoki, programowania ani własności konkretnego produktu.
Kontakt kinetyczny używa ograniczonej pracy zgniatania nosa na drodze ¼ średnicy.
Współczynniki penetracji, deformacji, stabilizacji i wybuchu pozostają modelem gry.
