# Building Lab: wnętrza i wyposażenie

[Wzorce](building-patterns.md) · [Bryły i dachy](building-roofs.md) · [Generator i odbiór](building-generator.md)

## Pomieszczenia i komunikacja

**Źródło:** rzuty autorów Dualchas, rundzwei i Fereos w [kartach wzorców](building-patterns.md). **Zasada:** pomieszczenia, wejście i komunikacja tworzą wspólny układ, z oknami przypisanymi do funkcji. **Uproszczenie:** centralny korytarz i cztery pokoje na kondygnację; skrzydło biura i zaplecze hali są większymi otwartymi pomieszczeniami. **Parametry:** pas centralny 3 m, ściana zewnętrzna 0,3 m, działowa 0,14 m, drzwi pokojowe 0,92 m. **Niedopuszczalne:** pokój bez ścieżki od wejścia, drzwi w połowie biegu schodów albo rzut z innymi przegrodami niż model.

To makieta gry: brak kuchni, łazienek, wind i pełnego programu mieszkań. Nie jest to projekt budowlany. Walidacja sprawdza graf dostępności; dodatkowo rezerwujemy geometryczne pasy komunikacji i miejsce przed drzwiami.

## Schody

**Źródło:** układ komunikacji na rzutach autorów powyżej; wymiarowanie proceduralne poniżej jest własną regułą gry. **Zasada:** różnicę wysokości trzeba powiązać z długością biegu i spocznikami. **Uproszczenie:** dwa równoległe biegi z półpiętrem, bez skrętnych stopni. **Parametry:** parzysta liczba podniesień `2 × ceil(wysokość / 0,18 / 2)`, szerokość biegu 1,2 m, stopnica 0,28 m, spocznik 1,2 m; `n` podniesień daje `n−1` stopnic i końcowy spocznik. Otwór w wyższym stropie obejmuje oba biegi i półpiętro. **Niedopuszczalne:** stałe dziewięć stopni niezależnie od wysokości, ucięty otwór w stropie, brak 1,2 m wolnej strefy przy wyjściu. Zbyt mały układ odrzucamy przed utworzeniem modelu.

## Stoły i szafy

**Źródło:** własne reguły gry, inspirowane funkcjonalnym podziałem autorskich rzutów Fereos; poniższe liczby nie pochodzą z pomiarów fotografii. **Zasada:** wyposażenie zajmuje wolną przestrzeń i nie blokuje drzwi. **Uproszczenie:** stół z blatem i czterema nogami; szafa z korpusem i uchwytem. **Parametry:** stół 1,15 × 0,72 × 0,76 m, bufor 0,6 m; szafa 1 × 0,56 × 1,9 m, bufor obsługi 0,75 m; pas przejścia 1,12–1,2 m. Szafa wymaga pełnego odcinka rzeczywistej ściany. **Niedopuszczalne:** szafa pod oknem, blat w świetle drzwi lub wyposażenie w otworze schodowym.

Każdy pokój ma niezależny strumień losowania wyposażenia. Rozmieszczenie próbuje kilku pozycji, odrzucając kolizje z drzwiami, schodami, trasami przejścia i wcześniej wybranym meblem. Jeśli nie ma miejsca, element zostaje pominięty. Minimalny dom może mieć same szafy; nie zmniejszamy rezerwy przejść, aby wymusić stół.

## Odbiór rzutu

Rzut rysuje te same `walls`, `slabs`, `rooms`, `stairs` i `furniture`, które trafiają do geometrii. Włącz „Do piętra rzutu”, wybierz kondygnację, wyłącz dach i sprawdź wejścia z obu stron. Ręcznie sprawdź także otwarte skrzydła drzwi, dojście do stołu i front szafy. Numeryczna dostępność nie zastępuje oceny wygody ani wyglądu.
