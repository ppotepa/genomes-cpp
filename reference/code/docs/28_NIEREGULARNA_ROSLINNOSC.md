# Nieregularna roślinność świata

START NEW używa 90 drzew i do 42 krzewów (leszczyna, głóg, dzika róża). Dziesięć eliptycznych skupisk o różnych rozmiarach miesza się z pojedynczymi roślinami poza skupiskami. Rozmieszczenie, gatunki, obrót i proporcje wynikają z seeda świata. Pas marszu pozostaje wolny; strome zbocza są odrzucane.

Każdy z sześciu gatunków drzew ma młodszy i starszy wariant genomu. Wysokość instancji wynosi 0,52–1,45 skali prototypu, szerokość jest różnicowana niezależnie. Zamiast stałego odstępu 7 m sprawdzane są lokalne promienie podstawy roślin: korony mogą na siebie zachodzić, a pnie zachowują odstęp. Krzewy częściej trafiają w skupiska niż drzewa.

Gen `girth` zmienia promienie pnia i gałęzi niezależnie od wysokości. Gen `damage` tworzy krótkie, bezlistne kikuty z zamkniętym przekrojem, zatrzymując generowanie dalszych gałęzi. Uszkodzenia mają osobny strumień losowy i wspólny szkielet dla sezonów oraz LOD. Oba geny uczestniczą w kluczach wszystkich cache geometrii i szkieletu oraz w cache edytora. Stare DNA bez tych pól otrzymuje `girth=0.5`, `damage=0`, zachowując wcześniejszą geometrię.

Edytor roślin udostępnia suwaki „Grubość pnia” i „Ułamane gałęzie”. Świat zachowuje instancing, trzy poziomy LOD, regionalny culling i współdzielone macierze instancji. Paleta jest ograniczona do 15 prototypów; większa różnorodność zwiększa liczbę grup renderowania względem wcześniejszych sześciu prototypów.

Weryfikacja: `vegetation_variation.cjs` sprawdza 64 seedy (powtarzalność, skupiska i przerwy, krzewy, różne skale, odstępy podstaw) oraz wszystkie 20 gatunków (grubość, uszkodzenia i izolacja cache). Testy plant async/cache/buffer potwierdzają zgodność sync/async, zwalnianie zasobów i zachowanie wcześniejszej geometrii przy domyślnych nowych genach. Test battlefield w Chrome obejmuje render, nowe rośliny, 50 jednostek, marsz, zatrzymanie i restart. To testy poprawności, bez benchmarków wydajności.
