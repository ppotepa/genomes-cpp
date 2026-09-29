# Cache geometrii bez utraty proceduralności

## Ustalenia z kodu

- Materiały ciała, ekwipunku i proceduralna tekstura tkaniny już są współdzielone (`InfantryMaterials`, `InfantryGear`). Nie generują się dla każdego żołnierza od nowa.
- Drzewa scenariusza mają sześć proceduralnych prototypów i są rysowane jako instancje (`Battlefield`). Nie istnieje 90 odrębnych kompletów geometrii.
- Przełączanie detalu piechoty wcześniej za każdym razem wywoływało `InfantrySurface.create` i `InfantryGear.build`. Powrót kamery odtwarzał te same wierzchołki, morph targety i ich normalne. Poprzednia geometria była usuwana.
- `SurfaceBuilder.finish` wylicza normalne również dla każdego morph targetu. Warto zachować wynik, zamiast liczyć go ponownie dla niezmienionego osobnika.
- Wszystkich 50 żołnierzy scenariusza nadal wykonuje pełne kroki animacji. Cache siatek nie usuwa kosztu animacji, cieni, liczby trójkątów ani draw calli. Nie wskazujemy dominującego kosztu klatki bez profilu CPU/GPU.

## Wdrożenie

`AppearanceCache` przechowuje nieaktywne zestawy: geometria ciała, morph targety, metadane i dopasowany ekwipunek. Klucz zawiera wersję generatora, dokładne parametry fenotypu używane do budowy, kolor strony, poziom detalu oraz rozwiązane sloty, warianty, paletę i zużycie wyposażenia. Nie zaokrągla genów, nie redukuje populacji do kilku gotowych sylwetek.

Aktywny model wyjmuje wpis z cache i przejmuje jego geometrię na wyłączność. Dzięki temu dwa jednocześnie widoczne modele nie współdzielą mutowalnej geometrii. Szkielet, pozycja, wagi morph targetów, broń i jej animacja pozostają własnością konkretnej jednostki. Przy użyciu buforów w nowym rigu odtwarzany jest `EquipmentFit` dla jego anatomii.

Po zmianie detalu lub usunięciu jednostki poprzedni zestaw trafia do cache. Limit to **64 MiB buforów geometrii i 48 wpisów**; najstarsze wpisy są usuwane. Limit dotyczy tablic geometrii, nie całkowitego RAM aplikacji ani pamięci GPU. Aktywne modele, obiekty JS i metadane są poza tym pomiarem. Zamykanie gry opróżnia cache. To cache sesyjny w RAM, bez zapisu na dysk.

Panel **Wydajność** pokazuje rozmiar buforów, liczbę wpisów i trafień. Z konsoli:

```js
RTS.AppearanceCache.stats();
RTS.AppearanceCache.clear();
RTS.AppearanceCache.setLimit(32 * 1024 * 1024);
```

Pierwszy, nowy genom wciąż wymaga wygenerowania. Losowy START NEW nie ma zagwarantowanych trafień, ponieważ celowo tworzy inne osobniki. Najbardziej bezpośrednia korzyść to powrót do wcześniej używanego detalu lub wyglądu. Przechowywany jest wynik procedury, nie zastępczy model o ograniczonej różnorodności.

## Pomiar i testy

`rtk proxy node docs/research/tools/infantry_capture.cjs cache,weaponui,battlefield`

Pierwszy pomiar czterech przełączeń detalu jednego żołnierza: **191,4 ms bez cache i 0,6 ms z rozgrzanym cache**. Bez cache wykonano cztery generowania; z cache zero. Jest to pojedynczy mikrobenchmark CPU w testowej przeglądarce, nie obietnica proporcjonalnego wzrostu FPS. Bieżący wynik powtórzenia: [cache.json](research/images/cache.json).

Testy sprawdzają tożsamość odzyskanej geometrii, zachowanie rigu i mimiki, unieważnienie po zmianie ekwipunku, niezależność dwóch aktywnych osobników, powtórne generowanie tego samego DNA, brak trafienia dla innego DNA i usuwanie wpisów po zmniejszeniu limitu. Osobno wykonywane są testy broni i scenariusza 25 vs 25.

## Kolejność dalszej pracy

1. Profil pełnej klatki: czas symulacji, animacji, interpolacji, przesyłania kości i renderowania; osobno czas GPU, jeśli dostępny. Pomiar przed i po na tym samym seedzie i ustawieniu kamery.
2. Animacyjny LOD: rzadsze aktualizowanie mimiki i dalekich postaci z interpolacją. Zachować kontakt stóp i deterministyczny czas symulacji; nie pomijać logiki tylko dlatego, że jednostki nie widać.
3. Prawdziwe odległe LOD drzew i piechoty. Obecny cache przyspiesza przełączenie, lecz nie upraszcza istniejących siatek.
4. Cache bazowej geometrii broni po oddzieleniu geometrii od parametrów skali i koloru. Aktualnie warianty mają te parametry zapisane w wierzchołkach, więc przypadkowe współdzielenie tylko po nazwie broni byłoby błędem.
5. Worker do budowania buforów i opcjonalny cache na dysku/IndexedDB. Najpierw wydzielić generator z zależności od sceny i globalnego stanu (`activeFit`, `activeAnatomy` w generatorze powierzchni). Worker skróci blokowanie UI; nie gwarantuje mniejszego łącznego kosztu obliczeń. Cache trwały wymaga wersjonowania generatorów i schematów oraz limitu miejsca.

Każdy z tych etapów może pozostać w JavaScript i działać również po spakowaniu aplikacji do EXE.
