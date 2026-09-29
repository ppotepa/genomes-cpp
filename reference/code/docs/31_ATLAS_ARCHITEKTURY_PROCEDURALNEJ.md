# Building Lab — atlas architektury i reguł generowania

Data: 25 września 2026. Dokument uzupełnia [research metod i obecnego kodu](30_RESEARCH_PROCEDURALNYCH_BUDYNKOW.md) oraz [bibliografię](32_ZRODLA_BUILDING_LAB.md).

**Status:** poniższy katalog jest autorską propozycją podziału problemu dla gry. Opisy źródłowych przykładów są oznaczone i podlinkowane. Katalog nie oznacza, że wszystkie kombinacje występują w każdej epoce i regionie ani że którakolwiek z nowych rodzin została już zaimplementowana.

## 1. Słownik: co dokładnie losujemy

| Pojęcie | Znaczenie dla generatora | Przykład |
|---|---|---|
| Funkcja | Sposób użytkowania | Mieszkanie, handel, produkcja, edukacja |
| Typologia | Organizacja funkcji i dostępu | Kamienica z oficynami, blok klatkowy, hala |
| Morfologia | Układ brył w przestrzeni | L, U, zamknięty dziedziniec, wieża na podstawie |
| Konstrukcja | System podtrzymywania elementów | Mur nośny, rama stalowa, szkielet żelbetowy |
| Styl | Zasady kompozycji i detalu | Oszczędna elewacja modernistyczna, zdobiona historyczna |
| Wykończenie | Widoczna warstwa | Tynk, cegła licowa, okładzina kamienna |
| Pokrycie dachu | Materiał na połaci | Dachówka, łupek, blacha, membrana |
| Historia | Kolejność zmian | Dobudowa oficyny, remont dachu, wymiana okien |

Kamienica może być ceglana i otynkowana jednocześnie. Blok może mieć elewację tynkowaną, choć jego konstrukcja jest prefabrykowana. Budynek o rzucie U nie musi być kamienicą. Dlatego przyszłe menu powinno rozdzielać typ, bryłę, konstrukcję i wygląd.

Terminologia przydatna w danych: **kondygnacja** obejmuje również parter; **trakt** to pas pomieszczeń w głębokości budynku; **oś elewacji** porządkuje otwory; **przęsło** odnosi się do podziału konstrukcji; **oficyna** jest skrzydłem zabudowy przy podwórzu; **ryzalit** wysuwa część bryły na większej wysokości; **wykusz** jest wystającą częścią pomieszczenia; **loggia** jest przestrzenią cofniętą w obręb budynku. W modelu nie powinny być jednym typem dekoracyjnego pudełka.

## 2. Katalog 24 rodzin

To rodziny proceduralne dobrane tak, by wymagały różnych reguł. Nie są rozłącznym podziałem całej architektury. Wariant kamienicy U jest osobną rodziną generacji, chociaż nadal należy do tej samej szerokiej typologii co kamienica frontowa.

