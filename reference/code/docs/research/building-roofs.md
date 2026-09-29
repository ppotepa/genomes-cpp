# Dachy Building Lab

Generator obsługuje trzy publiczne formy dachu: `GABLE`, `HIP` i `FLAT`. Aneksy korzystają wewnętrznie z `SHED`, który nie jest wybierany jako typ głównej bryły.

`buildingRoofs.js` wyprowadza połacie bezpośrednio z obrysu każdej masy. Te same krawędzie są używane przez ściany i walidację planu. Każda masa musi mieć co najmniej jedną powierzchnię dachu, a współrzędne połaci muszą być skończone.

Test `tests/building_v6.cjs` sprawdza deterministyczność planów v6, obecność połaci i brak zdegenerowanych powierzchni w rendererze.
