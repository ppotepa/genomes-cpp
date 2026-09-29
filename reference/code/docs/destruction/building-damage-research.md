# Destrukcja budynków: diagnoza, źródła i kryteria odbioru

Data przeglądu: 2026-09-25. Zakres: pojedynczy skopiowany budynek w DESTRUCTION
DEMO. Parametry penetracji i wytrzymałości są przybliżeniami modelu gry. Ten
dokument nie jest tabelą odporności rzeczywistych konstrukcji ani amunicji.

## Wniosek

Ściana powinna mieć skończony cykl życia: nienaruszona sekcja, sekcja z otworem
lub kilkoma płytami, odłączony gruz, usunięty fragment. Trafienie musi zmieniać
ten sam stan, z którego korzystają balistyka, graf podpór, widoczna geometria
i fizyka. Osiągnięcie limitu części nie może utrwalać niezniszczalnej ściany.

Wdrożony model zachowuje balistyczne wyznaczanie trafień i energii
resztkowej, a ogranicza liczbę zmian geometrii. Dalsze zniszczenie końcowego
fragmentu usuwa go zamiast tworzyć kolejne pokolenia odłamków. Duże płyty mogą
trafić do Rapier; drobny gruz i pył otrzymują ograniczony budżet efektów.

Dalszą diagnozę wgnieceń, czytelności wybuchów oraz wiszących fragmentów
opisuje [analiza ubytków, efektów i stabilności](impact-effects-and-stability.md).

## Korekta poprzedniej diagnozy i dowodów

Poprzednia wersja dokumentu błędnie stwierdzała, że adapter przypisywał prawie
wszystkim ścianom beton. Wcześniejszy kod rozpoznawał już cegłę i kamień.
Potwierdzony problem dotyczył między innymi mapowania drewna i metalu oraz
doboru materiału konstrukcji: materiał elewacji nie może automatycznie
decydować o odporności betonowego fundamentu lub stropu. Dobór wymaga
uwzględnienia kategorii komponentu i ustroju konstrukcji.

Mnożnik pracy penetracji AP równy `0.55` zmniejszał opór już przed poprzednią
zmianą. Zastąpienie go `0.35` było strojeniem gry. Samo obniżenie współczynnika
nie dowodzi naprawy toru pocisku ani widocznego uszkodzenia budynku; przytoczone
publikacje nie uzasadniają tej konkretnej wartości. Bieżąca poprawka nie
zmienia go ponownie.

Audyt rdzenia wykazał dodatkowe błędy:

- Zdarzenie trafienia potrafiło raportować tylko ostatni krótki odcinek
  przejścia przez ścianę. Dla przeciwnych kierunków dawało inne raportowane
  grubości i straty energii mimo podobnego faktycznego uszkodzenia materiału.
  Raport powinien kumulować cały kontakt, od wejścia do wyjścia.
- Gdy odejmowanie ubytku usuwało całą sekcję i zwracało pustą listę pozostałości,
  kod przywracał oryginalną bryłę. Pusty wynik musi oznaczać zniszczenie.
- Wyznaczanie płaszczyzny z pierwszych trzech punktów ściany było zawodne po
  cięciu, gdy punkty leżały na jednej prostej. Potrzebna jest normalna
  wyznaczona z całego wielokąta.
- Osiągnięcie limitu podziału lub geometrii wymaga stanu końcowego zniszczenia;
  odmowa dalszego cięcia nie może zatrzymać uszkadzania sekcji.

Audyt adaptera potwierdził również problemy widoczne dopiero na pełnym celu:

- Wykończenia, obramowania otworów, cokoły, detale dachu i wyposażenie nie miały
  powiązanego właściciela uszkodzeń, więc mogły pozostać widoczne po usunięciu
  ściany. Poprawka przypisuje im sekcję, wraz z którą znikają.
- Prostopadłościan obejmujący skośną połać dachową dodawał materiał wewnątrz
  pustej przestrzeni. Adapter korzysta teraz z otoczki wypukłej geometrii;
  cienkie szczyty dostają jawną zastępczą grubość.
- Zakotwienie każdej ściany dotykającej gruntu utrwalało podparcie po utracie
  fundamentu. Zakotwienie przysługuje fundamentom; również wzorce
  eksperymentalne otrzymują graf sekcji.
- Usuwanie starej bryły z puli fizyki nie wycofywało spójnie jej objętości
  balistycznej i obrazu. Kolejne zastąpienia siatki pozostawiały też należące
  do demo zasoby do zwolnienia dopiero przy zamknięciu sesji.