| ID / rodzina | Bryła i parcela | Organizacja dostępu | Zestaw cech do opracowania | Pułapka generatora |
|---|---|---|---|---|
| 01. Dom zrębowy | Zwarty prostokąt, opcjonalna sień lub ganek | Jedno główne wejście, prosty układ izb | Poziome bale, narożne połączenia, dach spadzisty, mało otworów | Deski na elewacji nie zastępują geometrii zrębu |
| 02. Dom szkieletowy drewniany | Prostokąt albo lekka rozbudowa L | Wejście, strefa wspólna, pokoje | Szkielet, wypełnienie lub szalunek, rytm konstrukcji | Losowe belki przecinające drzwi i okna |
| 03. Dom murowany wiejski | Prosta bryła i ewentualna dobudówka | Wejście od drogi lub podwórza | Mur, tynk albo kamień, okap, komin, cokół | Nadmiernie wielkie otwory bez reguły podparcia |
| 04. Willa z ryzalitem | Wolnostojąca, różne wysunięcia i wysokości | Wyraźne wejście, ogród, reprezentacyjna część | Taras, ryzalit, balkon, złożony dach | Każdy dodatek losowany bez odniesienia do wnętrza |
| 05. Segment szeregowy | Wąskie parcele, wspólne boki | Osobne wejścia poszczególnych segmentów | Powtarzalny przekrój, elewacja frontowa i ogrodowa | Okna w ścianie stykającej się z sąsiadem |
| 06. Mały dom miejski z usługą | Zwarty front, zaplecze na działce | Sklep i niezależne dojście do mieszkania | Inny parter, szyld, podwórze gospodarcze | Mieszkańcy przechodzą wyłącznie przez sklep |
| 07. Kamienica frontowa | Budynek w pierzei, tył przy podwórzu | Brama lub sień, klatka, lokale | Hierarchia parter–piętra–zwieńczenie | Wszystkie cztery fasady jednakowo reprezentacyjne |
| 08. Kamienica z jedną oficyną | Front plus skrzydło L | Przejście z ulicy i obsługa skrzydła | Różne głębokości traktów, styk dachów | Dwa prostopadłościany z podwójną ścianą na styku |
| 09. Kamienica z dwiema oficynami | Układ U wokół podwórza | Główna i ewentualne dodatkowe klatki | Niższe lub skromniejsze oficyny, brama, dachy skrzydeł | Zablokowany dostęp do podwórza lub pozorne okna do sąsiedniej ściany |
| 10. Kamienica narożna | Skrzydła przy dwóch ulicach, narożnik prosty lub ścięty | Jedno lub kilka wejść | Dwie uliczne elewacje, rozwiązany narożnik, opcjonalny wykusz | Rytmy obu fasad zderzają się w narożniku |
| 11. Zespół z zamkniętym dziedzińcem | Pierścień lub kilka skrzydeł O | Brama, kilka rdzeni, połączenia podwórzowe | Zewnętrzna i wewnętrzna elewacja, dachy wokół pustki | Dziedziniec bez przejścia albo potraktowany jak pełny strop |
| 12. Blok klatkowy | Długi budynek z powtarzanych sekcji | Kilka wejść i klatek obsługujących mieszkania | Piony okien, loggie/balkony, moduły konstrukcji | Jeden korytarz przez wszystkie mieszkania zamiast sekcji |
| 13. Blok korytarzowy | Wydłużony korpus | Korytarz i rdzenie, lokale po jednej lub obu stronach | Regularne drzwi mieszkań, różne role fasad | Korytarz bez końcowego połączenia i zbyt głębokie lokale |
| 14. Galeriowiec | Liniowy albo łamany korpus | Zewnętrzna galeria, piony komunikacyjne | Fasada wejściowa inna od prywatnej, ciągła balustrada | Galeria jako dekoracja bez drzwi do mieszkań |
| 15. Punktowiec | Zwarta bryła wolnostojąca | Rdzeń centralny lub przesunięty | Mieszkania wokół rdzenia, kilka ekspozycji | Duże ślepe wnętrze i przypadkowy układ pionów |
| 16. Wieża z niższą podstawą | Wieża nad szerszym podium | Wejścia i rdzenie łączące obie części | Inny parter, taras na podstawie, dach techniczny | Wieża nie ma logicznego podparcia ani dojścia |
| 17. Współczesny zespół dziedzińcowy | Otwarty/pełny kwartał, uskoki ostatnich pięter | Kilka rdzeni i wejść od ulicy lub dziedzińca | Loggie, tarasy, dylatacje, różne wysokości | Jeden monolityczny budynek bez podziału na obsługiwane części |
| 18. Fabryka ceglana wielokondygnacyjna | Długi korpus, ewentualny aneks klatki | Komunikacja pracownicza i transportowa | Przęsła, duże okna, czytelne stropy | Skala domu mieszkalnego ukryta pod cegłą |
| 19. Hala z dachem szedowym | Powtarzane nawy lub pasma | Duże wejścia i oddzielny dostęp ludzi | Rytm ram, zęby dachu, pasma doświetlenia | Każdy „ząb” o losowym kierunku i nachyleniu |
| 20. Magazyn ramowy | Duża prosta bryła i zaplecze | Bramy, rampa, wejście personelu | Rama, panele, obróbki, opcjonalne świetliki | Otwór bramy przecinający główny słup |
| 21. Stodoła | Wydłużona przestrzeń gospodarcza | Wrota i układ przejazdu zależny od odmiany | Przęsła konstrukcji, wentylacja, znaczny dach | Mieszkalne okna i klatka schodowa dodane automatycznie |
| 22. Zabudowania zagrody | Kilka osobnych obiektów przy podwórzu | Ścieżki, gospodarczy plac, osobne wejścia | Dom, stodoła, szopa, różne fazy budowy | Losowe rozsianie bez wspólnej obsługi podwórza |
| 23. Szkoła lub budynek publiczny | Korpus z jednym lub kilkoma skrzydłami | Hol, korytarze, kilka pionów komunikacji | Powtarzalne sale, większe okna, wyraźne wejście | Powiększony dom bez programu dużych pomieszczeń |
| 24. Pawilon handlowy | Niska bryła, ekspozycja ku dojściu | Wejście klientów i dostawy | Witryny, osłona wejścia, szyldy, zaplecze | Całe ściany ze szkła także w strefie magazynowej |

Każda rodzina potrzebuje własnej karty referencyjnej: miejsce, epoka, przykładowy rzut, przekrój, zdjęcia kilku stron, zasady dopuszczalnej zmiany. Podana tabela jest szkieletem takiej biblioteki. Nie zastępuje materiałów historycznych dla 24 rodzin.

## 3. Kamienice: szczegółowy model różnorodności

### 3.1. Dwa źródłowe przykłady

