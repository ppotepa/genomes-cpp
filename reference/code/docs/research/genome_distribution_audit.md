# Rozkłady genomów i zgodność proporcji — audyt 2026-09-24

Pomiary, selekcja seedów i zakresy w tym dokumencie dotyczą generatora bazowego `36b3808` (`infantry-5.0.0`). Generator 0.14.0 (`infantry-7.0.0`) zmienił rozkład części genów i ich zależności; liczby nie są pomiarem nowego rozkładu. Zmiany i porównania wizualne opisuje [raport główny](../19_AUDYT_WYGLADU_PIECHOTY.md#14-wdrożenie-poprawek-i-stan-po-zmianach).

Generator: `infantry-5.0.0`. To diagnoza algorytmu i dobór próbek do oceny artystycznej, nie pomiar medycznej „normalności” ludzi. Nietypowa proporcja sama w sobie nie oznacza błędu. Problemem projektu są niezależnie składane kształty, nagłe korekty i niespójne miejsca mocowania. Nie używamy kategorii rasowych ani etnicznych do generowania lub oceniania postaci.

## Metoda i odtwarzalność

Trwała sonda: [`tools/genome_distribution_probe.cjs`](tools/genome_distribution_probe.cjs). Uruchomienie w katalogu repozytorium:

```powershell
rtk proxy node docs/research/tools/genome_distribution_probe.cjs
```

Sonda zapisuje pełne wyniki `tests/out/genome_distribution_summary.json` i `tests/out/genome_distribution_rows.json`, celowo ignorowane przez Git, oraz trwały [kompaktowy raport JSON](data/genome_distribution_summary.json). Pierwotna robocza sonda i jej wyniki pozostają w `tests/out`. Próbka obejmuje seedy **0–1999**, każdy przy variation **0, 0.5, 1, 1.25, 1.75**. Jest to 2000 tożsamości w pięciu konfiguracjach, nie 10 000 niezależnych postaci. Wyniki zawierają SHA-256 wejściowych plików.

Wczytano bieżące `Config`, `SeededRandom`, `InfantryGenome`, `FaceAnatomy`, `InfantryAnatomy`; `numeric_math.cjs` dostarcza operacje wektorowe. Nie generowano pełnej siatki ani WebGL. Porównano ponowne wygenerowanie genomu, fenotypu i poziomów przekrojów głowy: wszystkie 10 000 konfiguracji dały identyczne wyniki. Genomy i ich grupy są zamrożone. To potwierdza powtarzalność bieżącego algorytmu, nie wygląd.

CodeGraph był aktualny. `query` odnajduje pliki modułów; `callers faceAnatomy.js` nie zwraca powiązań, więc zależności obiektów przypisywanych do `window.RTS` prześledzono w kodzie. Nie zmieniono runtime ani testów produkcyjnych.

## Główny mechanizm powstawania skrajnych kombinacji

`infantryGenome.js:7` używa `varied = clamp01(0.5 + (U - 0.5) * 1.08)`. Losowanie jest prawie jednostajne, dodatkowo rozszerzone poza zakres, a następnie ucinane. Dla idealnego jednostajnego U daje to około **7.41% prawdopodobieństwa dokładnej wartości 0 lub 1 dla pojedynczego genu**. Szerokość szczęki, podbródka, policzków, nosa, ust i oczodołów to osobne losowania. Geny mimiki i asymetrii mają łagodniejszy rozkład trójkątny `centred`, ale główna geometria go nie ma.

W próbce variation=1:

- **1912/2000 = 95.6%** postaci ma przynajmniej jeden z 41 geometrycznych genów twarzy na dokładnym 0 lub 1; średnio 2.994 takich genów.
- Po dodaniu sześciu geometrycznych genów włosów: **1941/2000 = 97.05%**, średnio 3.454 skrajnego genu. Nie zaliczono koloru, stylu włosów, mimiki ani asymetrii.
- W szerszych ogonach, czyli gen <0.1 lub >0.9, mediana wynosi **12 z 47** cech twarzy/włosów. Duża liczba odległych od środka cech jednocześnie jest skutkiem wymiarowości losowania, a nie rzadkim pechem jednego seeda.

Korelacje Pearsona surowych genów są bliskie zeru: jaw–cheek **−0.008**, jaw–neck **−0.004**, eye width–spacing **−0.007**, hand scale–arm length **0.006**. To jest zgodne z niezależnym losowaniem. Fenotyp ciała ma już pewne zależności poprzez frame/mass/muscle, lecz sama głowa nie ma wspólnego modelu proporcji. Te korelacje nie opisują biologii; opisują wyłącznie generator.

Asymetria ma osobne geny `eyeHeightAsymmetryGene`, `browHeightAsymmetryGene`, `mouthCornerAsymmetryGene`, `earAsymmetryGene`. Są losowane rozkładem trójkątnym `centred`, a `signed(gene)` wyznacza różnicę między stronami. Teoretyczne maksima różnicy to odpowiednio **.0018H, .0020H, .0016H, .0019H**; położenie każdej strony otrzymuje połowę z przeciwnym znakiem. W próbie P05–P95 żądanej różnicy wynosi około ±.00125H dla oczu, ±.00135H dla brwi, ±.00108H dla kącików ust i ±.00131H dla uszu. To niewielkie stałe odchylenia tożsamości, które warto zachować, a nie zerować w celu uzyskania idealnej symetrii. `browY` po ograniczeniu może zmieniać faktyczną asymetrię. Metryka dotyczy żądanych przesunięć landmarków, nie końcowego renderu ani późniejszej animacji.

## Korekty geometrii są częścią typowego przypadku

`FaceAnatomy` poprawnie chroni przed częścią kolizji i odklejeniem szyi. Jednak obecnie często generuje najpierw niespójny zestaw, a następnie wyrównuje go twardym ograniczeniem. Jednostką delta poniżej jest H, czyli wzrost postaci. Mediana dotyczy tylko przypadków faktycznie skorygowanych.

| Korekta, variation=1 | Liczba / 2000 | Udział | Mediana delta H | Maksimum delta H |
|---|---:|---:|---:|---:|
| Linia włosów ponad brwiami | 1910 | 95.50% | 0.010839 | 0.026656 |
| Usta ponad podbródkiem | 1080 | 54.00% | 0.003580 | 0.010500 |
| Prawa brew ponad okiem | 830 | 41.50% | 0.002722 | 0.009467 |
| Lewa brew ponad okiem | 823 | 41.15% | 0.002784 | 0.009381 |
| Podstawa nosa ponad ustami | 716 | 35.80% | 0.003128 | 0.012309 |
| Poszerzenie dolnej czaszki do szyi | 629 | 31.45% | 0.004474 | 0.017324 |
| Dodatkowy clamp wysokości podbródka | 388 | 19.40% | — | — |
| Rozstaw oczu | 19 | 0.95% | 0.000174 | 0.000616 |

**Nie należy pisać, że 54% twarzy wygląda źle.** Korekta jest wskaźnikiem niezgodności żądanych parametrów i może skutecznie zapobiegać widocznemu problemowi. Istotna jest częstotliwość, wielkość oraz utrata kontroli artystycznej. Gdy ponad połowa ust kończy dokładnie na `chinY + .014`, wiele różnych wartości `mouthHeightGene` daje ten sam względny odstęp. Analogicznie 35.8% nosów ma minimalny odstęp od ust `.010H`.

Nie wszystkie ograniczenia są w `adjustments`: clamp `chinY`, powiększenie dolnej czaszki w Z, porządkowanie wysokości ringów i ograniczenie szerokości ust nie używają `record`. Sonda dodatkowo mierzy pierwszy z nich. Szerokość ust dochodzi do swojego limitu `.57` lokalnej szerokości twarzy dla seeda 865 (1/2000). Nie jest to kompletny audyt wszystkich clampów siatki.

Porównanie zwężania istniejących genów:

| Variation | Usta | Podstawa nosa | Brew L | Szyja/czaszka | Linia włosów | Clamp podbródka |
|---|---:|---:|---:|---:|---:|---:|
| 0 | 100% | 0% | 0% | 0% | 100% | 0% |
| 0.5 | 57.65% | 15.65% | 32.35% | 16.15% | 100% | 0% |
| 1 | 54.00% | 35.80% | 41.15% | 31.45% | 95.50% | 19.40% |
| 1.25 | 53.25% | 38.25% | 42.10% | 34.60% | 93.10% | 28.55% |
| 1.75 | 51.15% | 39.85% | 42.35% | 36.35% | 91.40% | 35.75% |

Przy variation=0 środkowa baza nadal przesuwa usta z `.894500H` do `.895057H`, a linię włosów z `.947500H` do `.956530H`. To bezpośredni argument, żeby najpierw uzgodnić bazę i punkty odniesienia. Samo obniżenie variation nie rozwiązuje problemu. `applyVariation` skaluje również wzrost, szybkość, kolory i dyskretny styl włosów, więc nie jest czystym suwakiem naturalności geometrii. Zmiana stylu włosów w takim porównaniu może zasłonić poprawę lub pogorszenie twarzy.

## Relacje proporcji

Poniższe wartości są wskaźnikami kodu. Nie są zalecanymi zakresami budowy człowieka. Jaw oznacza promień przekroju `section(.900)`, cheek przekrój `.925 + cheekboneY`, a head height od `chinY` do czubka. Eye width to pełna szerokość apertury przed animacją powiek. Ilorazy promieni i odpowiadających im pełnych szerokości są takie same.

| Wskaźnik | Minimum | P05 | Mediana | P95 | Maksimum |
|---|---:|---:|---:|---:|---:|
| Jaw / cheek width | 0.568 | 0.698 | 0.838 | 1.009 | 1.213 |
| Neck / jaw width | 0.405 | 0.507 | 0.707 | 0.983 | 1.290 |
| Neck / cheek width | 0.345 | 0.434 | 0.592 | 0.806 | 1.008 |
| Head width / chin-to-crown height | 0.597 | 0.707 | 0.859 | 1.023 | 1.144 |
| Przerwa między oczami / szerokość oka | 0.635 | 0.798 | 1.476 | 2.399 | 2.862 |
| Szerokość oka / lokalna szerokość twarzy | 0.101 | 0.127 | 0.172 | 0.228 | 0.281 |
| Szerokość ust / lokalna szerokość twarzy | 0.148 | 0.195 | 0.285 | 0.424 | 0.570 |
| Obrys nozdrzy / szerokość ust | 0.422 | 0.573 | 0.883 | 1.349 | 1.743 |
| Szerokość dłoni / szerokość mankietu | 0.568 | 0.648 | 0.866 | 1.179 | 1.375 |

Obrys nozdrzy jest przybliżeniem z analitycznych elipsoid (`.0065 * noseWidthScale * nostrilWidthScale + .0035 * nostrilWidthScale` dla połowy szerokości), nie segmentacją renderu. Jest szerszy niż usta u 651/2000 postaci. Neck/jaw >1 występuje dla 86/2000. Są to dobre filtry do ręcznej oceny próbek, nie automatyczne kryteria odrzucenia.

Szczególnie ważna jest rozbieżność globalnych i lokalnych współrzędnych: szerokość głowy zależy od kilku mnożników, ale rozmieszczenie cech w Y jest zapisane bezpośrednio jako część H. `headLengthGene` przemieszcza tylko górne przekroje wokół stałego czubka `.999H`; nie skaluje całej twarzy wraz z oczami, ustami i nosem. Grubość szyi zmienia obrys, a pivots neck `.84H` i head `.90H` pozostają stałe. Zmiany wyglądu nie zawsze mają odpowiadającą im zmianę miejsca mocowania.

## Seedy do kontrolowanych renderów

Nie nazywamy tych seedów „dobrymi” i „złymi” przed oceną obrazu. To centralne oraz skrajne przypadki określonych metryk.

| Seed | Powód wyboru |
|---:|---|
| **1533** | Proporcje blisko median; zero zarejestrowanych adjustments. Najlepszy liczbowy baseline. |
| 1660, 618 | Dalsze centralne próbki według siedmiu standaryzowanych ilorazów. |
| **1187** | Największe poszerzenie dolnej czaszki: `.021424H → .038749H`, około +81%. |
| **1574** | Największa suma zapisanych korekt, razem z dużą korektą mocowania szyi. |
| **607** | Maksimum neck/jaw = 1.290; mocne poszerzenie dolnej czaszki. |
| 902 | Maksimum neck/cheek = 1.008. |
| **638** | Najwęższa głowa względem wysokości i największe oczy względem lokalnej szerokości twarzy. |
| **1688** | Przeciwny kraniec: szeroka głowa i małe oczy względem twarzy. |
| **119** | Maksimum obrysu nozdrzy względem ust = 1.743. |
| **593** | Duża dłoń względem wąskiego mankietu = 1.375. |
| 1118, 1477 | Największa korekta podstawy nosa / linii włosów. |
| 938, 844 | Minimalna / maksymalna przerwa między oczami względem szerokości oka. |

W porównaniu warto zachować H, kamerę, światło, pozę i czas animacji; oglądać przód, 3/4 i profil. Dla oceny samego losowania utrzymać również kolory oraz fryzurę. Pokazać neutralną twarz i osobno FEAR/ANGER/PAIN w połowie i pełnej intensywności. Odsłonięta szyja i wersja z docelowym wyposażeniem pozwalają oddzielić proporcje od dopasowania kołnierza. Pełna sylwetka i zbliżenie nie są zamienne: ogromna poprawa na twarzy może być niewidoczna przy kamerze RTS.

## Porównanie możliwych rozwiązań

| Podejście | Zysk | Ograniczenie / koszt |
|---|---|---|
| Tylko mniejsze variation | Szybko ogranicza część skrajności | Nie uzgadnia neutralnej bazy; zmienia także fryzury, kolory i parametry ruchu; część clampów nadal bardzo częsta |
| Własna baza + kilka wspólnych czynników + mały lokalny szum | Zachowuje proceduralność i prostotę; można sterować zgodnością głowy, szyi, twarzy i dłoni | Wymaga autorskiego strojenia i porównań; nie jest automatycznie modelem statystycznym ludzi |
| Kilka zgodnych baz o jednej topologii i blend między nimi | Łatwo oceniać różnorodność i ekspresje; interpolacje zaczynają od sprawdzonych kształtów | Potrzebne spójne targety; unikać dowolnej ekstrapolacji i niezależnego losowania fragmentów |
| Nauczona baza statystyczna, np. FLAME | Gotowe współzależności kształtu, oddzielenie tożsamości/ekspresji/pozy | Import, redukcja, retarget rig, integracja z ubraniami; sprawdzenie konkretnej wersji/licencji/dodatkowych assetów; wynik może być zbyt ciężki dla obecnego pipeline |

**Rekomendacja projektu:** drugi wariant jako następny etap, z możliwością użycia zewnętrznego modelu jako referencji offline. Generator może nadal tworzyć model z parametrów w runtime. Nie trzeba zastępować proceduralności biblioteką gotowych postaci.

Proponowana kolejność:

1. Ustalić i zaakceptować jedną bazę: czaszka/żuchwa, szyja w profilu, otwór ust, oczodół, korzeń nosa. Naprawić niespójności nawet przy variation=0.
2. Wprowadzić współrzędne lokalne głowy i landmarki. Mouth/nose/brow/hairline wyrażać względem właściwych obszarów głowy, a nie niezależnych globalnych wysokości H. Rig i powierzchnia powinny pobierać te same punkty.
3. Wprowadzić kilka wspólnych czynników, np. ogólna szerokość czaszki, długość dolnej twarzy, projekcja środkowej twarzy, budowa szyi. Indywidualne reszty losować łagodnie, bez masy prawdopodobieństwa na granicach. Korelacje stroić na sprawdzonych kształtach, nie wymyślać rzekomo naukowych współczynników.
4. Uzależnić szerokość nosa/ust/rozstaw oczu od lokalnej szerokości twarzy. Zależność powinna być miękka, z zachowaniem różnorodności; twarde ograniczenia zostawić dla ochrony geometrii.
5. Oddzielić variation geometrii od tożsamości kolorystycznej, fryzury i parametrów ruchu. Nowe pola losować z osobnych stabilnych podseedów, żeby dodanie detalu nie przelosowało wszystkich dalszych cech.
6. Ponowić tę samą sondę oraz kontaktówki. Śledzić wielkość korekt i nawarstwienie skrajności, nie tylko liczbę. Nie narzucać celu „zero korekt” bez oceny mechanizmu.

Wdrożenie zmienia wygląd seedów i powinno dostać nową wersję generatora. Jeśli zapisane jednostki mają zachować tożsamość, potrzebna jest jawna migracja lub możliwość odtworzenia starej wersji.

Szyja i dłonie już mają bones (`infantryRig.js:37`, `:46`). Palce są wagowane do kości dłoni i zginane morphem `handsRelax`; nie mają własnych łańcuchów. Rozkłady genów nie naprawią tej ograniczonej artykulacji. Szczegóły deformacji należą do osobnego audytu rigu.

## Źródła pierwotne i licencje

Sprawdzono 2026-09-24. Opis licencji dotyczy wskazanych materiałów; nie obejmuje automatycznie wszystkich tekstur, danych treningowych ani bibliotek powiązanych z projektem.

- [FLAME — oficjalny projekt, Li i in., SIGGRAPH Asia 2017](https://flame.is.tue.mpg.de/): model łączy przestrzeń kształtu nauczoną ze skanów z artykulacją szczęki, szyi i gałek ocznych, korektami zależnymi od pozy i osobnymi blendshape ekspresji. Wniosek projektowy: współzależne kształty i oddzielenie tożsamości od ekspresji są sprawdzonym kierunkiem; nie wynika z tego, że musimy importować cały model.
- [FLAME — oficjalna licencja modeli](https://flame.is.tue.mpg.de/modellicense.html): od listopada 2025 istnieje **FLAME 2023 Open**, oznaczony przez projekt jako CC-BY-4.0, ze zgodą na zastosowanie komercyjne i obowiązkiem przypisania autorstwa. Starsze FLAME 2017/2019/2020/2023 mają oddzielną licencję niekomercyjną. Strona Open zawiera także dodatkowe warunki użycia; przy ewentualnym pobraniu trzeba zachować dokładny tekst licencji danego artefaktu. Nie jest poprawne ogólne twierdzenie „FLAME jest tylko NC” ani „każdy plik FLAME jest CC-BY”.
- [Albrecht, Lüthi, Gerig, Vetter — Posterior shape models, 2013, §2.1](https://shapemodelling.cs.unibas.ch/gravis-literature/publications/2013/2013-posterior-shape-models.pdf): źródło formalizmu statystycznego `shape = mean + Q * latent`, gdzie współczynniki i baza wyznaczają wspólną kowariancję kształtu. To źródło matematyki zależności, nie zakresów proporcji twarzy. Opracowany ręcznie `Q` nadal jest artystycznym modelem, dopóki nie wyznaczono i nie zweryfikowano go na odpowiednich danych.
- [MakeHuman — oficjalna licencja repozytorium](https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md): kod programu jest AGPL, natomiast zasoby dostarczone z programem (m.in. bazowa siatka, targety, pozy i ekspresje) są CC0. Dokument oddziela wynik użytkownika od kodu i zaznacza, że zewnętrzne assety mogą mieć inne warunki. Potencjalne źródło referencji lub autorskiej uproszczonej bazy po sprawdzeniu dokładnego assetu; nie potrzeba kopiować logiki programu do runtime.

## Granice audytu

Nie oceniono percepcji ludzkiej, animacji w przeglądarce, kontaktu kołnierza ani samoprzecięć pełnej siatki. Wyniki nie dowodzą, że każdy skrajny seed wymaga poprawki. Jaw/cheek to zdefiniowane przekroje modelu, nie pomiary landmarków antropometrycznych. Zakres 2000 kolejnych seedów nie wyczerpuje 32-bitowej przestrzeni. Wnioski o kierunku zmian wymagają kontaktówek i oglądania ruchu w docelowym renderze.
