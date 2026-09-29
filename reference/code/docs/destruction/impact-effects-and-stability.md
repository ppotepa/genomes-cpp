# HE, ślady trafień i stabilność ruin

Analiza kolejnej iteracji DESTRUCTION DEMO, 25.09.2026. Zakres: pojedynczy
skopiowany cel, czytelne trafienia i ograniczony koszt. Wyniki wdrożenia
i pomiarów znajdują się na końcu dokumentu.

## Rozpoznane braki

| Objaw | Przyczyna w kodzie | Kierunek poprawki |
|---|---|---|
| HE wygląda jak brak wybuchu | `effects.js` emitował tylko małe brązowe tetraedry; brak błysku, chmury pyłu, charakterystycznego dźwięku. | Kilka krótkich warstw efektu z jednej wspólnej puli. |
| Efekt zatrzymuje się podczas burzenia | Cząstki aktualizowały się w ticku balistyki, wstrzymywanym przez kolejkę przebudowy siatek. | Zegar prezentacji na klatkę renderowania, niezależny od kolejki geometrii. |
| AP lub zatrzymana kula nie zostawia czytelnego śladu | Zdarzenia trafienia nie przekazywały materiału i normalnej; płytkie kontakty metalu/drewna nie tworzyły ubytku. | Materiałowe ślady, krater lub płytka wnęka zależnie od energii. |
| Górna ściana nadal stoi na pojedynczym odległym fragmencie | Graf sprawdzał istnienie drogi do fundamentu, bez ograniczenia zasięgu podparcia bocznego. | Ograniczony model wysięgu, przeliczany przy zmianie geometrii. |
| Osiadły gruz zawisa po zniszczeniu stropu | `settleSleeping()` zmieniało śpiące bryły w `Fixed`, również wysoko nad ziemią. | Zachować uśpione bryły dynamiczne, umożliwiając ponowne obudzenie. |
| Wybuch kosztuje dużo CPU | Promienie osłony wybierały kandydatów z AABB całego odcinka, zbierając wiele nietrafionych brył. | Przecinanie promienia z węzłami BVH, bez zmiany reguł obrażeń. |
| Koszt VFX pozostaje po wygaśnięciu cząstek | Poprzednia pula aktualizowała i wysyłała do GPU także wygasłe miejsca aż do historycznego maksimum. | Zwarta lista aktywnych instancji, brak aktualizacji pustych pul. |

W odtworzeniu problemu na domu seed 2026 usunięcie 28 z 29 dolnych podpór
pozostawiało 71 górnych sekcji nieruchomych; najdalsza leżała około 12 m od
ocalałej podstawy. To błąd uproszczonego modelu stabilności, który występował
jeszcze przed przekazaniem elementów do Rapiera.

## Co wynika z researchu

### Podparcie potrzebuje więcej niż samej łączności

