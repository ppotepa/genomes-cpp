# Audyt proceduralnej twarzy i mimiki

Audyt bazowy: `36b3808`, przed wdrożeniem 0.14.0. Mechanizmy i wartości poniżej opisują tamten kod; aktualne rozwiązania oraz rendery są w [raporcie głównym](../19_AUDYT_WYGLADU_PIECHOTY.md#14-wdrożenie-poprawek-i-stan-po-zmianach) i [galerii](index.html). Wnioski o spodziewanym efekcie wizualnym bazują na kodzie i porównaniach renderów, nie na badaniu użytkowników. CodeGraph nie odtworzył części klas IIFE, dlatego odczyt uzupełniono wyszukiwaniem referencji.

## Wniosek

Największy zysk da korekta neutralnej geometrii i współpracy istniejących kontrolek. Sama większa liczba wielokątów albo silniejsze ekspresje nie usuwają przyczyn. Obecna baza ma już gałki oczne, otwarte oczodoły, połączoną powierzchnię głowy i szyi, kość szczęki, kontrolki brwi/policzków/warg oraz morphy powiek. Warto ją rozwijać proceduralnie, zachowując czytelny, prosty styl.

## Konkretne mechanizmy

| Priorytet | Obecne zachowanie i dowód w kodzie | Skutek / proponowana poprawa |
| --- | --- | --- |
| P0 | Neutralne usta mają stałą połowę wysokości `.00115 H` (`faceAnatomy.js:58`), a kontur jest elipsą (`infantryFace.js:133`). | Otwór w spoczynku ma wysokość `.0023 H`, czyli 4,0 mm dla 175 cm. Brakuje stanu kontaktu warg. Wprowadzić neutralną linię styku, rozdzielić otwarcie szczęki i rozwarcie warg oraz ograniczyć przenikanie. |
| P0 | `lipPress` tylko przesuwa górną i dolną wargę o `.0007 H` i `.0009 H` (`faceAnimator.js:368–371`). `ANGER` jednocześnie otwiera szczękę (`:18,363–364`). | Nazwa „zaciśnięte usta” nie gwarantuje zamknięcia. Rozwiązywać docisk względem aktualnego położenia szczęki; nie dosuwać mechanicznie obu warg bez oceny ich szczeliny. |
| P0 | Brwi są rurkami (`infantryFace.js:178–188`), poruszają je własne kości (`faceAnimator.js:344–352`), lecz `FaceAnatomy.skinWeights` nie przypisuje skórze czoła wpływu tych kości (`faceAnatomy.js:123–137`). | Brwi przemieszczają się niezależnie od czoła. Dodać małe, gładkie obszary wpływu na skórę i dopasować brwi do powierzchni; spłaszczyć profil włosków zamiast pełnego walca. |
| P0 | Wagi presetów są sumowane, potem mnożone przez indywidualne skale, bez końcowego ograniczenia większości kanałów (`faceAnimator.js:252–283`). | W mieszance `ANGER + PAIN` kanały mogą przekroczyć zakres dostrojony osobno. Dodać ograniczenia kanałów i korekty kombinacji. Nie normalizować wszystkich emocji jednym współczynnikiem: połączenie uniesienia wewnętrznej brwi i jej napięcia może być celowe. |
| P1 | Powieki górna i dolna zbiegają symetrycznie do środka oka (`infantryFace.js:93–113`); otwarcie, mrużenie, zmęczenie i mrugnięcie sterują jednym stopniem zamknięcia (`faceAnimator.js:297–301,375–376`). | Jedna szczelina obsługuje różne działania mięśni. Rozdzielić kształt zamknięcia, napięcie dolnej powieki i uniesienie górnej. Zachować aktualną korektę łuku i stałe kąciki. |
| P1 | Gałki obracają się w pionie (`faceAnimator.js:332–337`), powieki nie mają składnika zależnego od spojrzenia. | Przy patrzeniu w górę/dół źrenica porusza się pod nieruchomą szczeliną. Dodać łagodne śledzenie pionowego spojrzenia przez powieki, ograniczone otwarciem i mruganiem. |
| P1 | Oś szczęki jest stała `V(0,.915,.001)` (`faceAnatomy.js:55`); wiele proporcji żuchwy jest losowanych niezależnie (`infantryGenome.js:129–130`). | Różne twarze obracają się wokół tej samej względnej osi. Wyznaczać oś z rozstrzygniętej anatomii czaszki/żuchwy, sprawdzać profil i obszar podbródka. |
| P1 | Skóra i wargi są deformowane głównie kilkoma sztywnymi kontrolkami; kąciki nie są dziećmi szczęki (`infantryRig.js:56–70`, `infantryFace.js:121–124`). | Duże otwarcie może dawać efekt zawiasu i ciągnięcia kącików. Dostroić rozkład wpływu jaw/head/corner w oparciu o odległość i obszar anatomiczny, z korektą dla kombinacji stretch + jaw. |
| P1 | Geny wielu proporcji są niezależne, z rozkładem prawie jednostajnym i obcięciem (`infantryGenome.js:7,42–69`). `mouthY`, `noseBaseY`, brwi i rozstaw oczu są potem poprawiane clampami (`faceAnatomy.js:56,63,68,73`). | Bezpieczne geometrycznie położenie nie musi tworzyć spójnej twarzy. Najpierw wspólne parametry długości dolnej twarzy, szerokości czaszki i środkowej części twarzy, dopiero później małe odchylenia cech; używać miękkiego rozstrzygania ograniczeń. |
| P2 | Gen `mouthAsymmetry` ustawia pozycję spoczynkową kontrolki (`infantryRig.js:68`), ale sam kontur ust go nie używa (`infantryFace.js:133`). Kąciki są przesuwane, bez obrotu (`faceAnimator.js:357–359`). | Pozycja kości w bind pose sama nie wprowadza asymetrii siatki. Gen asymetrii ust wymaga przesunięcia neutralnych punktów powierzchni, a nie tylko pivotu. |
| P2 | Nos i uszy to oddzielne, nakładające się elementy powierzchni (`infantryFace.js:154–176`). Wargi mają zawsze kolor `0x99665a` (`:127`), niezależny od skóry. | Najpierw wygładzić połączenie skrzydeł nosa z policzkiem, profil ucha i przejście czerwieni wargowej. Kolor warg wyprowadzać z palety skóry z małą kontrolowaną różnicą; ostre zmiany koloru przypominają nałożone części. |

Ważne rozróżnienia: obecny zwykły wybór jednej ekspresji używa `setExpression`, która zeruje pozostałe cele (`faceAnimator.js:140–150`), więc pełne sumowanie kilku presetów nie jest przyczyną każdej dziwnej twarzy. API mieszanek `setExpressionWeight` jednak istnieje (`:153–157`) i powinno być bezpieczne. Analogicznie górna/dolna powieka mają nazwane kości, ale widoczna deformacja zamknięcia pochodzi z morphów, nie z ruchu tych kości.

Próbkowanie wykonane w równoległym audycie genomów ([trwałe dane](data/genome_distribution_summary.json), seedy 0–1999, variation=1) potwierdziło częste korekty: `mouthY` 54%, `noseBaseY` 35,8%, `browY.L` 41,15%, `browY.R` 41,5%, promień mocowania szyi 31,45%, rozstaw oczu 0,95%. Także wariant ze wszystkimi genami ustawionymi na środek (`variation=0`) wymaga korekty ust. Wniosek własny: oprócz ekstremów losowania trzeba poprawić sam bazowy układ proporcji; częstość korekty nie jest jednak miarą częstości widocznej groteski.

## Porównanie reprezentacji

| Wariant | Koszt / zakres | Ocena dla projektu |
| --- | --- | --- |
| Korekta parametrów i wag istniejących kości | Mały–średni; bez zewnętrznych assetów, bez wzrostu draw calli | Pierwszy krok: naturalny neutral, zakresy ekspresji, brwi związane ze skórą, proporcje nosa/ust. |
| Kości + nieliczne proceduralne korekty powierzchni | Średni; trzeba rozstrzygnąć budżet shaderów i kolejność deformacji | Najlepszy kierunek docelowy: kości dla szczęki/oczu, kształt powiek i warg rozstrzygany w anatomii. |
| Duży zestaw ręcznie rzeźbionych blendshape'ów na każdy genom | Duży; trudno utrzymać poprawność dla losowych proporcji | Niekorzystny dla obecnej procedury. Bardziej użyteczne są reguły przeliczane z landmarków konkretnej twarzy. |
| Pełny model statystyczny skanów lub symulacja mięśni | Duży wzrost zakresu, zależności i kosztu dostrojenia | Może być materiałem referencyjnym; nie jest potrzebny do usunięcia obecnych problemów. |

## Ograniczenie techniczne przed dodawaniem morphów

Projekt ładuje Three.js r128 (`js/loader.js:23–28`), a materiały włączają `morphNormals` (`js/rendering/infantryMaterials.js:17–19`). Cztery obecne kanały to `eyelidsClose`, `eyelidsArc`, `neckFlex`, `handsRelax` (`surfaceBuilder.js:10`). Shader tej wersji, przy włączonych normalnych morphów, wykorzystuje cztery pozycje. Renderer wybiera do ośmiu aktywnych wpływów, a następnie układa je według indeksu: nie zapewnia to bezpiecznego wyboru czterech najważniejszych.

Wniosek własny: nie dodawać po prostu kolejnych targetów do wspólnej siatki. Najpierw przenieść zwijanie palców na kości, wykorzystać istniejące kontrolki i wybrać dalszą architekturę. Osobna siatka głowy z szyją daje odrębny budżet, lecz wymaga spójnego szwu, normalnych, tagów i obsługi narzędzi. Alternatywą jest osobno zaplanowana aktualizacja renderera. [Shader r128](https://raw.githubusercontent.com/mrdoob/three.js/r128/src/renderers/shaders/ShaderChunk/morphtarget_vertex.glsl.js), [wybór morphów r128](https://raw.githubusercontent.com/mrdoob/three.js/r128/src/renderers/webgl/WebGLMorphtargets.js).

## Co wynika ze źródeł pierwotnych

1. FACS opisuje obserwowalne ruchy poprzez jednostki działania, a nie wyłącznie nazwy emocji. Wniosek dla projektu: zachować proste presety, ale składać je ze spójnych działań brwi, powiek, policzków i warg. To inspiracja do rigowania, nie deklaracja pełnej zgodności z FACS. [Paul Ekman Group — Facial Action Coding System](https://www.paulekman.com/facial-action-coding-system/).
2. Apple rozdziela zamknięcie warg od pozycji szczęki oraz mrugnięcie, mrużenie i ruch powiek związany ze spojrzeniem. Dokumentacja dopuszcza użycie małego podzbioru kontrolek. Wniosek dla projektu: potrzebne jest rozdzielenie kilku funkcji, nie cały rozbudowany zestaw ARKit. [Apple — ARFaceAnchor.BlendShapeLocation](https://developer.apple.com/documentation/arkit/arfaceanchor/blendshapelocation/).
3. Badania Disney pokazują, że otwieranie/zamykanie powiek obejmuje toczenie i składanie skóry; sam liniowy ruch nie oddaje ich geometrii. Wniosek dla projektu: obecny łuk zamknięcia jest właściwym fundamentem, do którego warto dodać różny udział górnej i dolnej powieki, bez modelowania mikrofałd. [Bermano i in., Detailed Spatio-Temporal Reconstruction of Eyelids, 2015](https://studios.disneyresearch.com/2015/07/27/detailed-spatio-temporal-reconstruction-of-eyelids/).
4. FLAME łączy przestrzeń kształtów twarzy z ruchomą szczęką, szyją i oczami oraz korektami zależnymi od pozy. Wniosek dla projektu: oddzielić tożsamość, pozę i ekspresję, a poprawki wyprowadzać ze wspólnych landmarków. Import całego modelu nie jest konieczny. [Li i in., Learning a model of facial shape and expression from 4D scans, 2017](https://flame.is.tue.mpg.de/).
5. Oficjalna dokumentacja Maya opisuje korekty uruchamiane kombinacją innych kształtów, gdy poprawne osobne ruchy źle wyglądają razem. Wniosek dla projektu: osobno rozstrzygać np. `jawOpen + lipPress` i `jawOpen + mouthStretch`, zamiast wyłącznie zmniejszać wszystkie intensywności. [Autodesk — Create combination target shapes](https://help.autodesk.com/cloudhelp/2026/ENU/Maya-CharacterAnimation/files/GUID-7B04F045-71F2-491D-8C2A-18B3DB1AA1F4.htm).

## Kolejność i kryteria porównania

1. Utrwalić materiał bazowy: te same seedy, światło, perspektywa i rozmiar ekranowy; front, 3/4, profil. Osobno bez hełmu i z wyposażeniem.
2. Rozstrzygnąć neutralną anatomię: linia ust, proporcje usta–nos–podbródek, grubość i przyklejenie brwi, mocowanie żuchwy. Nie wybierać tylko kilku korzystnych seedów.
3. Dostroić pojedyncze działania przy 25%, 50%, 100% i dopiero potem presety NEUTRAL/ALERT/ANGER/FEAR/PAIN/FATIGUE. Pełna intensywność ma być czytelna bez przenikania i przesadnego rozciągania.
4. Sprawdzić kombinacje, spojrzenie w górę/dół, mrugnięcie w trakcie ekspresji oraz przejścia. Zdefiniować kontakty, końcowe ograniczenia i korekty.
5. Dodać niedrogie detale dopiero po poprawie kształtu: przejście koloru warg, subtelny fałd górnej powieki, skrzydła nosa, helix/antihelix ucha czy delikatny zarost. W widoku RTS oceniać przede wszystkim sylwetkę i czytelność, w generatorze twarz z bliska.

Szacowany koszt względny: zakresy kanałów i kolory — mały; neutralne usta, wagi brwi oraz związki parametrów — średni; odrębne kształty powiek z budżetem morphów — średni/duży; rozdzielenie siatki albo migracja renderera — duży. Ocena dotyczy zakresu prac, nie obietnicy konkretnej liczby godzin.