**Przykład źródłowy A — Rynek Trybunalski 7 w Piotrkowie.** Opis NID wskazuje układ U, dwutraktowy korpus frontowy i jednotraktowe oficyny. Klatka łączy się z sienią przejściową. Front ma dach dwuspadowy, oficyny pulpitowe, a wykończenie elewacji frontowej i podwórzowej różni się. To konkretny przykład powiązania rzutu, komunikacji i dachu; nie uniwersalny schemat wszystkich kamienic. [Karta NID](https://zabytek.pl/pl/obiekty/g-223961).

**Przykład źródłowy B — kamienica z oficynami w Szczecinie.** Opis NID wyróżnia trzy klatki: reprezentacyjną w korpusie frontowym i dwie obsługujące oficyny. Opisuje również układ bram i przejść do podwórza. To argument za obsługą wielu rdzeni w jednym zespole, bez kopiowania jego dekoracji do każdej kamienicy. [Karta NID](https://zabytek.pl/pl/obiekty/szczecin-kamienica-23062).

### 3.2. Proponowane reguły rodziny

Geny topologii: szerokość parceli, głębokość frontu, liczba oficyn, rozmiar pustki podwórzowej, liczba klatek, dostęp do zaplecza. Geny kompozycji: osie, wyróżniony środek lub narożnik, parter usługowy, poziome zwieńczenia. Geny historii: nadbudowane piętro, dobudowane skrzydło, wtórny sklep, wymieniona stolarka.

Parter, kondygnacje mieszkalne, poddasze i piwnica nie muszą mieć tej samej wysokości. Elewacje dzielimy według znaczenia: frontowa, boczna uliczna, podwórzowa, wspólna graniczna. Oficyna nie dziedziczy automatycznie wszystkich balkonów i gzymsów frontu.

Przejście bramne jest przestrzenią pod budynkiem: trzeba odjąć je od parteru, zachować strop nad nim i połączyć ulicę z podwórzem. Brama nie jest większymi drzwiami w ścianie, jeśli zaraz za nimi znajduje się zamknięty pokój.

### 3.3. Kontrolowana nieregularność

- Skrzydło młodsze może mieć inny dach lub wysokość; styk wymaga obróbki i rozwiązania ściany.
- Inne okna mogą należeć do całego mieszkania, lokalu usługowego albo etapu remontu.
- Dobudowana klatka może zmienić tył budynku, zachowując dawną fasadę.
- Zamurowany otwór może pozostawić nadproże i inny materiał wypełnienia.
- Ubytek sąsiedniego budynku odsłania ścianę graniczną; nie powoduje automatycznie pojawienia się regularnych okien.

## 4. Bloki: system zamiast powiększonej kamienicy

**Ustalenie źródłowe.** Materiał ITB publikowany przez administrację opisuje identyfikację systemu konstrukcyjnego, połączenia elementów i rozróżnienie warstw w ścianach prefabrykowanych. Do generatora przenosimy zasadę, że panel, połączenie i wykończenie są różnymi rzeczami. Dokument nie dostarcza gotowego parametrycznego modelu wszystkich systemów wielkopłytowych. [Raport ITB / Budowlane ABC](https://budowlaneabc.gov.pl/budownictwo-wielkoplytowe-raport-o-stanie-technicznym/opis-wykonanych-badan-budynkow-z-wielkiej-plyty-w-ramach-badan-itb-pt-ocena-bezpieczenstwa-i-trwalosc-budynkow-wykonanych-metodami-uprzemyslowionymi/).

### 4.1. Cztery różne organizacje

| Model | Reguła podstawowa | Co dziedziczą kondygnacje | Dodatkowa decyzja |
|---|---|---|---|
| Klatkowy | Sekcja = klatka + przypisane mieszkania | Rdzeń, piony, podstawowe osie | Jak łączą się sekcje i gdzie są wejścia |
| Korytarzowy | Rdzenie połączone korytarzem | Przebieg komunikacji i granice lokali | Jednostronna lub dwustronna obsługa |
| Galeriowy | Zewnętrzna komunikacja wzdłuż lokali | Galeria, drzwi, rdzenie | Ochrona wejść i charakter przeciwnej fasady |
| Punktowy | Lokale otaczają zwarty rdzeń | Położenie rdzenia i pionów | Kształt obwodu i dostęp lokali do fasady |

**Propozycja genomu:** liczba sekcji, typ sekcji, liczba kondygnacji, moduł przęseł, układ mieszkań, rodzina balkonów/loggii, stan termomodernizacji. Kolorystyka remontu może przykrywać dawny podział prefabrykatów, więc nie należy zawsze rysować spoin na wierzchu nowego tynku.

### 4.2. Zależności widoczne na elewacji

Okno klatki może znajdować się pomiędzy poziomami okien mieszkań. Loggia zajmuje głębokość bryły i ma strop, ściany boczne oraz balustradę. Balkon jest wysunięty; wymaga innej geometrii i innego modelu podparcia. Rytm elementów może być powtarzalny przez kilka kondygnacji, a wyjątki odpowiadać parterowi, wejściom lub ostatniemu poziomowi.

Nie przypisujemy na tym etapie wymiarów ani nazwy konkretnego systemu prefabrykacji. Do wiernego wariantu W-70 lub OWT potrzebne byłyby właściwe katalogi elementów i rzuty referencyjne, nie tylko fotografie elewacji.

## 5. Hala, fabryka i budynki gospodarcze

### 5.1. Hala i magazyn

Proponowana kolejność: obrys → siatka słupów/ram → strefy użytkowania → bramy i dojścia → dach → panele ścienne → detale. Długość hali może wynikać z liczby przęseł, a nie ciągłego losowania każdej ściany. Brama musi zmieścić się w wybranym przęśle lub wymagać specjalnego rozwiązania ramy.

Parametry: liczba naw, liczba przęseł, typ dachu, doświetlenie, rampa, aneks biurowy, podział stref transportu i ludzi. Świetliki mogą tworzyć wspólny rytm. Widoczne stężenia należą do siatki konstrukcji; nie powinny być losowymi przekątnymi przez otwory.

### 5.2. Fabryka wielokondygnacyjna

Duże okna, powtarzalne przęsła i aneks komunikacyjny mogą nadać charakter przy prostej siatce. Najpierw trzeba zdecydować, czy pokazujemy mur z wewnętrznym szkieletem, czy inną konstrukcję. Cegła jako kolor nie odpowiada na to pytanie. Komin przemysłowy może być oddzielnym obiektem zespołu, nie standardowym kominkiem na dachu domu.

### 5.3. Stodoła i zagroda

NPS opisuje różne historyczne typy stodół i ich charakterystyczne elementy. To materiał o budownictwie amerykańskim; nie kopiujemy jego typologii jako domyślnej polskiej zagrody. [Preservation of Historic Barns](https://www.nps.gov/orgs/1739/upload/preservation-brief-20-barns.pdf).

**Propozycja dla świata:** zagroda ma najpierw plan wspólnego podwórza i relacje funkcjonalne, następnie osobne genomy domu i budynków gospodarczych. Zmienność obejmuje odległości, obrót, dobudówki i etapy budowy, ale zachowuje przejazd oraz dojścia. Dach i wrota stodoły mają większe znaczenie dla sylwety niż liczne drobne dekoracje.

## 6. Katalog rzutów i brył

| Forma | Potrzebne dane | Reguła do sprawdzenia |
|---|---|---|
| Prostokąt | Szerokość, głębokość, orientacja | Głębokość pasuje do programu |
| Prostokąt z aneksem | Bryła główna, dobudówka, styk | Usunięcie zbędnej ściany na połączeniu |
| L | Dwa skrzydła i narożnik wewnętrzny | Wspólny poziom lub jawne schody przejściowe |
| T | Korpus, skrzydło, węzeł | Nieblokowany punkt komunikacji |
| U | Front, dwa boki, podwórze | Dostęp i sensowna szerokość pustki |
| H | Dwa korpusy i łącznik | Prześwity przy łączniku, własne dachy |
| O / dziedziniec | Kontur zewnętrzny i otwór | Stropy i dach nie zasłaniają dziedzińca |
| Grzebień | Korytarz/łącznik, rytm skrzydeł | Obsługa wszystkich skrzydeł |
| Liniowy segmentowy | Lista modułów i połączeń | Rytm i różnice sekcji nie gubią wejść |
| Łamany / zygzak | Ciąg odcinków i kąty | Geometria narożników, klinowe resztki |
| Narożnik ścięty | Dwie krawędzie uliczne i ścięcie | Osie oraz dach rozwiązane wokół ścięcia |
| Stopniowany pionowo | Obrysy kolejnych kondygnacji | Tarasy, podparcie i odpływ |
| Wieża + podstawa | Dwie skale obrysu, rdzeń | Połączenie konstrukcji i komunikacji |
| Pawilony połączone | Obiekty i łączniki | Poziomy terenu oraz przechodniość |
| Wielokąt / obrys zakrzywiony | Krzywe lub segmenty, tolerancja | Kontrolowana aproksymacja i poprawne ościeża |

Nie każdy kształt jest odpowiedni dla każdego programu. Głęboki kwadratowy plan ma inny problem dostępu do fasady niż wąskie skrzydło. Dziedziniec to ważna część topologii, a nie dekoracyjny obiekt wstawiony do pełnego budynku.

## 7. Katalog 16 rodzin dachów

Poniższy podział jest roboczy dla geometrii. Pokrycie wybieramy osobno. Nazwy regionalne mogą się różnić; w danych najlepiej przechowywać regułę i kształt, a etykiety tłumaczyć w UI.

| Rodzina | Topologia i parametry | Zastosowanie do zbadania | Najważniejsza pułapka |
|---|---|---|---|
| 01. Płaski / małospadowy | Powierzchnia, spadki, attyka lub okap, odpływy | Bloki, pawilony, hale | „Płaski” potraktowany jako brak odwodnienia |
| 02. Pulpitowy | Jedna połać i wysoka ściana | Oficyna, aneks, hala | Otwarta szczelina pod górną krawędzią |
| 03. Dwuspadowy | Dwie połacie, kalenica, dwa szczyty | Domy, kamienice, budynki gospodarcze | Brak ścian szczytowych |
| 04. Dwuspadowy asymetryczny | Różne rozpiętości lub nachylenia | Dobudowy i wybrane domy | Mylenie różnej długości z przypadkowym przesunięciem płyt |
| 05. Kopertowy | Połacie boczne i czołowe, wspólna kalenica | Willa, dom, pawilon | Prostokątne płyty zachodzą zamiast wspólnych naroży |
| 06. Namiotowy | Połacie zbiegające się do wierzchołka | Zwarta bryła, pawilon, wieżyczka | Sztucznie krótka kalenica zamiast właściwej topologii |
| 07. Naczółkowy | Dwuspadowy ze ściętymi górnymi partiami szczytów | Wybrane odmiany regionalne | Niewłaściwe domknięcie pozostałej ściany szczytowej |
| 08. Mansardowy | Załamanie połaci, stromy dół, łagodniejsza góra | Poddasza i architektura historyczna | Jeden kąt dachu bez rzeczywistego załamania |
| 09. Dwuspadowy łamany / gambrel | Dwie pochyłości na każdej głównej stronie | Wybrane domy i stodoły, zależnie od regionu | Automatyczne mieszanie z każdą mansardą |
| 10. Wielospadowy złożony | Połączone dachy skrzydeł, kosze i naroża | L, T, U, rozbudowane wille | Nierozwiązane styki i niezamknięte dziury |
| 11. Szedowy | Powtarzane segmenty z doświetleniem | Hale | Losowy rytm zamiast jednej reguły naw |
| 12. Motylkowy | Połacie opadające do wewnętrznej doliny | Wybrane nowoczesne pawilony | Brak obsługi odpływu w najniższym pasie |
| 13. Kolebkowy | Wyciągnięty przekrój krzywoliniowy | Hale, magazyny, obiekty specjalne | Zbyt wiele segmentów bez korzyści w sylwecie |
| 14. Kopuła | Powierzchnia obrotowa lub segmentowa | Obiekty specjalne | Użycie przypadkowo na dowolnym budynku |
| 15. Stożkowy / wieloboczny hełm | Dach wieżyczki o określonym obrysie | Narożny akcent, wieża | Pokrycie bez dostosowania do zwężania połaci |
| 16. Tarasowy / użytkowy | Dach z funkcją, dojściem i zabezpieczeniem krawędzi | Niższa podstawa, dach dostępny | Sam mebel na dachu bez wejścia i balustrady |

### 7.1. Pokrycia: osobny katalog

| Pokrycie | Co powinno być rozpoznawalne | Jak zachować prostą geometrię |
|---|---|---|
| Dachówka płaska | Rzędy, zakład, kształt dolnej krawędzi | Wzór powierzchni plus geometria okapu i kalenicy |
| Dachówka profilowana | Kierunek fal/profili i rytm rzędów | Detal normalnej, wybrane krawędzie jako mesh |
| Łupek | Cienkie płytki, rzędy, kontrolowana różnica odcieni | Proceduralny wzór o stałej skali |
| Gont drewniany | Zakład i długości elementów | Rozdział większych plam i drobnych szczelin |
| Strzecha | Grubość warstwy, miękki okap i kalenica | Sylweta i gęsty wzór, bez pojedynczych źdźbeł |
| Blacha na rąbek | Ciągłe pasy i podniesione łączenia | Nieliczne profile lub mapa normalnych |
| Blacha falista / trapezowa | Kierunkowa powtarzalna geometria | Uproszczona połać z profilem na brzegu |
| Membrana / papa | Spoiny, obróbki, odpływy | Prawie płaska siatka, detale w miejscach styku |
| Żwir / warstwa balastowa | Drobna ziarnistość i obrzeża | Materiał, nie tysiące kamieni na każdym dachu |
| Dach zielony | Strefy roślinności, obrzeża, dojście serwisowe | Wspólny materiał i ograniczone kępy w bliskim LOD |

Źródła NPS o łupku i dachówkach opisują zróżnicowanie elementów oraz znaczenie wykończeń przy kalenicy, narożach i koszach. To potwierdza rozdzielenie powierzchni pola dachu od jego krawędzi. Szczegóły regionalne i wymagane nachylenia muszą być dobrane do konkretnego pokrycia, nie do jednej globalnej wartości. [Slate Roofs](https://www.nps.gov/orgs/1739/upload/preservation-brief-29-slate-roofs.pdf), [Clay Tile Roofs](https://www.nps.gov/orgs/1739/upload/preservation-brief-30-clay-tile-roofs.pdf).

### 7.2. Detale dachowe i ich zależności

Komin: powiązanie z pionem lub funkcją, przecięcie połaci, obróbka, zakończenie. Lukarna: związek z poddaszem, własny dach, policzki i styk z połacią główną. Okno połaciowe: otwór w konkretnej połaci i obróbka. Rynna: krawędź zbierająca wodę, narożniki, rura spustowa. Attyka: grubość, naroża i nakrycie. Wywietrznik: funkcja i wolna przestrzeń, bez kolizji z lukarną.

Antena, instalacja fotowoltaiczna i urządzenia techniczne zależą od epoki oraz przeznaczenia. Nie są uniwersalnym sposobem „dodania detalu”. Zielony dach, śnieg i zabrudzenie są warstwami stanu, a nie zamiennikami konstrukcji.

## 8. Konstrukcje i materiały ścian

### 8.1. Siedem modeli konstrukcji do rozróżnienia

| Model | Co organizuje budynek | Jak wpływa na generację |
|---|---|---|
| Mur nośny | Ciągi ścian i podparcie stropów | Otwory, filary i przekrycia muszą być rozpatrywane razem |
| Zrąb drewniany | Warstwy bali i naroża | Otwory nie mogą ignorować połączeń i zakończeń |
| Szkielet drewniany | Słupy, rygle, stężenia | Wypełnienia i stolarka odnoszą się do ram |
| Szkielet żelbetowy | Słupy, belki/płyty, rdzenie | Większa niezależność podziałów fasady i wypełnień |
| Ściany i płyty żelbetowe | Powtarzalne płaszczyzny nośne | Piony i podziały kondygnacji tworzą wspólny system |
| Prefabrykacja panelowa | Elementy, styki, sekcje | Biblioteka modułów i zgodności połączeń |
| Rama stalowa | Przęsła, słupy, rygle/kratownice | Rytm hal, bram i pokryć podporządkowany konstrukcji |

To modele do uproszczonego generatora. Nie definiują obliczeń nośności ani maksymalnych rozpiętości. Mieszanie systemów wymaga oznaczonej strefy przejściowej, np. przebudowanego parteru lub nowszego aneksu.

### 8.2. Widoczne materiały

| Rodzina | Cechy wizualne do zachowania | Reguła proceduralna |
|---|---|---|
| Cegła licowa | Moduł, spoiny, wiązanie, narożniki | Wzór w metrach, wspólny przebieg warstw |
| Cegła malowana | Relief cegły pod powłoką | Farba zmienia powierzchnię, nie geometrię muru |
| Kamień nieregularny | Różne kamienie i spoiny | Rozkład wielkości, zamknięte styki, wyraźne obramienia otworów |
| Kamień ciosany | Regularniejsze warstwy i obróbka lica | Osobny moduł bloków, nie przeskalowana cegła |
| Tynk gładki | Duże płaszczyzny, krawędzie, delikatna faktura | Mała zmienność lokalna, większe strefy remontów |
| Tynk fakturowany | Ziarnistość i sposób obróbki | Skala detalu zależna od odległości |
| Beton monolityczny | Ślady form, podziały robocze, powierzchnia | Wzór powiązany z elementem konstrukcji |
| Beton prefabrykowany | Granice paneli i powtarzalność | Podział wynika z katalogu elementów |
| Drewniane bale | Obły lub kanciasty profil, naroża | Warstwy i zakończenia, osobna rodzina otworów |
| Deski poziome | Kierunek, zakład, narożne listwy | Ciągłość rytmu i przycięcia przy otworach |
| Deski pionowe | Pasy, listwy i styki | Moduł szerokości, zakończenia na cokole i okapie |
| Widoczny szkielet z wypełnieniem | Rama odróżniona od pola | Pola wynikają z szkieletu |
| Blacha / panel warstwowy | Kierunek profilowania, styki, narożniki | Większe moduły niż cegła, obróbki przy otworach |
| Szkło w fasadzie słupowo-ryglowej | Rytm konstrukcji fasady i odbicia | Siatka pól, rozdział szyb i pasów nieprzezroczystych |
| Płytki ceramiczne | Drobny moduł, połysk, spoiny | Przypisanie do stref, np. cokołu lub wejścia |
| Okładzina kamienna | Płyty i mocowania/styki sugerowane detalem | Inny moduł niż mur z kamienia |
| Współczesna fasada wentylowana | Podział płyt, szczeliny, głębokość warstw | Zależność od mocowania i krawędzi bryły |

NPS o historycznym betonie opisuje zróżnicowanie powierzchni i mechanizmy degradacji; inspiracją dla materiału jest konkretny sposób wykonania, nie jednolity szary kolor z szumem. [Preservation of Historic Concrete](https://www.nps.gov/orgs/1739/upload/preservation-brief-15-concrete.pdf).

### 8.3. Stropy i fundamenty też mają znaczenie wizualne

W przekroju trzeba odróżnić strop belkowy, płytę, sklepienie i pokrycie sufitu. Wszystkie mogą być renderowane oszczędnie, lecz mają inną krawędź po uszkodzeniu. Cokół wynika z poziomu posadzki, terenu i wykończenia. Budynek na spadku może mieć stopniowany cokół, odsłoniętą piwnicę albo taras terenu — nie powinien zginać wszystkich ścian według wysokości gruntu.

## 9. Okna: osie niezależnych decyzji

Nie tworzymy jednej listy, w której „duże”, „drewniane” i „łukowe” konkurują jako typy. Okno jest kombinacją formy otworu, materiału, mechanizmu i podziałów, ograniczoną przez kontekst.

| Oś genomu | Warianty do zbadania | Zależność |
|---|---|---|
| Kształt otworu | Prostokątny, pełny łuk, odcinkowy łuk, ostrołuk, koło, wielobok | Styl, funkcja, konstrukcja nad otworem |
| Proporcje | Pionowe, prawie kwadratowe, poziome, pasmowe | Pomieszczenie i kompozycja |
| Osadzenie | Cofnięte, blisko lica, wnęka, narożne | Grubość i warstwy ściany |
| Materiał ramy | Drewno, stal, aluminium, tworzywo | Epoka budowy lub remontu |
| Mechanizm | Stałe, rozwierane, uchylne, przesuwne, sash, obrotowe | Rodzina stolarki i region |
| Podział | Jedno pole, dwa skrzydła, nadświetle, wiele kwater | Wymiary i technologia |
| Dolna krawędź | Zwykły parapet, wysoki parapet, drzwi balkonowe | Funkcja i relacja z wnętrzem |
| Osłona | Okiennice, żaluzja, roleta, markiza, krata | Klimat, użytkowanie, epoka |
| Stan | Zamknięte, uchylone, otwarte, zbite, zamurowane | Historia i rozgrywka |

### 9.1. Dwanaście pakietów okiennych do przyszłej biblioteki

1. Małe drewniane okno wiejskie — prosta rama, niewiele kwater, głębsze ościeże zależne od muru.
2. Wysokie okno kamienicy — stały rytm osi, dopuszczalne nadświetle i dekoracyjne obramienie.
3. Okno skrzynkowe — osobna rodzina głębokości i podwójnego układu ram; nie tylko szerszy profil.
4. Drewniane sash — pionowy mechanizm przesuwny w odpowiednim profilu regionalnym.
5. Stalowe przemysłowe — smukła siatka i wybrane otwierane pola.
6. Okno bloku — powtarzalny moduł przypisany do mieszkania i konkretnego systemu.
7. Okno klatki — związane z podestem, może łamać poziomy fasady mieszkań.
8. Zestaw balkonowy — drzwi i okno jako wspólna kompozycja z balkonem/loggią.
9. Witryna sklepu — relacja wejścia, szyldu, cokołu i ekspozycji.
10. Pasmo okienne — większa ciągłość z kontrolowanymi słupkami i strefą stropu.
11. Okno połaciowe — położenie w układzie połaci, inna orientacja i obróbka niż w murze.
12. Okno w lukarnie lub wykuszu — element większej bryły, a nie samotna rama przed ścianą.

Dokumenty NPS o drewnianych i stalowych oknach pokazują, że profile, podziały i sposób wykonania są istotnymi cechami ich charakteru. Zachowujemy te rozróżnienia, ale nie przyjmujemy jednego amerykańskiego typu stolarki dla wszystkich regionów. [Wooden Windows](https://www.nps.gov/orgs/1739/upload/preservation-brief-09-wood-windows.pdf), [Steel Windows](https://www.nps.gov/orgs/1739/upload/preservation-brief-13-steel-windows.pdf).

### 9.2. Mały detal o dużym znaczeniu

Nawet proste okno powinno mieć czytelne ościeże, ramę, szybę osadzoną na właściwej głębokości i parapet. Dekoracyjna opaska należy do elewacji, ościeżnica do stolarki. Szprosy nie zastępują skrzydeł ani słupka konstrukcyjnego. Przy ograniczonej geometrii warto najpierw zachować te głębokości i proporcje.

## 10. Drzwi, bramy i partery

| Rodzina | Cechy | Co musi wiedzieć generator |
|---|---|---|
| Deszczułkowe gospodarcze | Deski, rygle, zawiasy | Rozmiar otworu i sposób otwierania |
| Płycinowe | Rama i pola | Proporcje, materiał, osadzenie |
| Przeszklone wejściowe | Pełne pola i szklenie | Prywatność, stylistyka, ochrona wejścia |
| Dwuskrzydłowe | Dwa skrzydła, opcjonalne nadświetle | Aktywne skrzydło i szerokość używanego przejścia |
| Brama przejazdowa | Duży otwór i droga za nim | Ciągły przejazd do podwórza |
| Brama stodoły | Szeroki otwór, możliwy przejazd przez bryłę | Relacja z konstrukcją i układem gospodarskim |
| Przemysłowe przesuwne | Skrzydło odkładające się obok otworu | Wolna ściana na prowadzenie |
| Rolowane / segmentowe | Mechanizm nad otworem | Przestrzeń mechanizmu i nadproża |
| Techniczne stalowe | Prosty podział i osprzęt | Dostęp do zaplecza, epoka |
| Wewnętrzne mieszkalne | Skala pokoju, próg lub jego brak | Połączenie konkretnych przestrzeni |

Drzwi balkonowe należą do wspólnego zespołu okiennego. Obracane drzwi i śluzy wejściowe można rozważyć później dla wybranych budynków publicznych; nie muszą wejść do pierwszego zakresu.

NPS o historycznych witrynach omawia relację ekspozycji, wejścia i górnych kondygnacji oraz późniejsze przekształcenia parterów. W naszym modelu parter usługowy powinien być osobnym programem elewacji, a jego remont lokalnym wydarzeniem. [Rehabilitating Historic Storefronts](https://www.nps.gov/orgs/1739/upload/preservation-brief-11-storefronts.pdf).

## 11. Detale elewacji według roli

| Grupa | Elementy | Reguła przypisania |
|---|---|---|
| Kontakt z gruntem | Cokół, schodki, studzienka piwniczna, podest | Poziom wejścia i ukształtowanie terenu |
| Otwory | Parapet, nadproże, opaska, zwornik | Rzeczywisty obrys otworu |
| Podziały poziome | Gzyms, pas międzykondygnacyjny, fryz | Poziomy konstrukcji i kompozycja |
| Podziały pionowe | Pilaster, lizena, narożne boniowanie | Osie i narożniki, bez kolizji z otworami |
| Przestrzenie zewnętrzne | Balkon, loggia, galeria, ganek, taras | Dostęp, strop, zabezpieczenie krawędzi |
| Rozbudowa bryły | Ryzalit, wykusz, wieżyczka, aneks klatki | Program i styk z podstawowym obrysem |
| Ochrona przed pogodą | Daszek, okap, markiza, żaluzje | Ekspozycja i rola wejścia/okna |
| Instalacje | Rura spustowa, kratka, przewód, skrzynka | Piony i funkcja, nie jednolita dekoracyjna siatka |
| Użytkowanie | Szyld, numer, tablica, donice | Wejścia, lokale, epoka i dostępność miejsca |

Balustrada powinna mieć ciągłość przy zmianie kierunku i kończyć się w logicznym miejscu. Balkon nie może zasłonić wejścia pod nim bez odpowiedniej wysokości. Gzyms powinien przejść przez narożnik albo świadomie się zakończyć. Ornament można uprościć do rytmu i profilu; nie trzeba od razu generować rzeźbionych liści.

## 12. Region, epoka i historia zmian

### 12.1. Robocze profile kontekstu

| Profil do opracowania | Preferencje do zebrania ze źródeł | Czego nie mieszać przypadkowo |
|---|---|---|
| Historyczna polska pierzeja miejska | Parcele, bramy, oficyny, tynk/cegła, stolarka | Wszystkich odmian kamienic w jeden dom „europejski” |
| Powojenne osiedle mieszkaniowe | Sekcje, rdzenie, loggie, dachy techniczne | Detali z różnych systemów prefabrykacji bez uzasadnienia |
| Wieś środkowoeuropejska | Regionalne konstrukcje, zagrody, pokrycia | Jednej generycznej chaty dla wszystkich miejsc |
| Historyczna dzielnica przemysłowa | Hale, fabryki, transport, zespoły obiektów | Skali mieszkalnej i czysto dekoracyjnych bram |
| Brytyjska zabudowa szeregowa | Rytm segmentów i lokalne odmiany | Automatycznego utożsamienia z kamienicą czynszową |
| Współczesna zabudowa mieszana | Inne partery, loggie, okładziny, instalacje | Historycznych detali dodawanych bez reguły |

Historic England opisuje zróżnicowanie georgiańskiej i wiktoriańskiej zabudowy szeregowej. Używamy tego jako osobnego regionalnego punktu odniesienia, nie zamiennej definicji polskiej kamienicy. [Conserving Georgian and Victorian Terraced Housing, 2020](https://historicengland.org.uk/images-books/publications/conserving-georgian-victorian-terraced-housing/).

### 12.2. Cztery niezależne stany budynku

**Wiek** nie jest tym samym co **utrzymanie**. **Uszkodzenia bojowe** nie są tym samym co **przebudowa**. Stary budynek może być świeżo odnowiony, nowy — zaniedbany, a odbudowany parter może różnić się od górnych pięter.

Proponowany zapis wydarzeń: `{rodzaj, czas, obszar, zakres, seed}`. Obszarem może być dach, skrzydło, parter, mieszkanie albo kilka otworów. Najpierw wydarzenie, potem jego ślady. Dzięki temu zamurowane okno, resztka opaski i odmienna faktura ściany występują razem.

## 13. Przykładowe genomy do opracowania teorii

Poniższe liczby są **syntetycznymi przykładami dla makiety**, nie statystyką budownictwa ani wymiarami normowymi. Trzeba je dopiero rozwiązać geometrycznie i sprawdzić. Nie są konfiguracją już obsługiwaną przez aktualny kod.

### A. Dom z dobudowaną sienią

Bryła główna 9 × 7 m, jedna kondygnacja i nieużytkowe poddasze, aneks 3 × 2 m. Mur wykończony tynkiem, główny dach dwuspadowy, aneks pulpitowy. Wspólna rodzina stolarki; aneks może mieć nowsze drzwi. Najpierw działająca droga od wejścia do pomieszczeń, potem okna i komin. Przykład sprawdza styk dachów oraz lokalną historię rozbudowy.

### B. Kamienica U

Parcela 24 × 34 m; front 24 × 10 m, dwie oficyny po 5 m szerokości biegnące za frontem. Pozostaje podwórze o szerokości 14 m przed uwzględnieniem wykończeń i lokalnych wysunięć. Cztery kondygnacje frontu, trzy oficyn, wyższy parter. Brama prowadzi na podwórze, rdzenie obsługują wszystkie użytkowe części. Różne wysokości wywołują konieczność rozwiązania ścian ponad dachami oficyn. Przykład bada topologię, a nie zdobienie fasady.

### C. Blok klatkowy z trzech sekcji

Trzy sekcje po 18 × 12 m, pięć kondygnacji. Każda sekcja ma własny rdzeń i wejście. Moduły elewacji wynikają z przypisanych mieszkań, a loggie tworzą wspólne piony. Wariant historii: część okien wymieniona przez mieszkańców, później wspólne ocieplenie budynku. Przykład sprawdza dziedziczenie pięter, stabilność sekcji i rozdzielenie panelu od wykończenia.

### D. Magazyn ramowy

Obrys 30 × 18 m, długość podzielona na pięć przęseł po 6 m. Wydzielony aneks personelu, bramy w wybranych polach konstrukcji, osobne wejście piesze. Dach dwuspadowy lub wariant z doświetleniem opracowany w osobnej regule. Przykład sprawdza dużą przestrzeń bez domyślnych pięter i mieszkaniowych okien.

Każdą kartę należy później przedstawić w tej samej formie: plan, przekrój, cztery elewacje, graf przejść, model szary i model z materiałami. Dopiero wtedy porównywać warianty genomów.

## 14. Priorytety prostego, wiarygodnego wyglądu

| Priorytet | Element | Dlaczego warto zacząć tutaj |
|---|---|---|
| P0 | Różne typologie i proporcje bryły | Budynek musi różnić się organizacją, nie tylko paletą |
| P0 | Poprawne dachy i styki skrzydeł | Błędy w sylwecie widać z daleka |
| P0 | Wejścia, osie i ościeża | Dają skalę i czytelność użytkowania |
| P0 | Ciągła komunikacja | Warunek wnętrza używanego przez piechotę |
| P1 | Rozróżnienie fasad i parteru | Budynek reaguje na ulicę i podwórze |
| P1 | Balkony, loggie, okapy, cokoły | Duże cienie i głębokość bez gęstego ornamentu |
| P1 | Moduł i kierunek materiału | Mniej efektu rozciągniętej tekstury lub przypadkowego szumu |
| P2 | Warianty stolarki, instalacje i szyldy | Charakter regionu oraz użytkownika |
| P2 | Lokalne remonty i starzenie | Nieregularność z czytelną przyczyną |
| P3 | Drobne profile, okucia, ornament | Potrzebne przede wszystkim w bliskim widoku |

Ten porządek jest hipotezą dla naszej gry. Powinien być oceniony na przyszłym porównaniu kilku budynków w docelowej kamerze; nie wynika z pomiaru wydajności ani z uniwersalnej punktacji realizmu.
