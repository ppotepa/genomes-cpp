# Generator otoczenia — rośliny, v0.17

Uruchom `environment.html` albo wybierz **Generatory → Otoczenie → Rośliny** w prawym panelu mapy. To pierwszy działający moduł generatora otoczenia, obok istniejącego generatora piechoty. Kategorie „Kamienie i skały” oraz „Znaki i obiekty” są widoczne jako planowane; na tym etapie generowane są rośliny. Ustawienia roślin znajdują się po prawej, w zwijanych sekcjach: rodzaj otoczenia, roślina, sezon i rozwój, genom, wyświetlanie oraz import i eksport. Wspólne style kategorii i przycisków są w `css/labUi.css`.

## Co działa

- 20 gatunków, 7 edytowalnych genów, deterministyczny seed.
- Regułowe generowanie pnia, gałęzi i pędów; wielopniowość krzewów; korony szerokie, kolumnowe, zwisające i iglaste.
- Cztery stany sezonowe, dojrzałość 5–100%, dwa poziomy szczegółowości.
- Kwiaty i owoce wybranych gatunków; młode osobniki nie owocują. Iglaki mają szyszki, leszczyna orzechy, dąb żołędzie. Są to uproszczone modele proceduralne.
- Liście proste, klapowane, złożone i pęczki igieł. Na dużych drzewach geometria liści reprezentuje większe skupiska, aby utrzymać czytelność koron.
- Obrót, zoom, siatka metrowa, dopasowanie kamery, eksport/import DNA razem ze stanem podglądu.
- Łączona geometria: do 4 siatek na osobnik, bez osobnych draw calli dla każdego liścia.

Podgląd interfejsu: [environment.png](research/images/environment.png). Porównanie wszystkich gatunków: [plants.png](research/images/plants.png). W atlasie sylwetki dopasowano osobno do kadrów; podpisy pokazują wysokość modelu, więc wielkość na obrazku nie jest wspólną skalą.

## Katalog 20 roślin

Parametry są dobrane artystycznie dla gry, a nie przeznaczone do prognozowania rzeczywistego wzrostu. Nazwy gatunków opisują inspirację i kierunek dalszego dopracowania.

| # | Roślina | Cechy generatora | Akcent sezonowy |
|---|---|---|---|
| 1 | Dąb | Szeroka korona, ciemna kora, klapowane liście | Ochra, żołędzie |
| 2 | Buk | Wyższa, bardziej zwarta korona, jasnoszary pień | Miedziane liście, uproszczone orzeszki |
| 3 | Brzoza | Smukłość, jasny pień z ciemnymi pasami | Żółte liście |
| 4 | Klon | Rozłożystość, gwiaździsta sylwetka blaszki | Pomarańczowe liście |
| 5 | Lipa | Szeroki owal korony | Złote liście |
| 6 | Jesion | Wyższa i prześwitująca korona, liście złożone | Przygaszona żółć |
| 7 | Wierzba płacząca | Rozłożyste konary, wydłużone zwisające pędy, wąskie liście | Oliwkowa żółć |
| 8 | Topola kolumnowa | Wąska, wysoka sylwetka i strome gałęzie | Żółte liście |
| 9 | Jarząb | Mniejsze drzewo, liście złożone | Pomarańczowe grona |
| 10 | Jabłoń | Niska, szeroka korona | Jasne kwiaty, czerwone jabłka |
| 11 | Grusza | Smuklejsza korona, wzniesione gałęzie | Kwiaty, wydłużone gruszki |
| 12 | Czereśnia | Wyższe drzewo owocowe, brunatnoczerwona kora | Kwiaty, letnie owoce |
| 13 | Sosna | Wysoki odsłonięty pień, korona w górnej części | Igły zimą, szyszki |
| 14 | Świerk | Niskie piętra gałęzi, stożkowy pokrój | Ciemnozielone igły zimą |
| 15 | Jodła | Węższy stożek, nieco wzniesione gałęzie | Chłodniejsza zieleń zimą |
| 16 | Modrzew | Lżejsze piętra, jaśniejsze pęczki | Złote igły jesienią, nagie gałęzie zimą |
| 17 | Leszczyna | Kilka pni, zwarta korona | Orzechy |
| 18 | Głóg | Gęsty, niski krzew | Kwiaty, czerwone owoce |
| 19 | Dzika róża | Niski, szeroki krzew, liście złożone | Różowe kwiaty, wydłużone owoce |
| 20 | Bez czarny | Wielopniowy krzew, liście złożone | Jasne kwiaty, ciemne grona |

