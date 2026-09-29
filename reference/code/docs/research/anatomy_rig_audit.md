# Audyt sylwetki, szyi i dłoni piechoty

Audyt bazowy: `36b3808`, przed v13 (39 kości). W repo znajduje się aktualizacja 0.14.0 z 30 kośćmi palców v13 (69 łącznie) oraz poprawkami opisanymi w [raporcie głównym](../19_AUDYT_WYGLADU_PIECHOTY.md#14-wdrożenie-poprawek-i-stan-po-zmianach). Pomiarów poniżej nie przeliczano po tych zmianach. Zakres bazowy: odczyt implementacji, analiza zależności i źródeł pierwotnych. CodeGraph nie rozpoznaje części symboli przypisywanych do `window.RTS`; po zapytaniach zastosowano odczyt wskazanych plików.

## Najważniejszy wniosek

Najwięcej poprawy daje lepsza bazowa bryła głowy i wspólna definicja anatomicznych punktów dla szkieletu, powierzchni i wyposażenia, następnie lepszy rozkład wag i proporcji. Obecny model już ma kości szyi, głowy i obu dłoni. Brakuje niezależnych kości palców, a położenie i deformacja szyi mają ograniczenia niezależne od liczby kości.

W trakcie audytu obejrzano rzeczywiste porównania WebGL przygotowane przez główny audyt: [twarze](images/faces.png), [szyje](images/necks.png), [sylwetki](images/bodies.png). Na twarzach widoczne są szerokie poziome półki policzków i szczęki, także w profilu. **To problem kształtu powierzchni, nie tylko jej rozdzielczości.** W `FaceAnatomy` geny szczęki, brody i policzków modyfikują cały promień eliptycznego poziomu, a następnie `point(y,theta)` rozprowadza wynik po całym obwodzie. Lokalna cecha twarzy może więc zmienić się w obwodowe zgrubienie. Zalecenie P0: bazowa bryła z wiarygodną szczęką, potylicą i policzkiem oraz lokalne maski wpływu cech. Dodatkowe poziomy i kąty patchy oczu/ust są dziś dodawane globalnie do całej głowy; dają nierówną gęstość siatki. Ich wpływ na normalne warto sprawdzić osobno, bez przypisywania im całej winy za półki.

Plansza szyi przedstawia narzucone statyczne obroty `neck/head` po `seek`, bez ponownego przeliczenia `neckFlex`. Pokazuje wpływ bind pose i wag, ale nie jest pomiarem całego zachowania runtime patrzenia ani skuteczności corrective w ruchu.

## Co faktycznie istnieje

| Obszar | Stan obecny | Konsekwencja |
| --- | --- | --- |
| Szkielet | 39 `THREE.Bone`: 22 kości ciała i 17 kontrolek twarzy | Nie trzeba budować riggu od nowa |
| Szyja | `chest → neck → head`; `neck=.84H`, `head=.90H`, oba `z=0` | Są dwa osobne obroty, lecz ich punkty odniesienia są stałe |
| Barki | `chest → clavicle.L/R → upperArm.L/R` | Obojczyki istnieją i poruszają ramiona jako rodzice |
| Dłonie | `foreArm → hand`, cała dłoń i palce mają wagę 100% `hand` | Nadgarstek obraca się, palce nie mają niezależnych stawów |
| Palce | Osobne rurki siatki i wspólny morph `handsRelax` | Jeden poziom zgięcia dla obu dłoni; brak chwytu i opozycji kciuka |
| Skóra | Liniowe mieszanie macierzy, maksymalnie cztery wagi na wierzchołek | Bardzo duże skręty i zgięcia mogą spłaszczać przekrój |
| LOD | `high` i `world` zmieniają tessellację; ten sam szkielet | Mniej geometrii nie usuwa kosztu przeliczania kości |
| Fizyka | Kontrakt ragdolla: 14 brył, bez aktywnej symulacji | Liczby kości wizualnych i brył fizyki nie muszą być równe |

Dowody: [szkielet](../../js/rendering/infantryRig.js:19), [dłonie](../../js/rendering/infantrySurface.js:124), [wagi i morphs](../../js/rendering/surfaceBuilder.js:10), [ragdoll](../../js/physics/ragdollSchema.js:4), [zmiana LOD zachowująca rig](../../js/rendering/infantryFactory.js:43).

## Dlaczego szyja może wyglądać na źle osadzoną

1. **Geometria głowy się zmienia, punkty obrotu nie.** Genome zmienia długość szczęki, wysokość brody, szerokość i głębokość głowy. Dolny poziom czaszki może przesuwać się w zakresie `.8755–.8855H`, a `head`, `neck`, oś szczęki i progi wag pozostają stałe. Różnica `.01H` to 17,75 mm dla postaci 177,5 cm. To nie dowód błędu każdego seeda, ale konkretne źródło niespójnej deformacji. [FaceAnatomy](../../js/rendering/faceAnatomy.js:40), [rig](../../js/rendering/infantryRig.js:20).

2. **Szyja przechodzi w pełny eliptyczny przekrój dolnej głowy.** Przekroje szyi kończą się na `.866H`, a następnie są interpolowane do całej podstawy szczęki. Ta sama reguła obejmuje przód pod brodą, boki i potylicę. Brakuje osobnej kontroli zagłębienia pod brodą, przejścia pod kątem żuchwy i tylnego przyczepu pod czaszką. Dodanie kości bez zmiany tego profilu zachowa charakter rurki wyrastającej ze środka głowy. [Profile i interpolacja](../../js/rendering/faceAnatomy.js:19).

3. **Gruba szyja poszerza dolną czaszkę.** `neckAttachmentRadius` wymusza `shaped[0].radiusX >= neck.radiusX*1.03`, aby uniknąć szczeliny. W próbce sąsiedniego audytu (seedy 0–1999, variation=1) korekta wystąpiła u 629/2000 postaci, 31,45%; seed 1574 wymagał zmiany `.021843H → .038430H`. To wzrost promienia o ok. 76%. Zabezpieczenie topologii jest potrzebne, lecz częste i duże poszerzanie całego dolnego przekroju może tworzyć zgrubienie pod małą szczęką. Docelowo korekta powinna dopasowywać przyczep z tyłu i po bokach oraz proporcje szyi, zamiast rozszerzać całą brodę. [Korekta](../../js/rendering/faceAnatomy.js:43), [audyt rozkładu](genome_distribution_audit.md).

4. **Żuchwa deformuje też przód szyi.** `skinWeights` poniżej `.886H` dodaje wpływ `jaw`, rosnący od `.869H`; maksymalny współczynnik to `.82`. Przy otwieraniu ust porusza się więc również obszar opisywany tagiem `neck`. Takie powiązanie może być przydatne pod brodą, ale maska zależna głównie od wysokości i głębokości nie rozróżnia dokładnie żuchwy i przedniej części gardła. Potrzebna maska w przestrzeni punktów anatomicznych. [Wagi](../../js/rendering/faceAnatomy.js:119).

5. **Aktualny corrective jest bardzo prosty.** `neckFlex` rozszerza przekrój promieniowo do `.00065H` w bok i `.0011H` w przód/tył. Jego natężenie zależy od bezwzględnego obrotu X kości szyi; nie rozróżnia skłonu od odchylenia ani skrętu. Nie poprawia osadzenia, nie koryguje asymetrycznie miejsca ściskanego i rozciąganego. [Morph](../../js/rendering/infantryFace.js:43), [sterowanie](../../js/animation/faceAnimator.js:377).

### Zalecana naprawa szyi

Najpierw zdefiniować w jednym obiekcie punkty `neckBase`, `skullBase`, `jawHinge`, `chin`, barki i przebieg kołnierza. Wyprowadzać z nich zarówno bind pose, jak i profile szyi oraz wagi. Zachować nazwy `neck` i `head`, a ich położenie wyliczać z rozwiązanego fenotypu. Skóra przedniej szyi powinna mieć łagodniejsze powiązanie ze szczęką, a profil potylicy osobny przebieg od zagłębienia pod brodą.

Kość `neckUpper` ma sens dopiero po tej naprawie, jeżeli profil w skłonie/prone nadal traci objętość. Możliwy łańcuch: `chest → neck → neckUpper → head`, z podziałem rotacji według kierunku. Samo dodanie kości końcowej bez wag i sterowania nie zmieni obrazu. Nie kopiować pełnej obecnej rotacji szyi na oba segmenty: podwoiłoby to kąt. `FaceAnimator` już dokłada osobne obroty szyi i głowy do pozy ciała, więc nowy segment musi dostać jasno określonego właściciela.

Miejsca wymagające spójnej aktualizacji: `InfantryAnatomy`, `FaceAnatomy.neckWeights/skinWeights`, profile kołnierza, socket `NECK` (dziś `.866H`), animacja prone i patrzenia, indeksy kontaktu głowy, kontrakt ragdolla i jego macierze. [Socket](../../js/equipment/equipmentFit.js:64), [warstwa patrzenia](../../js/animation/faceAnimator.js:303).

## Dłonie: geometria i praktyczny wybór kości

Dzisiejsza dłoń ma cztery pierścienie tworzące płaski korpus, cztery osobne rurki palców i rurkę kciuka. Palce zaczynają się w jednym rzędzie; korpus dłoni kończy się osobnym zamknięciem. Brakuje wspólnej topologii błon między palcami, łuku kostek, wyraźniejszej nasady kciuka i rozdziału długości paliczków. Zwiększanie liczby przekrojów wokół tej samej rurki jedynie wygładzi obecny kształt. [buildHand/makeDigit](../../js/rendering/infantrySurface.js:124).

Do poprawy ogólnego wyglądu wystarczy prostsza, ale lepiej ukształtowana dłoń: lekki łuk kostek, wyodrębniony kłąb kciuka, zwężenie palców, umiarkowanie zgięty spoczynek i kciuk ustawiony przestrzennie względem dłoni. Długość dłoni, przekrój nadgarstka i mankiet powinny współdzielić rozmiar bazowy z małą niezależną wariacją; obecnie `handScale` i `armThicknessScale` są generowane oddzielnie.

| Wariant | Nowe kości | Razem z obecnymi 39 | Co daje |
| --- | ---: | ---: | --- |
| Poprawiona siatka + obecny morph | 0 | 39 | Lepszy spoczynek i sprint; nadal jeden wspólny parametr |
| Uproszczony chwyt: kciuk 2, wskazujący 2, wspólna para dla pozostałych trzech palców; na każdą dłoń | 12 | 51 | Opozycja kciuka, niezależny wskazujący, chwyty i rozluźnienie osobno L/R |
| Każdy palec, łącznie z kciukiem, po 2 segmenty | 20 | 59 | Osobne osie wszystkich palców, uproszczone zgięcie dalszych paliczków |
| Każdy z czterech palców 3 segmenty, kciuk 2; na każdą dłoń | 28 | 67 | Pełne różnicowanie chwytu, więcej przeliczanych kości |
| Każdy palec łącznie z kciukiem po 3 segmenty | 30 | 69 | Największa kontrola; uzasadniona głównie bliską kamerą |

**Rekomendacja:** porównać prototyp 59 kości (osobne osie wszystkich palców) z oszczędnym 51. Wariant 59 lepiej odpowiada celowi naturalnych dłoni również w generatorze postaci; 51 może wystarczyć z kamery gry. Wspólne sterowanie pozostałych trzech palców w wersji 51 to świadomy kompromis estetyczny: przy ciasnym zbliżeniu może wyginać palce wokół zbyt wspólnej osi. Gdy precyzyjny chwyt broni stanie się istotnym elementem bliskich ujęć, przejść do pełnych 67 kości. Dodać zadane pozy `relaxed`, `run`, `grip`, `supportPalm`, z niewielką wariacją, zamiast symulować mięśnie dłoni. Kości palców pozwolą także zastąpić `handsRelax` i odzyskać slot morpha.

**Ważna zależność:** kontakt dłoni obecnie zakłada sztywne lokalne wierzchołki względem `hand`. `buildSupports` zapisuje surowe punkty całej dłoni, a IK szuka najniższego punktu po rotacji nadgarstka. Po dodaniu ruchomych palców należy oprzeć kontakt podporowy na stabilnym obszarze dłoni albo przeliczać punkty po skinningu. Inaczej palce mogą wejść w ziemię lub unosić całą rękę. IK powinien nadal kończyć się na `hand`; kości palców nie powinny zmieniać długości dwusegmentowego łańcucha ramienia. [Kontakty](../../js/animation/infantryAnimator.js:810), [próbki dłoni](../../js/animation/infantryAnimator.js:852), [IK](../../js/animation/twoBoneIK.js:44).

## Sylwetka, barki, ubranie i buty

**Barki.** Siatka jest ciągła między tułowiem i rękawem, ale waga samego obojczyka nie jest używana w przejściu. Port barkowy miesza `chest` z `upperArm`, podobnie początek rękawa. Obrót obojczyka przemieszcza dziecko, więc wierzchołki ważone do ramienia nadal za nim podążają; brak wpływu bezpośredniego ogranicza jednak kształt pośredniej powierzchni nad barkiem. Dać strefę `chest → clavicle → upperArm`, rozdzielić górę barku i pachę oraz dopasować profile rękawa do deltoidu i łokcia. Najpierw wagi, potem ewentualne dodatkowe kości. [Porty i wagi](../../js/rendering/infantrySurface.js:51).

**Sylwetka.** Tułów powstaje z eliptycznych pierścieni o stałym profilu bazowym. Zmieniają się skale, ale stosunek długości ramienia do przedramienia pozostaje `.18/.155`, kolano jest w połowie odcinka biodro–kostka, a odcinki kręgosłupa dzielą tułów stałymi współczynnikami. Warianty różnią się głównie skalami i mogą utrzymywać tę samą sztywność obrysu. Warto dodać niewielkie, skorelowane zmiany położenia talii, spadku barku, łuku klatki i zwężenia kończyn, pilnując stabilności IK. Nie należy uznawać jednej proporcji za jedyną „normalną”. [Rig](../../js/rendering/infantryRig.js:22), [genome](../../js/units/infantryGenome.js:104).

**Ubranie.** Największy efekt da czytelny krój i kilka szerokich zmian powierzchni przy pachach, łokciach, pasie i kolanach. Obecne guziki, kieszenie, kołnierz, szwy i bump splotu już istnieją. Dodawanie kolejnych drobnych rurek szwów może zwiększyć koszt bez poprawienia obrysu. Należy utrzymać spójne wagi naszywanych elementów z podłożem. [Detale](../../js/rendering/infantrySurface.js:226).

**Buty.** Model buduje obie stopy z identycznych symetrycznych przekrojów; stronę rozróżnia położenie X. Brakuje wyraźnej strony przyśrodkowej/bocznej i osobnego obrysu podbicia, pięty oraz asymetrycznego noska. Sama neutralna podeszwa ma długość `.174H` przed `footScale`, czyli ok. 30,9 cm dla 177,5 cm. Dla maksymalnego `H=1.95` i `footScale=1.14` to ok. 38,7 cm zewnętrznej podeszwy przed ewentualną dodatkową szerokością wyposażenia. To obwiednia kodu, nie typowy wynik losowania i nie długość bosej stopy. Warto mierzyć rozkład i relację do szerokości łydki, zamiast globalnie skracać wszystkie buty. Zwęzić rejon pięty, nadać osobny profil noska i podbicia, zmniejszyć optyczną masę cholewki, zachować geometrię kontaktu podeszwy. [buildBoot](../../js/rendering/infantrySurface.js:207).

## Ograniczenia Three.js r128 i koszt

Projekt ładuje dokładnie r128. Shader skinningu wykonuje cztery ważone transformacje macierzą; builder zachowuje cztery największe dodatnie wagi i normalizuje je. Dodanie kości nie zwiększa automatycznie liczby operacji skinningu na wierzchołek, lecz zwiększa przeliczanie hierarchii, macierzy i pozy. Przy kościach pomocniczych należy uważać na ciche obcięcie piątej wagi przez builder. [Lokalny builder](../../js/rendering/surfaceBuilder.js:15), [shader r128](https://github.com/mrdoob/three.js/blob/r128/src/renderers/shaders/ShaderChunk/skinning_vertex.glsl.js).

**Istotny limit morphs:** wszystkie trzy materiały mają `morphNormals:true`. r128 przy tej opcji oblicza pozycje tylko dla czterech slotów; przy wyłączonych normalnych osiem. Aktualnie są cztery cele: `eyelidsClose`, `eyelidsArc`, `neckFlex`, `handsRelax`. Nie można bez analizy dopisać piątego corrective. Mechanizm CPU wybiera do ośmiu wpływów według wartości bezwzględnej, następnie porządkuje je według indeksu; nie gwarantuje „czterech najsilniejszych” w shaderze z normalnymi. [Lokalne materiały](../../js/rendering/infantryMaterials.js:17), [deklaracja slotów r128](https://github.com/mrdoob/three.js/blob/r128/src/renderers/shaders/ShaderChunk/morphtarget_pars_vertex.glsl.js), [obliczenie r128](https://github.com/mrdoob/three.js/blob/r128/src/renderers/shaders/ShaderChunk/morphtarget_vertex.glsl.js), [wybór wpływów r128](https://github.com/mrdoob/three.js/blob/r128/src/renderers/webgl/WebGLMorphtargets.js).

Możliwe rozwiązania: odzyskać slot po przeniesieniu dłoni na kości; rozdzielić twarz i ciało na meshes z osobnymi targetami; albo wykonać odrębną migrację renderera. Podział mesh wymaga zachowania ciągłości szyi, normalnych i kontaktów oraz oceny draw calls. Wyłączenie morph normals może pogorszyć oświetlenie zmienionej twarzy. CPU `skinVertex` obecnie sumuje wszystkie morphs, więc przy przekroczeniu limitu może raportować inny kształt niż GPU. [CPU skinning](../../js/rendering/infantryFactory.js:26).

`Skeleton` przechowuje co najmniej 16 liczb float32 na kość, czyli 64 B macierzy: obecne 39 to 2496 B, 51 to 3264 B, 67 to 4288 B na szkielet, przed pamięcią obiektów, macierzy odwrotnych i wyrównaniem tekstury. W r128 dostępna liczba kości na ścieżce uniform zależy od GPU (`floor((maxVertexUniforms-20)/4)`); ścieżka float vertex textures ma osobny limit 1024. Żadnej wybranej liczby nie należy deklarować jako bezwarunkowo obsługiwanej na każdym urządzeniu. [Skeleton r128](https://github.com/mrdoob/three.js/blob/r128/src/objects/Skeleton.js), [limity renderera r128](https://github.com/mrdoob/three.js/blob/r128/src/renderers/webgl/WebGLPrograms.js).

Każdy morph pozycji i normalnych jest obecnie gęstym buforem całej powierzchni: 24 B na wierzchołek. Przy 10 tys. wierzchołków to ok. 234 KiB dodatkowego CPU/GPU payload na cel, nawet jeśli porusza tylko palcami. To rachunek buforów, nie zmierzony koszt klatki. Warto ograniczać geometrię i częstotliwość twarzy w `world`, zachowując podstawowe proporcje i sylwetkę.

## Porównanie podejść do deformacji

| Podejście | Korzyść | Koszt / ryzyko | Decyzja |
| --- | --- | --- | --- |
| Poprawa punktów anatomii, przekrojów i wag | Usuwa źródło wielu artefaktów; bez nowego systemu | Wymaga strojenia reprezentatywnych seedów | Pierwszy etap |
| Jedna pomocnicza kość szyi | Lepszy rozkład skłonu na długości szyi | Zmiany warstw animacji i kontaktów | Warunkowo po pierwszym etapie |
| Forearm twist na stronę | Łagodniejszy skręt przedramienia przy ustawieniu dłoni | Podział swing/twist, prawidłowa lokalna oś | Dopiero jeśli porównanie ujawnia zapadanie |
| Corrective morphs | Precyzyjnie poprawiają zgięcia | Obecny limit czterech slotów, pamięć buforów | Selektywnie po rozwiązaniu limitu |
| Dual quaternion skinning | Ogranicza zapadanie objętości pod wpływem skrętu | Własny shader i zgodność CPU kontaktów, cieni, eksportu | Nie jako pierwszy krok |

Problem utraty objętości liniowego skinningu i alternatywę dual quaternion opisują autorzy metody wraz z przykładami i implementacją. To uzasadnienie doboru techniki; nie jest dowodem, że DQS sam naprawi złą topologię, pozycję kości czy proporcje. [Kavan i in., zasoby autorów](https://users.cs.utah.edu/~ladislav/dq/index.html).

## Źródła anatomiczne i sposób korzystania

Do kalibracji długości kończyn, stóp, dłoni, obwodów i wzrostu można użyć publicznych tabel ANSUR II. Oficjalna baza zawiera 93 pomiary, 4082 mężczyzn i 1986 kobiet; skany 3D nie są publiczne. To dane określonej populacji wojskowej, więc służą jako punkt odniesienia i źródło relacji, a nie uniwersalny model wszystkich ludzi. [Defense Centers for Public Health: ANSUR II](https://ph.health.mil/topics/workplacehealth/ergo/Pages/Anthropometric-Database.aspx).

NASA OCHMO-HB-004 (Revision A, 2023) podkreśla, że różne wymiary tej samej osoby mają różne percentyle i trzeba uwzględniać pomiary wielowymiarowe, pozycję pomiarową oraz wpływ ubrania. Stąd zalecenie: skorelowane grupy parametrów z zachowaną różnorodnością, a nie jedna idealna proporcja ani niezależne losowanie wszystkich ekstremów. Wzorce pomiaru muszą odpowiadać temu, co mierzymy w siatce; długość zewnętrznej podeszwy nie jest długością bosej stopy. [NASA OCHMO-HB-004, rozdz. 2–3](https://www.nasa.gov/wp-content/uploads/2025/09/ochmo-hb-004.pdf).

## Kolejność wdrożenia i kryteria porównania

1. Utrwalić zestaw seedów: neutralny, drobny, masywny, gruba szyja/wąska szczęka (1574), maksymalna relacja szyja/szczęka (607), duże dłonie/cienki mankiet (593), odwrotne proporcje (1816), oraz zwykłe losowe przypadki. To przypadki diagnozujące, nie lista odrzucanych sylwetek.
2. Porównać `IDLE`, sprint, głęboki crouch, prone, spojrzenie L/R/góra/dół i otwarcie szczęki przy stałej kamerze, świetle, loadoucie i czasie. Front, profil i 3/4; bez hełmu i z wyposażeniem. Poprawa musi obejmować zakres ruchu.
3. Ujednolicić anatomiczne punkty szyi i dopracować profil pod brodą; dopasować kołnierz i wagi żuchwy. Zachować wzrost i kontakty.
4. Poprawić barki, rękawy, nadgarstek/mankiet, sylwetkę buta i kilka dużych fałd ubrania.
5. Porównać warianty 51 i 59 kości z nową geometrią dłoni oraz stabilnym kontaktem podporowym. Ocenić efekt z kamery gry i zbliżenia.
6. Dopiero po takim porównaniu rozstrzygnąć o dodatkowej kości szyi, twist bones i podziale mesh. Mierzyć czas klatki, liczbę draw calls i pamięć; sama liczba kości nie określa wydajności.

Warunek wizualny: szyja ma wiarygodne przejście do czaszki i barków podczas ruchu, szczęka nie ciągnie gardła jak gumy, dłonie nie wyglądają jak płaska łopatka z doklejonymi rurkami, a ubranie i buty mają czytelny krój. Warunek techniczny: ciągłość powierzchni, wagi znormalizowane do czterech wpływów, poprawne kontakty, powtarzalność seeda i zgodność kształtu CPU/GPU. Same wyniki obecnej `UnitDiagnostics.validate` nie rozstrzygają o naturalności: sprawdzają przede wszystkim finitość, wagi i flagi metadanych.