- Pełna synchronizacja fizyki po każdym zdarzeniu mnożyła koszt seryjnego
  wybuchu. Aktualizacje są grupowane w przebiegu adaptera.

Kolejny audyt odróżnił dwa źródła wiszących elementów. Graf dopuszczał zbyt
długi łańcuch podparcia bocznego i zerowanie jego kosztu na pionowym
połączeniu. Osobno, utrwalanie uśpionego gruzu jako `Fixed` uniemożliwiało
ponowne opadnięcie po usunięciu podpory. Obecny graf zachowuje ograniczony
dystans połączeń, a uśpiony gruz pozostaje dynamiczny.

Stary efekt HE zawierał tylko małe brązowe wielościany, bez wyraźnego błysku
i chmury. Jego zegar zatrzymywał się wraz z tickami podczas przebudowy celu,
a pula nadal aktualizowała wygasłe instancje. Nowe efekty mają oddzielny
zegar klatek, krótki błysk, materiałowy pył oraz zwartą listę aktywnych części.

Poprzedni test przebicia wygenerowanej ściany wykonywał dwie osobne czynności:

1. Strzelał AP do nowego `MaterialModel` z jedną skopiowaną ścianą.
2. Wywoływał `damage()` bezpośrednio na komponencie wyświetlanego celu, po czym
   sprawdzał zastąpienie oryginalnej siatki.

Nie był to test pełnego przepływu strzału przez adapter budynku do obrazu,
kolizji i grafu podpór. Testy zawalenia przez bezpośrednie wywołania
`fracture()` lub `detach()` potwierdzały zachowanie tych metod, lecz nie
dowodziły możliwości wyburzenia domu dostępną amunicją. Liczby zaliczonych
testów z wcześniejszych podsumowań nie zamykają zgłoszonego problemu.

Wyniki benchmarku rdzenia bez renderowania nie potwierdzają płynności całego
demo. Pomiar musi uwzględniać również przebudowę siatek, tworzenie colliderów
i synchronizację adaptera. Rzadki kosztowny wybuch może wypaść poza p95
uśrednionego przebiegu; potrzebny jest także szczytowy czas klatki.

## Co wspierają źródła

### Skończone sekcje i graf połączeń