NVIDIA Blast rozdziela strukturę połączeń od opcjonalnego solvera naprężeń,
który rozprowadza siły i ma osobne limity rozciągania, ściskania i ścinania.
Liczba iteracji wpływa na koszt. Dla tego projektu przyjmujemy tańsze
przybliżenie zasięgu podparcia, bez deklarowania dokładnych naprężeń.
[NVIDIA Blast — ExtStress](https://docs.omniverse.nvidia.com/kit/docs/blast-sdk/latest/docs/api/extensions/ext_stress.html).

Uśpienie w Rapier pozwala zachować bryłę dynamiczną bez ciągłego wykonywania
jej symulacji. Bryłę można obudzić; zamiana na `Fixed` zmienia fizyczne
zachowanie, a nie tylko sposób optymalizacji. To rozróżnienie jest istotne
dla gruzu leżącego na stropie, który później znika.
[Rapier — sleeping](https://rapier.rs/docs/user_guides/javascript/rigid_body_sleeping/).

### Wgniecenie zależy od materiału

Dla betonu literatura rozróżnia penetrację, krater i odpryski po stronie
wlotu, odpryski od strony tylnej oraz pełne przebicie. Dla cienkiej stali
występują między innymi odkształcenia plastyczne, wycięcie korka i rozdarcie
krawędzi otworu. Stosowanie jednakowej sprężystej deformacji każdej ściany
byłoby słabym modelem gry.
[Li i in. — local impact effects on concrete](https://www.sciencedirect.com/science/article/pii/S0734743X05000692),
[energy absorption during perforation of thin steel plates](https://www.sciencedirect.com/science/article/pii/S0734743X09000906).

| Materiał | Czytelna reakcja w grze | Tanie przybliżenie |
|---|---|---|
| Cegła, beton, kamień | Krater, jasne świeże krawędzie, pył, drobne odpryski | Lokalny ubytek; drobiny w puli VFX |
| Blacha | Ciemne wgłębienie, rozjaśniona krawędź, krótkie iskry | Płytka wnęka w geometrii, ograniczona grubością |
| Drewno | Wydłużony ubytek, drzazgi, niewielki pył | Ograniczone wycięcie i odpowiedni kolor/kształt drobin |
| Szkło | Szybki rozpad, jasne drobiny | Istniejące zniszczenie sekcji oraz lekki efekt |

Wnęka w metalu jest przybliżeniem plastycznego odkształcenia: usuwa małą
objętość zamiast przesuwać cały metal i zachowywać jego objętość. Pełna
symulacja sprężystości i plastyczności wymagałaby znacznie droższego modelu.
Ślad poniżej progu deformacji może być tylko zmianą wyglądu. Geometria
istotnego ubytku nadal musi odpowiadać modelowi penetracji i kolizji.

### VFX powinny korzystać ze wspólnych pul

Epic zaleca ograniczanie liczby systemów i emiterów, współdzielenie pul,
przenoszenie wspólnych obliczeń poza pojedyncze cząstki i unikanie dużych
jednorazowych alokacji. To zasady, które można zastosować również w Three.js:
jedna pula na rodzaj cząstek, zamiast nowego zestawu obiektów przy każdym
trafieniu. [Epic — scalability and best practices](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara).

Liczba cząstek nie opisuje całego kosztu. Duże, nakładające się przezroczyste
chmury zwiększają liczbę wielokrotnie cieniowanych pikseli. Należy mierzyć
całą scenę i ograniczać także rozmiar oraz czas życia dymu.
[Epic — measuring performance](https://dev.epicgames.com/documentation/en-us/unreal-engine/measuring-performance-in-niagara).

## Docelowa sekwencja HE

Przedziały poniżej są wskazówką artystyczną, a nie modelem fizycznym wybuchu.

1. **Pierwsze klatki:** krótki jasny błysk i gwałtowne wyrzucenie drobin.
2. **Pierwsze dziesiąte sekundy:** odłamki oddalają się od miejsca trafienia,
   pył rośnie wzdłuż trafionej powierzchni.
3. **Kolejne sekundy:** chmura ciemnieje i zanika, odsłaniając wyrwę i gruz.
4. **Informacja dla gracza:** krótki dźwięk detonacji, łagodny impuls kamery
   zależny od odległości i czytelny komunikat trafienia.

Błysk, pył i dekoracyjne odpryski nie potrzebują dodatkowych brył Rapiera.
Fizyczne płyty budynku i balistyczne odłamki pocisku zachowują osobne role.
Zmiana jakości grafiki nie może zmienić trafień ani stabilności konstrukcji.
Drganie kamery powinno działać tylko podczas renderowania, bez zmiany kierunku
następnego pocisku.

## Co warto dodać dalej — kolejność według efektu i kosztu

Poniższe propozycje nie oznaczają funkcji już wdrożonych.

| Priorytet | Funkcja | Korzyść | Ograniczenie kosztu |
|---|---|---|---|
| 1 | Pył i stuknięcie przy pierwszym mocnym upadku płyty | Zawalenie staje się czytelne także po wygaśnięciu wybuchu | Jeden efekt na kontakt o odpowiedniej energii; limit zdarzeń na klatkę |
| 1 | Wyraźniejsza krawędź wyjściowa po AP | Widać kierunek i pełną penetrację | Krótki efekt tylnego odprysku na już wyliczonym wyjściu; bez dodatkowego pocisku |
| 2 | Rysy wskazujące narastające osłabienie sekcji | Gracz widzi, który fragment jest bliski odpadnięcia | Maska materiału albo kilka instancjonowanych śladów; bez cięcia siatki |
| 2 | Tanie łączenie nieruchomego gruzu do renderowania | Mniej wywołań rysowania po dużym wyburzeniu | Zmiana reprezentacji wizualnej partiami; nadal zachować możliwość obudzenia fizyki |
| 2 | Dźwięk stłumiony przez ścianę | Materiał i osłona stają się słyszalne | Jeden promień dla ważnej detonacji, bez raycastu dla każdej drobiny |
| 3 | Zgrupowane płyty stropu i zbrojenie | Bardziej przekonujący wygląd żelbetu | Przygotowane warianty geometrii i mała liczba połączeń; osobny budżet |
| 3 | Krótki wtórny strumień pyłu z osłabionego złącza | Zapowiada lokalne zawalenie | Zdarzenie przy zmianie integralności, bez emitera działającego stale |

Pełna symulacja płynów dla dymu, deformowalne bryły FEM, tysiące fizycznych
drobnych odprysków i wiele świateł z cieniami przy trafieniu mają wysoki koszt.
Najpierw należy wykorzystać czytelne warstwy VFX i ograniczoną geometrię.

## Wdrożenie i pomiary

### Zmiany w tej iteracji

- HE otrzymał błysk, pył i szybkie odpryski z jednej współdzielonej tekstury
  proceduralnej. Materiał trafionej powierzchni wpływa na wygląd drobin.
  Efekty używają najwyżej czterech wywołań rysowania; limit wszystkich
  instancji wynosi 528, a w niskiej jakości 156.
- Efekty, odrzut i krótki komunikat trafienia mają zegar prezentacji,
  działający również podczas kolejkowanej przebudowy geometrii. Pauza
  zatrzymuje prezentację. Drganie kamery i wizualny odrzut broni nie zmieniają
  fizycznej pozycji wylotu ani kierunku strzału.
- Detonacja ma syntetyzowany dźwięk z filtrowanego szumu i tłumienie zależne
  od odległości. Wspólna próbka, limit ośmiu głosów i odstęp między dźwiękami
  trafień ograniczają koszt. Odłamki balistyczne nie uruchamiają osobnych
  dźwięków trafienia.
- Zatrzymany AP może utworzyć wnękę w metalu, a pocisk w materiale kruchym
  krater. Wnęka korzysta z energii normalnej do powierzchni, wytrzymałości
  materiału i głębokości penetracji. Jeden ograniczony zabieg pozostawia
  najwyżej cztery bryły; drobne odpryski budynku nie tworzą brył Rapiera.
  Trafienie poniżej progu deformacji pozostawia tylko ślad wizualny.
- Graf podparcia sumuje boczny wysięg także między piętrami. Domyślne
  limity to 4 m dla dachu, 3 m dla stropu i 1,2 m dla pozostałych sekcji,
  zwiększane do pierwotnego wysięgu wygenerowanego elementu plus 0,25 m.
  To parametry gry, nie obliczenie nośności rzeczywistego budynku.
- Osiadły gruz pozostaje uśpioną bryłą dynamiczną. Może ponownie spaść po
  usunięciu podparcia. Budżet 128 dotyczy brył aktualnie obudzonych; osobny
  limit 512 obejmuje cały zachowany gruz.
- Promienie BVH odrzucają węzły poza rzeczywistą trasą promienia. Aktualizacja
  geometrii ruchomych brył ponownie wykorzystuje tablice. Naprawiono też
  rykoszety odłamków HE, które błędnie uruchamiały wycinanie dużych kraterów.
- Ślady na usuniętych lub odłączonych sekcjach znikają bez dodatkowych
  raycastów. Wygasłe pule nie przesyłają pustych instancji do GPU.
- Podczas resetu nie można wznowić strzelania do poprzedniej kopii celu.
  Opóźnione zdarzenie przechwycenia myszy także respektuje przygotowanie.
- Weryfikacja wykryła okruch o grubości około 0,27 mm, dla którego Rapier
  tworzył opis kształtu, lecz odrzucał natywną wypukłą bryłę. Takie pozostałości
  są usuwane spójnie z modelu i obrazu, zamiast powodować przerwanie sesji.

### Dalsze zmiany balistyki i stabilności

- Stan pocisku jest trwały pomiędzy kontaktami. Rykoszet, otarcie i przebicie zmieniają integralność, deformację, stabilność, efektywną średnicę i opór dalszego lotu.
- Mocno uszkodzony pocisk może zachować rdzeń i zrzucić część masy albo rozpaść się na ograniczone wtórne fragmenty. Budżet fragmentów nie wzmacnia energii tych, które pozostały.
- Kontakt krawędziowy wykorzystuje broad-phase rozsuniętych płaszczyzn, ale kandydat przy narożniku jest potwierdzany odległością do rzeczywistych trójkątów convex hull. Zapobiega to części fałszywych rykoszetów od „kwadratowo rozszerzonych” narożników.
- Pełna perforacja raportuje punkt wyjścia i może wykonać osobny flare tylnej powierzchni. Pył i odpryski wyjściowe korzystają z tego samego zdarzenia, nie z losowego efektu dekoracyjnego.
- Połączenia nośne mają integralność; lokalna energia blisko styku może usunąć konkretną krawędź grafu jeszcze zanim cała sekcja osiągnie globalny próg rozpadu.
- Rapier otrzymuje punkt przyłożenia impulsu. Trafienie poza środkiem masy może więc nadać odłączonemu fragmentowi obrót.
- Statyczna konstrukcja i ruchomy gruz mają osobne BVH. Dodatkowy reverse-index podpór ogranicza koszt lokalnego osłabienia połączeń.
- Maksymalny odcinek lotu wzrósł do 12 m dla głównego pocisku i 6 m dla fragmentu, ale jest skracany na podstawie krzywizny toru. Cały odcinek nadal jest sprawdzany ciągle względem kolizji.
- Eventy wizualizacji lotu są emitowane tylko w trybie diagnostycznym; wyłączenie ich nie zmienia stanu fizycznego.

### Koszt samych efektów

Odtwarzalny [benchmark VFX](effects-benchmark.json) obejmuje 30 sekund,
300 wybuchów i 600 trafień w stal, wraz z tworzeniem i aktualizacją efektów.
Na i5-12600K / Node 24.19.0:

| Jakość | Średni krok CPU | P95 | Najdroższy krok |
|---|---:|---:|---:|
| Normalna | 0,0277 ms | 0,0414 ms | 1,199 ms |
| Niska | 0,0082 ms | 0,0117 ms | 0,164 ms |

Pomiar nie obejmuje GPU, renderowania, dźwięku, balistyki ani fizyki.
Przezroczyste chmury nadal kosztują czas GPU zależny od ich wielkości na
ekranie. Te liczby nie potwierdzają 60 FPS.
