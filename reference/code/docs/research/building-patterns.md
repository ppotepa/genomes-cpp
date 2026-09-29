# Building Lab: cztery wzorce architektury

[Indeks researchu](index.html) · [Bryły i dachy](building-roofs.md) · [Wnętrza](building-interiors.md) · [Generator i odbiór](building-generator.md)

Opracowanie: 25.09.2026. Poniższe zakresy są założeniami gry. Referencje dostarczają zasad kompozycji, a nie norm wymiarowania ani statystycznego obrazu całej Europy. Nie kopiujemy projektów ani fotografii do assetów. Opisy publikowane w ArchDaily są oznaczone jako dostarczone przez architektów; korzystamy z ich tekstów i autorskich rysunków, nie z ocen redakcji.

| Wzorzec / identyfikator | Obrys i kondygnacje | Wymiary całkowite w metrach | Dach |
|---|---|---|---|
| Dom / `EURO_HOUSE` | RECT, 1–2 | 9–13 × 9–13; piętro 2,8–3,2 | GABLE lub HIP |
| Mały blok / `EURO_APARTMENT` | RECT, 3–5 | 12–20 × 11–18; piętro 2,8–3,2 | FLAT |
| Usługi i biura / `EURO_OFFICE` | RECT albo L, 2–3 | 16–24 × 12–18; piętro 3–3,5 | FLAT |
| Hala z zapleczem / `EURO_HALL` | HALL_ANNEX, 1 | 18–30 × 12–24; hala 4,2–5,4, aneks 3 | GABLE + SHED |

## Dom jednorodzinny

**Źródło i referencje:** [Beach House, Dualchas Architects, Morar](https://www.archdaily.com/630457/beach-house-dualchas-architects), fotografie Andrew Lee oraz rzuty parteru i piętra autorów projektu. [Rzut parteru](https://www.archdaily.com/630457/beach-house-dualchas-architects/55529273e58ece8a26000012-beach-house-dualchas-architects-ground-floor-plan).

**Obserwowana zasada:** prosta sylwetka z dachem dwuspadowym i otwory związane z orientacją pomieszczeń. **Uproszczenie gry:** jeden prostokąt, bez wspornika i zagłębienia parteru obecnych w referencji. Wariant kopertowy jest własnym rozszerzeniem. **Parametry:** 1–2 kondygnacje, matowy tynk lub spójna paleta wybranego materiału, pionowo powtarzany układ okien. **Niedopuszczalne:** okno przecięte ścianką działową, dwa nakładające się prostokąty udające dach kopertowy.

## Mały blok mieszkalny

**Źródło i referencje:** [Iceberg, rundzwei Architekten, Berlin](https://www.archdaily.com/975329/iceberg-residential-building-rundzwei-architekten), fotografie Gui Rebelo, autorskie `Plan – 5th floor` i `Section AA` w galerii.

**Obserwowana zasada:** regularna elewacja uliczna i świadome umieszczenie komunikacji względem powierzchni lokali. **Uproszczenie gry:** 3–5 powtarzalnych poziomów i wewnętrzne schody; bez wykusza, zewnętrznego trzonu i mieszkań dwupoziomowych referencji. Rzut gry ma dostępne pokoje, ale nie pełne mieszkania z instalacjami. **Parametry:** prostokąt, dach płaski, wspólne osie otworów i przegród. **Niedopuszczalne:** losowe przesunięcie okien na każdym piętrze lub drzwi pokoju otwierające się w bieg schodów.

## Usługi i biura

**Źródło i referencje:** [Office Building, Dali, Fereos Architects](https://www.fereos.net/project/office-building-dali-nicosia/). Strona autorów zawiera widoki zewnętrzne `PIC. 01–02` oraz rzuty `PIC. 03–05`.

**Obserwowana zasada:** biura i sale przy obwodzie, wspólne funkcje na parterze, możliwość łączenia części. **Uproszczenie gry:** 2–3 kondygnacje na poziomym terenie; prostokąt albo L, bez uniesienia brył z referencji. **Parametry:** dach płaski, jeden trzon, portal w faktycznym wspólnym odcinku ściany skrzydeł. **Niedopuszczalne:** dach zasłaniający pusty narożnik L lub usunięcie całej elewacji tylko dlatego, że fragment przylega do skrzydła.

## Hala z zapleczem

**Źródło i referencje:** [Sohm Holzbau, HK Architekten, Alberschwende](https://www.hkarchitekten.at/de/projekt/holzbau-sohm/), fotografie Bruno Klomfar i [autorskie plany PDF](https://www.hkarchitekten.at/v74/wp-content/uploads/2016/05/07_43-plaene.pdf): parter s. 2–3, piętro s. 4–5, przekrój hali i biur s. 6.

**Obserwowana zasada:** większa przestrzeń hali jest funkcjonalnie związana z drobniejszym programem biurowym. **Uproszczenie gry:** zamknięta hala i niższy boczny aneks; dach dwuspadowy i jednospadowy są naszym wariantem, nie odwzorowaniem dachu Sohm. **Parametry:** szerokość aneksu 23% całości, jego długość 70% głębokości; osobna wysokość 3 m i połączenie drzwiowe. **Niedopuszczalne:** aneks odłączony szczeliną, dwa fundamenty o różnych przesunięciach albo okno trafiające w słup ramy.

## Rytm elewacji

**Źródło:** [CityEngine Facade Wizard](https://doc.arcgis.com/en/cityengine/2022.1/tutorials/tutorial-13-facade-wizard.htm). **Zasada:** hierarchiczny podział na parter, piętra i moduły, z wyrównaniem osi między poziomami. **Uproszczenie:** podziały pomieszczeń i osie zapisane w planie wyznaczają przedziały okien; kolor jest losowany raz dla palety. **Parametry:** parapet 0,85 m, okna hali od 2,4 m, szerokość ograniczona miejscem między osiami. **Niedopuszczalne:** osobny losowy rytm konstrukcji, okien i pomieszczeń.