## DNA i stan rośliny

`PlantGenome` zawiera `version`, `category`, `species`, `seed`, `genes`. Geny sterują wysokością, szerokością korony, rozgałęzieniami, krzywizną, gęstością i wielkością liści oraz owocowaniem. Zakresy 0–1 są przeliczane przez profil gatunku; maksimum nie oznacza dowolnie wielkiego drzewa.

Wiek, sezon i detal są osobnym stanem. Zmiana jesieni na zimę zachowuje położenie węzłów i gałęzi. Osobne strumienie losowe dla drewna, liści i owoców uniemożliwiają przypadkowe przelosowanie drzewa przy zmianie liczby liści. Identyfikatory segmentów i punktów ulistnienia są stabilne dla tego samego genomu i wieku.

Wiek jest znormalizowaną dojrzałością, nie liczbą lat. Obecnie wpływa na rozmiar i możliwość owocowania; nie symuluje kolejnych lat powstawania i zamierania gałęzi. Zmiana genów architektury może zmienić topologię. Nie jest to genetyka mendlowska ani symulacja botaniki.

## Dlaczego taki generator

Pierwszy etap używa ograniczonej rekurencji i profili architektury. Ma przewidywalny koszt, szybko reaguje na suwaki i nie potrzebuje zasobów graficznych. Gałęzie powstają jako zwężające się segmenty, a powierzchnie są łączone według materiału.

Docelowo najlepszy kierunek to **hybryda reguł gatunkowych i kolonizacji przestrzeni**: reguły wyznaczają pień, dominację przewodnika i główne konary, a przyrost pędów kieruje się ku wolnym obszarom korony. Metoda Runionsa, Lane'a i Prusinkiewicza rozszerza model żyłkowania liści do struktur drzew w 3D. To uzasadnia użycie punktów przyciągania do lepszego wypełnienia koron i reakcji na sąsiadów: [Modeling Trees with a Space Colonization Algorithm, 2007](https://algorithmicbotany.org/papers/colonization.egwnp2007.html).

**Kolonizacja przestrzeni nie jest jeszcze zaimplementowana.** Obecna rekurencja pozostaje świadomie prostszym pierwszym krokiem. Należy ją rozwijać w stronę mniej regularnych rozwidleń, lepszej ciągłości konarów, pędów wewnętrznych i charakterystycznych pokrojów starych drzew. Szczególnie warto dalej różnicować buk/lipę/klon oraz świerk/jodłę; sama kolorystyka nie wystarczy do mocnej identyfikacji gatunku.

