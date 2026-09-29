# START NEW — v0.18.0

Przycisk **START NEW** w prawym panelu mapy generuje nowy scenariusz:

- mapa 200 × 200 m, siatka terenu 160 × 160, wysokość z trzech skal szumu;
- 90 drzew z sześciu gatunków: dąb, brzoza, sosna, świerk, buk, modrzew;
- dwie armie po 25 żołnierzy, każda w szyku 5 × 5;
- początek marszu po przeciwnych stronach mapy, prędkość 1,55 m/s;
- zatrzymanie naprzeciw siebie, przednie szeregi w odległości 8 m.

Żołnierze używają istniejącego genomu, ekwipunku, animacji chodu i próbkowania powierzchni terenu. Każda armia ma dowódcę i 24 strzelców. To scenariusz marszu: nie obejmuje ostrzału, obrażeń ani taktycznego AI. Drzewa są rozmieszczone poza korytarzem ruchu, więc ten scenariusz nie wymaga omijania przeszkód. Nie wprowadza ogólnego systemu kolizji ani nawigacji w lesie.

Każdy start losuje nowy seed widoczny w panelu. Do odtwarzania scenariusza z konsoli można użyć `RTS.game.startNew(20260924)`. Teren, drzewa i genomy jednostek wynikają z tego seeda. Warianty drzew są współdzielone przez instancje, zamiast generować 90 niezależnych kompletów geometrii.

Generowanie jest dzielone na paczki po pięciu żołnierzy z komunikatem postępu. Podwójny start jest blokowany podczas budowy. Nowy świat zastępuje stary dopiero po udanym generowaniu. Poprzednie jednostki, teren i zasoby drzew są zwalniane. Generatory piechoty i roślin pozostają dostępne w panelu.

## Sprawdzenie

`rtk proxy node docs/research/tools/infantry_capture.cjs battlefield`

Test rzeczywistej przeglądarki sprawdza liczbę jednostek i stron, rozmiar oraz granice sektorów, nierówność gruntu, liczbę drzew, drożność korytarza, kierunek ruchu, położenie na gruncie, fazę zatrzymania i ponowne uruchomienie przyciskiem bez akumulacji jednostek. Faza zatrzymania jest sprawdzana po przeniesieniu jednostek blisko celu; nie jest to symulacja całego marszu od początku do końca. Wyniki geometrii i draw calli opisują pojedynczy kadr, a nie pomiar FPS.

[Podgląd](research/images/battlefield.png) · [Wyniki testu](research/images/battlefield.json)
