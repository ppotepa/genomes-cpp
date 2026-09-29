RTS PROTOTYPE 0.12.0 | generator infantry-5.0.0 | 24.09.2026

Rozpakuj cały ZIP do nowego katalogu i otwórz index.html dwuklikiem.
Nie potrzeba localhosta. Three.js r128 i OrbitControls są ładowane z CDN.
Połączenie z internetem jest wymagane; nie wyłączaj zabezpieczeń przeglądarki.

NOWE: PŁYNNA POSTAWA I LOKOMOCJA
DEBUG -> UNITS -> Generator jednostek -> Animacja.
Dwa suwaki: Obniżenie postawy (0..1) i Żądana prędkość (m/s).
Można utrzymywać dowolną pośrednią głębokość, również niższą niż w v0.11.
WALK/RUN/CROUCH są presetami jednej rodziny, bez resetu fazy przy zmianie.
Głębsza postawa ogranicza prędkość; bieg najpierw hamuje, potem się obniża.
Na postoju można nadal się obniżać i rozglądać. Faza/pauza działają jak wcześniej.
PRONE/PRONE_MOVE/SITTING pozostają osobnymi postawami i zachowują swoje animacje.
Genom, wyposażenie, wszystkie dziesięć loadoutów i kamera pozostają niezależne.

Kod: js/animation/postureProfile.js, locomotionController.js, bipedFeet.js,
infantryAnimator.js oraz API unit.setLocomotion({crouch, speedMps}).
Dokumentacja: docs/16_PLYNNA_POSTAWA_I_LOKOMOCJA.txt oraz aktualny changelog.

Wykonano składnię, ścieżki, integralność ZIP i krótkie próby numeryczne.
Nie uruchomiono pełnego Three.js/WebGL w środowisku przygotowania paczki.
Ręczna kontrola w przeglądarce: tests/index.html.
Nie ma pełnej symulacji fizycznej ani samokolizji wszystkich kombinacji gear.