Rozdzielenie architektury i poziomów szczegółowości ma precedens w parametrycznym modelowaniu drzew: [Weber i Penn, Creation and Rendering of Realistic Trees, 1995](https://doi.org/10.1145/218380.218427). Modrzew wymaga osobnego zachowania sezonowego: jest iglakiem zrzucającym igły, co opisuje [US Forest Service — Larix decidua](https://research.fs.usda.gov/feis/species-reviews/lardec).

## Następne rozszerzenia — projekt

1. **Fenologia ciągła:** dzień roku, lokalna temperatura i profil gatunku. Osobne fazy pąków, kwiatów, niedojrzałych owoców, dojrzałości, opadania i pozostałości zimowych. Obecnie są cztery dyskretne stany; nie ma kalendarza klimatycznego ani płynnych przejść.
2. **Starzenie:** siewka, młode drzewo, dojrzałe, stare, martwe. Poszerzanie rozwidleń, ubytki korony, odrosty, suche gałęzie. Zmiany środowiskowe nie powinny nadpisywać dziedzicznego genomu.
3. **Siedlisko:** wilgotność, nachylenie i ekspozycja gruntu, gleba, zacienienie, sąsiedztwo. Zależne od siedliska mieszanki zamiast równomiernego losowania wszystkich gatunków.
4. **Rozmieszczanie:** kępy krzewów, skraj lasu, zagajniki brzozowe, sady w nieregularnych rzędach, pojedyncze stare drzewa i polany. Ochrona tras testowych piechoty, odstępy od dróg i zabudowy, pionowy wzrost na stoku.
5. **Wydajność świata:** osobny generator w chunkach, ograniczona biblioteka wariantów genomu instancjonowana na mapie, uproszczone gałęzie dla dystansu, impostory koron daleko, histereza przełączania. Aktualny tryb „Dystans” redukuje liście i pomija owoce, lecz zachowuje cały szkielet — nie jest gotowym LOD lasu 4 km².
6. **Wiatr i pogoda:** podatność pędów zależna od grubości, stabilna faza wiatru z seeda; śnieg jako pogoda, a nie automatyczna cecha zimy. Opadłe liście i owoce jako rzadsze dekoracje pod rośliną.
7. **Interakcje:** uproszczony collider pnia, korona jako osłona wizualna, osobna przepuszczalność krzewu. Dopiero później łamanie gałęzi, ścinanie i zbieranie owoców.
8. **Kolejne kategorie:** wspólny kontrakt genom → obiekt → statystyki → zwolnienie zasobów; skały z geometrią warstw i pęknięć, kamienie z erozją, znaki z regułowym słupkiem i tablicą. Każda kategoria otrzyma własne geny i generator.

## Weryfikacja i ograniczenia

`rtk proxy node docs/research/tools/infantry_capture.cjs environment` uruchamia prawdziwą przeglądarkę, wykonuje `tests/environment_checks.js` i zapisuje podgląd oraz atlas. Sprawdza 20 gatunków × 4 sezony, deterministyczność geometrii, niezmienność szkieletu między sezonami i detalami, brak owoców u młodych roślin, skrajne geny i podstawowe zdarzenia edytora. Raport liczby trójkątów jest w `research/images/environment.json`.

Pierwszy etap obejmował generator i laboratorium. Od v0.18 przycisk **START NEW** rozmieszcza sześć gatunków drzew na mapie 200 × 200 m — szczegóły w [opisie scenariusza](25_START_NEW.md). Rośliny nadal nie mają kolizji, animacji wiatru, śniegu, chorób, dziedziczenia przez krzyżowanie ani symulacji ekosystemu. Podgląd 20 gatunków służy porównaniu pierwszego zestawu stylizowanych modeli. Testy nie stanowią pomiaru FPS lasu; docelowy budżet świata wymaga osobnego benchmarku.
# World generation 1

Start gry otwiera konfigurator `world-generation-1`. Zapisuje on seed, rozmiar
mapy (400/600/800/1200 m), zagęszczenie roślinności i zabudowy oraz udział
ogrodzonych parcel w `localStorage`. Plan osady jest deterministyczny: najpierw
powstają drogi i parcele, potem niwelacja terenu, nawierzchnie, budynki,
ogrodzenia i instancje roślin. Roślinność sprawdza rezerwacje planu, dlatego nie
wchodzi na drogi ani działki. Drogi, chodniki i ogrodzenia pozostają wizualne.

Ogrodzenie każdej wybranej parceli powstaje z jej prostokątnej granicy. Otwór
bramy jest wycinany na krawędzi od strony drogi i ma szerokość drogi dojazdowej
plus zapas. Odcinki dzielą się na przęsła do 2,4 m; przed utworzeniem siatki
odrzucane są fragmenty przecinające korytarz drogowy. Słupki i przęsła drewna,
siatki oraz niskiego muru korzystają z wysokości terenu na końcach segmentu;
mur jest dodatkowo próbkowany co najwyżej co 1,2 m. Elementy powtarzalne
używają instancji Three.js, a ich wspólna geometria i materiały są zwalniane
przy usuwaniu świata. Skały również omijają rezerwacje dróg i parceli.
Kontrolę planu uruchamia `node tests/fence_generation.cjs`.
