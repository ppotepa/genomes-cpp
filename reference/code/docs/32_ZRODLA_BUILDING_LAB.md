# Building Lab — bibliografia, dowody i dalsze pytania

Data dostępu i opracowania: **25 września 2026**.

[Raport główny](30_RESEARCH_PROCEDURALNYCH_BUDYNKOW.md) · [Atlas architektury](31_ATLAS_ARCHITEKTURY_PROCEDURALNEJ.md) · [Punkt startowy w Building Lab](../building-lab/RESEARCH.md)

## 1. Metoda i granice przeglądu

Zebrano **35 pozycji**: prace naukowe, dokumentację autorów narzędzi i standardów oraz instytucjonalne opracowania architektury. Źródła dobierano do konkretnych problemów obecnego generatora: typologii, planów, hierarchii reguł, dachów, materiałów, otworów, semantyki i reprezentacji w grze.

Wyszukiwanie prowadzono po nazwach metod i publikacji, m.in. „procedural modeling buildings shape grammar”, „residential building layouts”, „straight skeleton roof”, „graph constrained house generation”, „kamienica oficyny klatki”, „ITB wielka płyta” oraz nazwach materiałów i elementów. Preferowano strony autorów, uczelni, organizacji standaryzujących, NID, ITB, NPS i Historic England. Wyniki wyszukiwarki służyły odnalezieniu dokumentów; daty indeksowania nie zostały uznane za daty publikacji.

Zakres lektury obejmuje abstrakty i opisy projektów oraz odpowiednie fragmenty dostępnych dokumentów, nie pełną lekturę każdej bibliografii cytowanej przez autorów. Poniżej jawnie zaznaczono, na jakim materiale opiera się dana notatka. Nie odtwarzano eksperymentów, nie pobierano zbiorów treningowych i nie porównywano wyników wydajności.

Nie jest to systematyczny przegląd z protokołem wyszukiwania wszystkich baz. W szczególności nie stanowi wyczerpującego zestawienia najnowszych modeli generatywnych z lat 2024–2026. Celem jest podstawa dla sterowalnego generatora gry, a nie ranking aktualnego stanu badań nad AI.

W raportach oddzielono: fakt źródłowy, obserwację lokalnego kodu i autorską propozycję. Źródło o zabytkowej kamienicy potwierdza cechy opisanego obiektu; nie dowodzi rozkładu statystycznego całej zabudowy miasta. Własne tabele 24 rodzin i 16 dachów są katalogiem projektowym, nie katalogiem skopiowanym z jednej publikacji.

## 2. Gramatyki, rzuty i sterowanie

### S01. Wonka, Wimmer, Sillion, Ribarsky — Instant Architecture, 2003

