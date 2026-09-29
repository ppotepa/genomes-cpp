# Building Lab — research i reguły generatora

Aktualny pakiet implementacyjny z 25 września 2026:

1. [Wzorce architektoniczne](../docs/research/building-patterns.md) — cztery poprawione typy, fotografie i rzuty autorów projektów, parametry oraz niedopuszczalne wyniki.
2. [Bryły i dachy](../docs/research/building-roofs.md) — wspólne ściany, analityczne połacie, obrys L i dalszy kierunek straight skeleton.
3. [Wnętrza i wyposażenie](../docs/research/building-interiors.md) — pomieszczenia, schody, stoły, szafy i wolne przejścia.
4. [Architektura generatora i odbiór](../docs/research/building-generator.md) — moduły, API, weryfikacja i gotowe przypadki ręcznego odbioru.

AUTO korzysta z czterech obsługiwanych wzorców (dom, biuro, hala i blok). W jakości v6 wzorce mają warstwowy WallAssembly, warianty brył i rzutów, rezerwacje otworów oraz deterministyczny parametr szkód 0–100%. Dom otrzymuje przybudówkę z prawdopodobieństwem 60%, jeśli wymiary pozwalają zachować minimalny rdzeń; biuro i hala mają zmienny zakres długości skrzydła/aneksu.

## Ręczny odbiór

Dla każdego przypadku z `acceptance-cases.json` powtórz seed i sprawdź cztery elewacje, dach z góry, przekrój oraz rzut każdej kondygnacji. Potwierdź dojście od wejścia do każdego pokoju, połączenie aneksu, brak zdublowanych ścian i zgodność osi okien z pokojami. Sprawdź szkody 0%, 50% i 100%, użyj „Wybij wyłom”, a następnie „Przywróć stan początkowy”; manifest powinien wrócić do tego samego stanu początkowego.

## Materiały wcześniejszej iteracji

Pakiet z 25 września 2026 opisuje metody, architekturę oraz ograniczenia kodu. Pierwsza wersja katalogu typologii, brył i dachów jest już dostępna w generatorze; poszczególne rodziny mają różny poziom szczegółowości.

Czytaj w tej kolejności:

1. [Raport główny](../docs/30_RESEARCH_PROCEDURALNYCH_BUDYNKOW.md) — stan kodu, porównanie algorytmów, genom, rzuty, dachy, wnętrza, destrukcja, cache i walidacja.
2. [Atlas architektury](../docs/31_ATLAS_ARCHITEKTURY_PROCEDURALNEJ.md) — 24 rodziny budynków, 16 rodzin dachów, konstrukcje, materiały, okna, drzwi i detale; osobne rozwinięcie kamienic, bloków i hal.
3. [35 źródeł i mapa dowodów](../docs/32_ZRODLA_BUILDING_LAB.md) — literatura naukowa, dokumentacja, NID, ITB, NPS, Historic England oraz jawne ograniczenia przeglądu.

Główna propozycja do dalszej pracy: budynek opisujemy zależnymi regułami funkcji, konstrukcji i wyglądu. Genom, model logiczny, siatki renderujące i stan uszkodzeń mają osobne odpowiedzialności.

[Uruchom laboratorium](index.html) · [Opis aktualnej implementacji](../docs/research/building-generator.md)