NVIDIA Blast opisuje hierarchię fragmentów, połączenia z określoną
wytrzymałością oraz wykrywanie grup odłączonych po zerwaniu połączeń.
Połączenia z otoczeniem pozwalają oznaczyć grupę podpartą przez świat.
Usuwanie końcowych fragmentów i zarządzanie ich reprezentacją graficzną oraz
fizyczną pozostaje zadaniem aplikacji. To uzasadnia oddzielny stan końcowy
gruzu i jawne usuwanie go z wszystkich reprezentacji.
[NVIDIA Blast — Introduction](https://docs.omniverse.nvidia.com/kit/docs/blast-sdk/latest/docs/api/introduction.html)

Chaos grupuje fragmenty w hierarchię klastrów. Dopóki grupa jest połączona,
nie trzeba traktować każdej jej części jako osobnej aktywnej bryły. Epic
wskazuje ograniczenie liczby obliczeń kolizji jako korzyść grupowania.
Przeniesienie tej zasady do demo oznacza większe sekcje ścian i płyt przed
odłączeniem, z ograniczoną liczbą ich potomków.
[Epic — Cluster Geometry Collections](https://dev.epicgames.com/documentation/unreal-engine/cluster-geometry-collections-user-guide-in-unreal-engine)

Sam graf łączności opisuje utratę podparcia. Nie wylicza obciążeń ani tego,
czy pozostały cienki słup utrzyma dach. Blast ma osobne rozszerzenie do
iteracyjnego rozprowadzania sił i kontroli granic rozciągania, ściskania oraz
ścinania. Nasz graf kontaktów pozostaje przybliżeniem gry; pełna analiza
naprężeń nie jest warunkiem obecnej poprawki.
[NVIDIA Blast — Stress Solver](https://docs.omniverse.nvidia.com/kit/docs/blast-sdk/latest/docs/api/extensions/ext_stress.html)

### Koszt wyburzenia i gruzu

Epic opisuje ograniczanie liczby rozbić i nowo aktywowanych brył w pojedynczej
klatce, usuwanie fragmentów po rozbiciu lub uśpieniu oraz zastępowanie małych
części efektami cząsteczkowymi. Do pocisków wskazuje lekkie śledzenie trafień
i zadawanie lokalnego uszkodzenia. Są to wskazówki architektoniczne; nie
przenoszą liczb FPS z Unreal do tej aplikacji WebGL.
[Epic — Chaos Destruction Optimization, UE 5.6](https://dev.epicgames.com/documentation/unreal-engine/chaos-destruction-optimization?application_version=5.6)

Rapier odradza siatki trójkątów jako collidery ruchomych brył i zaleca
reprezentację przez części wypukłe. Płyty i większy gruz mogą korzystać z
prostych brył wypukłych. Wniosek dla naszego modelu: pojedyncza otoczka
wypukła ściany z otworem zamknęłaby ten otwór; pozostałe części potrzebują
osobnych colliderów.
[Rapier — Colliders](https://rapier.rs/docs/user_guides/javascript/colliders/)

Uśpiona bryła nie jest aktywnie symulowana, lecz może zostać obudzona przez
kontakt. Uśpienie nie usuwa jej geometrii ani obiektu fizyki. Dlatego obok
limitu aktywnych brył potrzebny jest limit całej zachowanej puli oraz
usuwanie jej najstarszych zbędnych elementów.
[Rapier — Sleeping](https://rapier.rs/docs/user_guides/javascript/rigid_body_sleeping/)

### Granice realizmu balistycznego

Badania rozróżniają lokalne uszkodzenia betonu, penetrację i pełną perforację.
Nie wynika z nich, że AP powinien usuwać całą trafioną ścianę. W modelu gry
otwór wylotowy wymaga przejścia pocisku przez materiał; przerwanie całej sekcji
jest osobnym skutkiem utraty jej integralności.
[Li i in. — Local impact effects of hard missiles on concrete targets](https://doi.org/10.1016/j.ijimpeng.2005.04.005)

Raport DRDC wykazał różnice granicy balistycznej między badanymi partiami
płyt tej samej klasy RHA. Wynik zależy od konkretnego pocisku i materiału;
nie stanowi uniwersalnej wartości odporności wszystkich ścian.
[DRDC — RHA steel variations and their effects on ballistic protection](https://publications.gc.ca/site/eng/9.821408/publication.html)

Obliczanie trajektorii, grubości warstwy i energii resztkowej zapewnia spójność
modelu gry. Nie jest samo w sobie walidacją rzeczywistej penetracji.
Katalog, mnożniki AP, progi zniszczenia i rozmiary ubytków wymagają jawnego
oznaczenia jako parametry gry; źródła nie certyfikują obecnych presetów.

## Kontrakt wdrożonej poprawki

Poniższe zasady opisują bieżącą implementację. Tabela odbioru rozróżnia
wyniki rzeczywistych strzałów, testy grafu i sprawdzenia nadal otwarte.

| Obszar | Wymagane zachowanie |
|---|---|
| Trafienie | Pocisk przecina cały budynek w kolejności od najbliższej powierzchni; kierunki dodatnie i ujemne działają równoważnie. |
| Penetracja | Rzeczywista droga w materiale zużywa energię tego samego pocisku; dalsze ściany otrzymują energię resztkową. Zdarzenie raportuje cały kontakt. |
| Otwór i wnęka | Widoczna geometria, model balistyczny i kolizja fizyczna opisują ten sam ubytek. Metal i drewno mają próg lokalnej wnęki zależny od `strength` i pracy normalnej. Kruchy materiał tworzy krater, a pełna perforacja wymaga przejścia pocisku. |
| Sekcje początkowe | Większe ściany, stropy i połacie są dzielone przed ostrzałem: docelowo około 1,6 m, do 16 sekcji komponentu, z budżetem do 1024 dodatkowych sekcji celu. Ograniczenia mogą pozostawić większe sekcje. |
| Podział | Domyślny limit `cutDepth` wynosi `2`. Pełne rozbicie daje do trzech fragmentów. Lokalna wnęka zostawia do czterech części i nie tworzy drobnych brył Rapiera. Po osiągnięciu limitu głębszy wyłom lub przebicie odłącza sekcję; płytkie wgniecenie może pozostać efektem, nadal zwiększając uszkodzenie. |
| Gruz końcowy | `terminalDebris` oznacza fragment, który nie podlega kolejnemu podziałowi. Dalsze zniszczenie może go usunąć. |
| Podparcie | Kierunkowy graf uwzględnia podpory od dołu i ograniczoną rozpiętość boczną. Pionowe połączenie zachowuje zużyty dystans (`carry`). Ściany nie wiszą na dachu, a wypełnienia nie podtrzymują stropów. Usunięcie podpór odłącza elementy bez dopuszczalnej drogi do fundamentu. |
| Usunięcie | Spójne wycofanie fragmentu usuwa jego model, wpisy śledzenia trafień, collider i widoczną geometrię; zadanie w kolejce nie może go odtworzyć. |
| Wykończenia | Detale wizualne mają właściciela wśród sekcji. Usunięcie sekcji usuwa też przypisane wykończenia. Jest to przybliżenie, bez osobnej symulacji każdego detalu. |
| Budżet fizyki | Maksymalnie 128 nieśpiących dynamicznych brył i 512 zachowanych brył gruzu. Uśpione pozostają dynamiczne, aby reagować na utratę podpory. Gdy nie ma miejsca, nowy gruz jest pomijany, a sekcja usuwana z obrazu, balistyki i kolizji. Nadmiar brył obudzonych przez kontakt także jest usuwany. Nie powstaje kolejka gruzu zamrożonego w powietrzu. |
| Efekty | Osobne pule: 16 błysków, 128 chmur, 256 odprysków, 128 śladów; w niskiej jakości 4/40/80/32. Aktualizacja raz na aktywną klatkę nie czeka na tick balistyki. Brak dodatkowych świateł, cieni i brył fizyki. |
| Zasoby | Wygenerowane przez demo siatki i materiały są zwalniane; współdzielone zasoby źródłowego budynku pozostają własnością generatora. |

Limit dwóch cięć jest decyzją implementacyjną, a nie wnioskiem z publikacji.
Należy mierzyć także liczbę wynikowych części: sam limit głębokości nie jest
automatycznie limitem liczby colliderów, połączeń ani wywołań renderowania.
Stałe pule są potrzebne nawet przy ograniczonym podziale.

Końcowa integracja dodaje również budżet części całego budynku:
`max(N+1, min(2048, max(256, 3N)))`, gdzie `N` to liczba początkowych sekcji.
Osiągnięcie budżetu uruchamia odłączenie trafionej sekcji. Wcześniejszy pomiar
wykazał, że sam limit dwóch poziomów cięcia pozwalał małemu domowi urosnąć do
ponad 1500 części po serii MG. Budżet zależny od rozmiaru celu ogranicza ten
koszt; nie jest parametrem materiałowym ani ustawieniem jakości efektów.

Po osiągnięciu limitów symulacja potrzebuje jednoznacznego uproszczenia.
Wycofanie gruzu z modelu usuwa również jego przyszłą zdolność zatrzymywania
pocisków. Jest to ograniczenie gry, które powinno być deterministyczne i
niezależne od ustawienia jakości cząstek. Nie należy obiecywać zachowania
całego materiału budynku przy usuwaniu gruzu z ograniczonej puli.

Graf rozróżnia fundament, element nośny, wypełnienie, strop i dach. Szczególny
przypadek `suspendedFrame` opisuje górne pozostałości słupków wyciętych przez
generator wokół otworu: mogą wisieć przy nadprożu lub dachu, ale nie są jego
podporą. To nadal reguły gry. Graf nie oblicza, jaki ciężar utrzyma pozostały
wąski fragment ściany ani czy połączenie zerwie się od momentu zginającego.
Nominalne zasięgi boczne wynoszą 4 m dla dachu, 3 m dla stropu i 1,2 m dla
pozostałych elementów. Początkowa najkrótsza droga wygenerowanego projektu
może podnieść lokalny limit o wymagany dystans z zapasem 0,25 m. To zachowuje
nienaruszony projekt, jednocześnie ograniczając dalsze łańcuchy podparcia.

## Ograniczenie kosztu obliczeń

Wcześniej każde śledzenie krótkiego odcinka pocisku przeglądało wszystkie
części celu, aby znaleźć ruchome bryły. Indeks BVH przechowuje teraz granice
obejmujące ruch liniowy przez cały krok `1/60 s`, również dla odłączonego
gruzu. Zwykłe podkroki balistyki korzystają z indeksu. Zapytanie z większym
`dt` dodatkowo sprawdza listę części ruchomych. Aktualizacja pozycji lub
prędkości wymaga przebudowania indeksu; synchronizacja Rapier wykonuje tę
operację, a po uśpieniu zeruje nieaktualną prędkość.

Osłona wybuchu jest wyznaczana przed zadaniem obrażeń. Następnie przygotowane
obrażenia są stosowane w grupie, z jednym przebudowaniem indeksu przed
śledzeniem odłamków. Podział na trzy fragmenty również grupuje zmiany.
Wykrywanie utraty podparcia sprawdza kolejne krótsze drogi przez graf,
z uwzględnieniem kosztu i limitu rozpiętości, po czym przebudowuje indeks
po całej kaskadzie. Zabezpieczenie przed ponownym wejściem obsługuje usuwanie
gruzu przez fizykę w trakcie tej kaskady.

Odłamki balistyczne zachowują swój typ także przy rykoszecie. Dzięki temu
drobny odprysk nie uruchamia kosztownej ścieżki lokalnego kraterowania
przeznaczonej dla głównego pocisku. Jego obrażenia nadal się kumulują i mogą
rozbić osłabioną sekcję. Próg wnęki i jej rozmiary są udokumentowane w
[parametrach modelu](data-sources.md); nie zmieniono profili amunicji.

Adapter rozkłada tworzenie siatek na zadania i grupuje synchronizację
colliderów. Te zmiany ograniczają zbędną pracę; obliczenia Rapier, aktualizacja
geometrii ruchomego gruzu i koszt samego wybuchu nadal obciążają klatkę.
Submilimetrowe pozostałości po cięciu są sprawdzane również przez natywny
konstruktor otoczki Rapiera. Jeśli nie da się ich reprezentować, znikają
spójnie z fizyki, modelu i obrazu; `unrepresentableHulls` zlicza ten przypadek.
Samo istnienie opisu collidera po stronie JavaScript nie gwarantowało wcześniej
poprawnej otoczki po stronie WASM.
Końcowy pomiar całego przebiegu CPU znajduje się w
[benchmark.json](benchmark.json), a jego zakres opisuje [README](README.md).
Nie należy porównywać tych wyników bezpośrednio ze starym benchmarkiem
samych paneli, który nie uruchamiał kompletnego adaptera budynku.

## Kryteria odbioru

W integracji użyto kompletnych wygenerowanych budynków, rzeczywistego
adaptera, Three oraz lokalnego Rapier. Zaliczone zestawy obejmują 33 sprawdzenia
[rdzenia](../../tests/destruction_core.cjs), 14 sprawdzeń
[adapterów i fizyki](../../tests/building_destruction_v6.cjs) oraz testów
materializacji pełnych budynków v6
[efektów](../../tests/destruction_effects.cjs).
Testy grafu z bezpośrednim usunięciem podpór mają inny zakres niż ostrzał.

Benchmark ostrzału pełnego domu zapisuje potwierdzone trafienia, liczbę
detonacji w budynku i dodatkowych eksplozji gruntu, resety celu oraz części
konstrukcji pozostałe ponad fundamentem. Bieżące wyniki i konfigurację należy
odczytać z [raportu JSON](benchmark.json). Zmiany kształtu wnęki, reguł
podparcia i uśpienia gruzu zmieniają także obciążenie; wcześniejsze liczby
nie opisują nowego wariantu. Maksimum kroku nadal jest istotne, nawet przy
niskiej średniej i p95.

Koszt emisji i aktualizacji efektów ma [osobny raport CPU](effects-benchmark.json)
odtwarzany przez `node tools/destruction/effects-benchmark.cjs`. Scenariusz
obejmuje 300 wybuchów i 600 trafień w stal w 30 sekund symulacji, w obu
jakościach. Nie mierzy rasteryzacji, GPU, fizyki ani dźwięku.

| Przypadek | Co sprawdzić | Wynik po integracji |
|---|---|---|
| AP z kamery demo | Strzał w stronę `-Z` do kompletnego budynku trafia fasadę i dalsze warstwy. | Zaliczone dla czterech wzorców w integracji. W renderowanym demo strzał przez rzeczywiste wejście UI przebił trzy części budynku. |
| Energia i kierunek | Materiał i grubość zużywają energię; sprawdzić przeciwne kierunki i trafienie ukośne. | Rdzeń sprawdza trzy warstwy przy `+Z` i `-Z` oraz pełny bilans kontaktu. Ukośny strzał w przeglądarce dał kolejno około 216,1, 212,1 i 210,2 kJ energii resztkowej. |
| Otwór po AP | Ubytek istnieje w materiale, widocznej siatce i colliderach. | Wszystkie cztery wzorce przeszły kontrolę promieniem w modelu, Three i Rapier. W przeglądarce promień po torze AP nie przeciął widocznej fasady; obejrzano zbliżenie otworu. Test przejścia kontrolera przez dostatecznie duży wyłom jest osobnym testem z bezpośrednim zadaniem ubytku. |
| Zniszczenie ściany | Kolejne trafienia mogą usunąć całą sekcję, również przy ograniczonym podziale. | Wybrana sekcja fasady zniknęła po 13 celowanych trafieniach AP w jej pozostały materiał. Pojedynczy 105 HE również usunął trafioną sekcję. Liczba 13 dotyczy tej sceny i sposobu celowania, nie każdej ściany. |
| HE i zawalenie | Rzeczywista detonacja niszczy budynek; utrata podpór odłącza elementy wyższe. | Integracja i przeglądarka potwierdzają detonację 105 HE, usunięcie ściany i powstanie gruzu. Osłonę sprawdza rdzeń. Cztery wzorce przechodzą test kierunkowego grafu po bezpośrednim usunięciu podstaw nośnych; ten test nie jest wyburzeniem całego budynku ostrzałem. |
| Długi ostrzał | Zasoby pozostają ograniczone, a limity nie unieśmiertelniają sekcji. | Rdzeń sprawdza limit cięcia, końcowy gruz i wyczerpanie budżetu geometrii; adapter sprawdza pulę brył i usuwanie zasobów. Długie serie na pełnym budynku i ich maksima raportuje [benchmark](benchmark.json). |
| Reset i wyjście | Reset przywraca cel, wyjście zamyka sesję, źródło pozostaje nienaruszone. | Integracja przywraca oryginalną sekcję w nowej kopii. Próba przeglądarkowa wykonuje reset AP→HE, zamyka demo i potwierdza niezmieniony manifest źródła oraz jeden canvas edytora po wyjściu. |
| Szczyt kosztu CPU | Średnia, p95, maksimum i zaległości obejmują balistykę, adapter i fizykę. | [Benchmark pełnego celu](benchmark.json) mierzy cały ten przebieg bez WebGL. Wyniki nie są pomiarem czasu renderowanej klatki ani gwarancją 60 FPS. |
| Czytelność efektów | Wybuch ma krótki błysk, materiałowy pył oraz odpryski; ślady znikają wraz z sekcją. | Sześć testów sprawdza ograniczone pule, czas życia, materiał, deterministyczność i zasoby. W Chrome skompilowano shadery oraz obejrzano błysk i pył HE; sprawdzony wybuch korzystał z trzech aktywnych pul renderowania. |
| Odbiór w przeglądarce | Widoczny otwór, usunięta ściana, gruz i poprawna obsługa UI. | Automatyczna próba z renderowaniem w Chrome 153 przez `file://`, 1920×1080, zakończyła się bez błędów strony. Obejrzano zrzuty AP i HE. Ręczny odbiór sterowania, chodzenia przez wyłom, pełnego wyburzenia i płynności na docelowym GPU nadal pozostaje otwarty. |

Próba przeglądarkowa jest odtwarzalna przez
[browser-check.cjs](../../tools/destruction/browser-check.cjs). Zapisano
[raport](artifacts/browser-check.json), [zbliżenie AP](artifacts/building-ap-detail.png)
i [stan po HE](artifacts/building-he.png), a także
[błysk](artifacts/building-he-flash.png), [pył](artifacts/building-he-dust.png)
i [wgniecenie stali](artifacts/building-steel-dent.png). Trzy dodatkowe cykle
wejście/reset/wyjście przywróciły 256 geometrii i jedną teksturę edytora.
Test uruchamia Building Lab z
`file://`, lokalny Rapier/WASM oraz przypięte skrypty Three kierowane przez
narzędzie testowe do lokalnych plików. Korzysta z Chrome w trybie headless;
nie zastępuje pomiaru wydajności na sprzęcie użytkownika.

Testy bez WebGL mogą sprawdzić strukturę siatki i istnienie collidera, ale nie
potwierdzają czytelności otworu na ekranie ani jakości animacji. Cel 60 FPS
oznacza około 16,7 ms na całą klatkę aplikacji; pomiar samego rdzenia
balistycznego nie dowodzi spełnienia tego celu.

Pozostają jawne uproszczenia: geometria zastępcza komponentów, graf kontaktów
bez solvera naprężeń, ograniczona pula gruzu i kalibracja materiałów jako
parametrów gry. Symulacja walki i destrukcji na całej mapie pozostaje osobnym
zakresem.