[Strona publikacji TU Wien](https://www.cg.tuwien.ac.at/research/publications/2003/Wonka-2003-Ins/).

Materiał: abstrakt i metadane publikacji. Opisuje gramatyki podziału, mechanizm atrybutów i kontrolę wyboru reguł. Przydatność: hierarchia elementów oraz różne reguły elewacji. Ograniczenie: nie jest dowodem poprawności wszystkich wnętrz i konstrukcji w naszej grze.

### S02. Müller, Wonka, Haegler, Ulmer, Van Gool — Procedural Modeling of Buildings, 2006

[PDF na stronie współautora](https://www.peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf) · [opis ETH](https://emeritus.icu.ee.ethz.ch/research/demos/procedural-modeling.html) · [DOI](https://doi.org/10.1145/1179352.1141931).

Materiał: abstrakt, wstęp i opis projektu. CGA łączy modelowanie bryły z detalami i regułami kontekstu. Przydatność: unikanie kolizji elementów na różnych częściach bryły. Ograniczenie: praca o powłokach budynków nie zastępuje programu użytkowego, nawigacji i destrukcji.

### S03. Merrell, Schkufza, Koltun — Computer-Generated Residential Building Layouts, 2010

[PDF w zasobach dydaktycznych Princeton](https://www.cs.princeton.edu/courses/archive/spr11/cos598A/pdfs/Merrell10a.pdf) · [kopia autora w Stanford](https://cs.stanford.edu/people/eschkufz/docs/siggraph_asia_10.pdf).

Materiał: publikacja, przede wszystkim opis metody. Program architektoniczny i optymalizacja planu są oddzielnymi etapami prowadzącymi do modelu 3D. Przydatność: zależności przestrzeni przed generowaniem elewacji. Ograniczenie: badany zakres budynków mieszkalnych nie obejmuje automatycznie kamienic z oficynami i hal.

### S04. Smelik, Tutenel, Bidarra, Benes — A Survey on Procedural Modelling for Virtual Worlds, 2014

[Strona publikacji Purdue](https://www.cs.purdue.edu/cgvlab/www/publications/Smelik14CGF/).

Materiał: abstrakt i opis przeglądu. Pomaga umieścić modelowanie architektury w szerszym problemie proceduralnego świata, kontroli i współpracy metod. Ograniczenie: jest przeglądem literatury z danego okresu, nie aktualnym rankingiem narzędzi ani dowodem wydajności naszego rozwiązania.

### S05. Lipp, Wonka, Wimmer — Interactive Visual Editing of Grammars for Procedural Architecture, 2008

[Strona publikacji TU Wien](https://www.cg.tuwien.ac.at/research/publications/2008/LIPP-2008-IEV/).

Materiał: abstrakt i opis projektu. Dotyczy interaktywnego edytowania architektury gramatycznej, w tym zachowania lokalnych zmian. Przydatność: blokowanie części genomu i przewidywalna regeneracja. Ograniczenie: nie dostarcza gotowego kontraktu identyfikatorów ani formatu zapisów dla naszego projektu.

### S06. Parish, Müller — Procedural Modeling of Cities, 2001

[PDF ETH](https://cgl.ethz.ch/Downloads/Publications/Papers/2001/p_Par01.pdf).

Materiał: opis podejścia i abstrakt publikacji. Proceduralna organizacja miasta korzysta z reguł i danych ograniczających wynik. Przydatność: oddzielenie kontekstu urbanistycznego od pojedynczego budynku. Ograniczenie: skala miasta i proste bryły nie rozwiązują detalu wnętrz.

## 3. Geometria, reguły i semantyka

### S07. Esri — CGA Shape Grammar Reference

[Oficjalna dokumentacja CityEngine](https://doc.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm).

Materiał: dokumentacja języka i indeks operacji, wersja dostępna w dniu dostępu. Przydatność: praktyczny słownik transformacji i hierarchii kształtów. Ograniczenie: inspiracja koncepcyjna nie oznacza przenośności implementacji do klasycznych skryptów JS.

### S08. Esri — roofHip Operation

[Dokumentacja operacji](https://esri.github.io/cityengine-sdk/html/cgaref/cgareference/op_roofHip.html).

Materiał: opis parametrów i działania operacji. Przydatność: dach jako wynik reguły nad wejściową geometrią. Ograniczenie: nie jest ogólnym rozwiązaniem wszystkich form dachów ani ich konstrukcji i odwodnienia.

### S09. CGAL — 2D Straight Skeleton and Polygon Offsetting

[Podręcznik](https://doc.cgal.org/latest/Straight_skeleton_2/index.html).

Materiał: opis algorytmu, warunków wejścia, wersji ważonej i ekstrudowania dachu. Przydatność: połacie wynikające z obrysu i odporność topologiczna. Ograniczenie: CGAL jest biblioteką C++; dokumentacja `latest` może się zmieniać. W chwili dostępu strona wskazywała 6.2.1. Nie przyjęto zależności od tej biblioteki.

### S10. Mapbox — Earcut

[Repozytorium autorów](https://github.com/mapbox/earcut).

Materiał: README, interfejs i deklarowane ograniczenia triangulacji. Przydatność: wielokąty z otworami, potencjalnie stropy i ściany. Ograniczenie: triangulacja nie naprawia programu budynku i nie gwarantuje poprawności wszystkich zdegenerowanych konturów.

### S11. Jonathan Richard Shewchuk — Adaptive Precision Floating-Point Arithmetic and Fast Robust Predicates for Computational Geometry

[Strona autora, publikacje 1996–1997](https://www.cs.cmu.edu/~quake/robust.html).

Materiał: opis predykatów i podejścia do precyzji. Przydatność: testy orientacji i relacji geometrycznych w obrysach. Ograniczenie: odporne predykaty są fragmentem rozwiązania, nie kompletnym silnikiem operacji na bryłach.

### S12. Maxim Gumin — WaveFunctionCollapse

[Oryginalne repozytorium](https://github.com/mxgmn/WaveFunctionCollapse).

Materiał: README, opis algorytmu i sprzeczności. Przydatność: lokalne reguły sąsiedztwa i wzorce modułów. Ograniczenie: podstawowy algorytm nie gwarantuje globalnego programu i dostępności budynku. Nasza propozycja zastosowania jest pomocnicza.

### S13. Open Geospatial Consortium — CityGML

[Oficjalna strona standardu](https://www.ogc.org/standards/citygml/).

Materiał: opis standardu i modelu semantycznego. Przydatność: budynek opisany przez znaczenie części, nie wyłącznie trójkąty. Ograniczenie: nie analizowano całej specyfikacji ani nie zaprojektowano zgodnego eksportera. Standard miejski nie jest automatycznie formatem rozgrywki.

### S14. buildingSMART — IfcOpeningElement, IFC 4.3

[Oficjalna dokumentacja](https://ifc43-docs.standards.buildingsmart.org/IFC/RELEASE/IFC4x3/HTML/lexical/IfcOpeningElement.htm).

Materiał: semantyka pustki i relacji z elementem. Przydatność: rozdzielenie ściany, otworu i wypełnienia. Ograniczenie: inspiracja bez przyjęcia pełnego IFC. W dniu dostępu adres przekierowywał do dokumentacji oznaczonej 4.3.2.0 w ścieżce DEV; przy integracji trzeba przypiąć konkretną wersję.

## 4. Typologia i materiały architektury

### S15. NID — kamienica, Rynek Trybunalski 7, Piotrków Trybunalski

[Karta obiektu](https://zabytek.pl/pl/obiekty/g-223961).

Materiał: opis NID opracowany przez Agnieszkę Lorenc-Karczewską, 2020. Konkretny przykład układu U, traktów, komunikacji i różnych dachów skrzydeł. Przydatność: karta złożonego archetypu. Ograniczenie: nie określa częstości takich rozwiązań ani typowych wymiarów dla całej populacji kamienic.

### S16. NID — kamienica z oficynami, Szczecin

[Karta obiektu](https://zabytek.pl/pl/obiekty/szczecin-kamienica-23062).

Materiał: dostępny opis obiektu, w szczególności fragmenty o klatkach i bramach; bez pomiarów fotografii. Przydatność: wiele rdzeni i różne strefy dostępu w jednym zespole. Ograniczenie: bogato opracowany indywidualny obiekt nie jest przeciętną kamienicą.

### S17. ITB / Budowlane ABC — badania budynków wielkopłytowych

[Opis badań na stronie administracji](https://budowlaneabc.gov.pl/budownictwo-wielkoplytowe-raport-o-stanie-technicznym/opis-wykonanych-badan-budynkow-z-wielkiej-plyty-w-ramach-badan-itb-pt-ocena-bezpieczenstwa-i-trwalosc-budynkow-wykonanych-metodami-uprzemyslowionymi/).

Materiał: opis identyfikacji systemów, elementów i złączy. Przydatność: prefabrykacja jako system zależności, nie wzór spoin na dowolnej ścianie. Ograniczenie: nie wykorzystano progów diagnostycznych do HP i nie odwzorowano katalogu konkretnego systemu.

### S18. Lee H. Nelson / NPS — Architectural Character, Preservation Brief 17, 1988

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-17-architectural-character.pdf).

Materiał: trzy poziomy oglądu budynku i opis cech charakterystycznych. Przydatność: porządek oceny sylwety, materiałów i wnętrza. Ograniczenie: poradnik ochrony dziedzictwa nie dowodzi, ile trójkątów potrzebuje model RTS.

### S19. John H. Myers / NPS — The Repair of Historic Wooden Windows, Brief 9, 1981

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-09-wood-windows.pdf).

Materiał: opisy części okna i znaczenia jego charakteru. Przydatność: rozdzielenie ramy, skrzydła, podziałów i osadzenia. Ograniczenie: przykłady historycznej stolarki amerykańskiej wymagają lokalnej adaptacji; raport nie stosuje porad naprawczych do rzeczywistych budynków.

### S20. Anne E. Grimmer / NPS — The Preservation and Repair of Historic Stucco, Brief 22, 1990

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-22-stucco.pdf).

Materiał: odmiany tynku, podłoża i powierzchnie. Przydatność: warstwy materiału oraz zmienność faktury. Ograniczenie: nie przenosimy historycznych receptur do parametrów fizycznych gry ani nie definiujemy jednej obowiązującej mieszanki.

### S21. Jeffrey S. Levine / NPS — Historic Slate Roofs, Brief 29, 1992

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-29-slate-roofs.pdf).

Materiał: warianty łupku i detale kalenic, naroży oraz koszy. Przydatność: odrębna reguła powierzchni pokrycia i wykończeń krawędzi. Ograniczenie: wzory regionalne nie są uniwersalnym zestawem dla każdego budynku.

### S22. Anne E. Grimmer, Paul K. Williams / NPS — Historic Clay Tile Roofs, Brief 30, 1992

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-30-clay-tile-roofs.pdf).

Materiał: kształty dachówek i elementy specjalne. Przydatność: kierunkowość, zakład, osobne elementy wykończeniowe. Ograniczenie: nazewnictwo pokryć zmienia się regionalnie; nie użyto tabel jako gotowej normy kąta dachu.

### S23. Sharon C. Park / NPS — Controlling Unwanted Moisture, Brief 39, 1996

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-39-controlling-moisture.pdf).

Materiał: źródła wilgoci, przecieki i lokalizacja problemów. Przydatność: starzenie mające przyczynę i miejsce. Ograniczenie: proponowane maski wizualne są naszym uproszczeniem, nie modelem fizyki transportu wilgoci.

### S24. Historic England — Conserving Georgian and Victorian Terraced Housing, 2020

[Strona publikacji](https://historicengland.org.uk/images-books/publications/conserving-georgian-victorian-terraced-housing/).

Materiał: opis zakresu poradnika i typów zabudowy. Przydatność: oddzielny profil brytyjskiej zabudowy szeregowej. Ograniczenie: nie służy do ustalania geometrii polskich kamienic; nie wyprowadzano z niego wymiarów.

## 5. Metody uczone i dane

### S25. Nauata i in. — House-GAN, 2020

[Publikacja autorów](https://arxiv.org/abs/2003.06988).

Materiał: abstrakt i definicja zadania. Graf zawiera typy oraz relacje pomieszczeń, wynik obejmuje ich układ. Przydatność: porównanie podejścia uczonego z jawnymi ograniczeniami. Ograniczenie: wyniki autorów nie są dowodem poprawności wszystkich przyszłych układów gry.

### S26. Nauata i in. — House-GAN++, 2021

[Publikacja autorów](https://arxiv.org/abs/2103.02574).

Materiał: abstrakt i opis iteracyjnego dopracowania układu. Przydatność: utrzymywanie części warunków podczas kolejnych zmian. Ograniczenie: nie oceniano kosztu treningu, uruchomienia ani możliwości użycia bezpośrednio w JS.

### S27. Tang i in. — Graph Transformer GANs for Graph-Constrained House Generation, CVPR 2023

[PDF w oficjalnych materiałach CVPR](https://openaccess.thecvf.com/content/CVPR2023/papers/Tang_Graph_Transformer_GANs_for_Graph-Constrained_House_Generation_CVPR_2023_paper.pdf).

Materiał: abstrakt i opis zakresu generacji. Przydatność: dalszy kierunek badań nad sterowaniem przez grafy, obejmujący również modele dachów. Ograniczenie: nie porównywano jakości i nie zakłada się, że wyuczona metoda zastąpi walidację geometryczną.

### S28. Zheng i in. — Structured3D, ECCV 2020

[Oficjalna strona zbioru](https://structured3d-dataset.org/).

Materiał: opis syntetycznych projektów i adnotacji struktur. Przydatność: inspiracja dla semantycznej reprezentacji wnętrz i przyszłych porównań. Ograniczenie: zbioru nie pobrano; warunki użycia danych są osobne od licencji kodu. Nie jest reprezentatywną próbą wszystkich historycznych budynków Polski.

### S29. Paschalidou i in. — ATISS: Autoregressive Transformers for Indoor Scene Synthesis, 2021

[Strona autorów / NVIDIA Research](https://research.nvidia.com/labs/toronto-ai/ATISS/).

Materiał: opis zadania syntezy wyposażenia scen wewnętrznych. Przydatność: możliwy późniejszy etap meblowania. Ograniczenie: ustawianie obiektów w pomieszczeniu jest innym zadaniem niż konstrukcja całego budynku i jego planu.

## 6. Runtime oraz dodatkowe rodziny detali

### S30. Recast Navigation

[Dokumentacja projektu](https://recastnav.com/).

Materiał: opis Recast i Detour. Przydatność: rozdzielenie generowania danych nawigacyjnych i obsługi ruchu. Ograniczenie: nie wybrano portu JS/WASM i nie wykonano integracji z istniejącymi jednostkami.

### S31. Three.js — InstancedMesh, tag r128

[Źródło przypiętej wersji](https://github.com/mrdoob/three.js/blob/r128/src/objects/InstancedMesh.js).

Materiał: kod konstruktora i reprezentacji instancji. Potwierdza m.in. ustawienie `frustumCulled = false` w tej wersji. Przydatność: planowanie grup renderujących zgodnie z rzeczywistym stackiem. Ograniczenie: bez testów nie przypisujemy konkretnego przyspieszenia ani zachowania późniejszych wydań.

### S32. H. Ward Jandl / NPS — Rehabilitating Historic Storefronts, Brief 11, 1982

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-11-storefronts.pdf).

Materiał: anatomia witryn i przekształcenia parterów. Przydatność: osobna kompozycja usługowej kondygnacji i lokalna historia remontu. Ograniczenie: szczegóły amerykańskich przykładów nie określają wyglądu polskich szyldów i sklepów.

### S33. Sharon C. Park / NPS — Historic Steel Windows, Brief 13, 1984

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-13-steel-windows.pdf).

Materiał: wprowadzenie do rodzajów i wykonania okien stalowych. Przydatność: odróżnienie smukłych ram przemysłowych od drewnianej stolarki. Ograniczenie: nie przeanalizowano katalogów producentów ani historycznych modułów dla konkretnej polskiej fabryki.

### S34. Michael J. Auer / NPS — The Preservation of Historic Barns, Brief 20, 1989

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-20-barns.pdf).

Materiał: przegląd odmian stodół i ich charakterystycznych cech. Przydatność: odrębny program gospodarczy i znaczenie układu bryły. Ograniczenie: katalog odnosi się do USA; polska zagroda potrzebuje własnych referencji regionalnych.

### S35. Paul Gaudette, Deborah Slaton / NPS — Preservation of Historic Concrete, Brief 15, 2007

[PDF NPS](https://www.nps.gov/orgs/1739/upload/preservation-brief-15-concrete.pdf).

Materiał: opis właściwości wizualnych i degradacji betonu. Przydatność: powierzchnia związana z wykonaniem elementu i lokalnymi zmianami. Ograniczenie: nie wyprowadzano wartości odporności bojowej ani parametrów symulacji konstrukcji.

## 7. Mapa: na czym opierają się najważniejsze wnioski

| Pytanie | Źródła | Co pozostaje autorską propozycją |
|---|---|---|
| Jak organizować powtarzalne części? | S01, S02, S07 | Nasza struktura klas i konkretny genom |
| Jak powiązać plan z funkcją? | S03 | Graf i solver odpowiedni dla piechoty |
| Jak zachować kontrolę nad lokalną zmianą? | S04, S05 | Haszowanie ścieżek, wersje i format nadpisań |
| Jak uwzględnić parcelę i ulicę? | S06, S15, S16 | Reguły dzielnicy i sąsiedztwa w grze |
| Jak generować dachy i poprawne siatki? | S08–S11 | Wybór implementacji i reguły wszystkich 16 rodzin |
| Czy WFC wystarczy do całego budynku? | S12 | Dodatkowa walidacja globalnych relacji |
| Jak rozdzielić element, otwór i mesh? | S13, S14 | Lekki własny manifest zamiast pełnego BIM |
| Dlaczego rozróżniać kamienice i bloki? | S15–S17 | Katalog 24 rodzin i ich dystrybucja w świecie |
| Co zachować w prostym wyglądzie? | S18–S22, S32–S35 | Priorytety detalu w kamerze tej gry |
| Jak modelować starzenie? | S23 | Maski powierzchni i zdarzenia historii |
| Jak nie mieszać regionów? | S15, S16, S24, S34 | Profile świata i dopuszczalne kombinacje |
| Co dają modele uczone? | S25–S29 | Decyzja, czy istnieje potrzeba ich wdrożenia |
| Jak połączyć generację i runtime? | S30, S31 oraz kod repozytorium | Cache, LOD, workers i aktualizacja destrukcji |

## 8. Brakujące dane do docelowej teorii

| Luka | Dlaczego jest istotna | Jak ją zamknąć w kolejnym etapie |
|---|---|---|
| Domyślny region i epoka | Bez nich „realistyczny budynek” nie ma jednoznacznego wzorca | Wybrać profil świata i rodziny referencyjne |
| Pełne karty 24 rodzin | Tabela to zakres projektu, nie komplet dokumentacji każdej rodziny | Zebrać kilka rzeczywistych przykładów, rzuty i przekroje dla wybranych rodzin |
| Moduły konkretnych systemów bloków | Nie można odtworzyć systemu wyłącznie kolorem i spoinami | Przeanalizować katalog właściwego systemu prefabrykacji |
| Polskie regionalne domy i zagrody | Referencje amerykańskie nie wystarczą | Uzupełnić opisami skansenów, muzeów i inwentaryzacjami NID |
| Przekroje schodów i stolarki | Realizm oraz przechodniość zależą od głębokości, nie tylko rzutu | Przygotować własne karty detalu na podstawie wybranych referencji |
| Rozkłady prawdopodobieństwa | Literatura nie ustala udziału naszych archetypów na mapie | Opracować profil dzielnicy i kalibrować na zestawie przykładów |
| Kryteria fizyki gry | Nie ustalono skali zawalania i interakcji z gruzem | Zdefiniować uproszczenia podpór oraz kontrakt destrukcji |
| Głębokość wnętrz | Zmienia zakres generatora i budżet reprezentacji | Wybrać, gdzie potrzebne są wnętrza taktyczne |
| Najnowsze modele generatywne | Nie wykonano pełnego przeglądu prac 2024–2026 | Osobna analiza dopiero przy decyzji o narzędziu uczonym/offline |

Te luki nie uniemożliwiają napisania pierwszej teorii generatora. Oznaczają miejsca, gdzie trzeba podjąć decyzję projektową lub zebrać dane, zamiast przypisywać wymyślonym parametrom naukowe uzasadnienie.

## 9. Proponowany formularz referencji budynku

Każdy przyszły archetyp powinien mieć krótką, powtarzalną kartę:

1. Obiekt lub rodzina; region, epoka, funkcja, źródło i data.
2. Poziom pewności: opis, zmierzony plan, fotografia, interpretacja.
3. Rzut parceli, bryły, kondygnacji i komunikacji; orientacja frontu.
4. Konstrukcja, pokrycie, wykończenie i stolarka — osobne pola.
5. Cechy obowiązkowe dla wybranego archetypu oraz dopuszczalne warianty.
6. Zdarzenia historii: co zostało dodane, przebudowane lub wymienione.
7. Dane, których nie znamy; bez dopisywania fikcyjnych wymiarów.
8. Przyjęte uproszczenie gry: wnętrza, kolizje, detale i zachowanie po uszkodzeniu.
9. Status praw do zdjęć, planów i danych, jeśli miałyby wejść do repozytorium jako zasoby.

Obecny pakiet zawiera własne opisy i odnośniki. Nie importowano zdjęć zabytków, planów, modeli, tekstur ani danych treningowych. Dostępność publikacji nie została potraktowana jako automatyczne zezwolenie na użycie jej ilustracji jako assetów gry.
