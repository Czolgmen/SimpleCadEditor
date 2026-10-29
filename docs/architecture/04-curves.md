# 04. Krzywe i ciągłość

[Spis dokumentacji architektury](README.md)

## Krzywa Béziera

Sześcienna krzywa Béziera wykorzystuje końce `P0`, `P3` z wierzchołków oraz punkty kontrolne `P1`, `P2` z krawędzi:

```text
B(t) = (1-t)³*P0 + 3*(1-t)²*t*P1 + 3*(1-t)*t²*P2 + t³*P3
t ∈ [0,1]
```

[`Bezier.cpp`](../../cad_core/src/geometry/Bezier.cpp) udostępnia obliczenie pojedynczego punktu schematem Hornera, próbkowanie metodą różnic progresywnych i podział de Casteljau dla `t=0,5`. Renderowanie korzysta z próbkowania całej krzywej. Kolejne próbki są łączone odcinkami; funkcje GDI przeznaczone do rysowania Bézierów nie są wykorzystywane.

Podział zachowuje kształt przed ponownym rozwiązaniem ograniczeń. Każda nowa połowa otrzymuje własny parametr `[0,1]`, co może wymagać korekty sąsiedniego C1. Nowy wierzchołek otrzymuje domyślne G1.

## Łuk okręgu

Łuk jest opisany końcami i parametrem `bulge=tan(θ/4)`, gdzie `θ` oznacza skierowany kąt przebiegu. Znak określa zwrot, a moduł równy 1 oznacza półokrąg. [`Arc.cpp`](../../cad_core/src/geometry/Arc.cpp) wyznacza środek, promień, styczne i próbki równomierne względem kąta.

G0/G0 wymaga półokręgu. Przy jednym G1 [`ArcContinuity.cpp`](../../cad_core/src/model/ArcContinuity.cpp) analitycznie wyznacza `bulge` ze stycznej sąsiada. Parametr nie jest niezależną niewiadomą solvera; jest ponownie wyznaczany podczas oceny geometrii. C1 i G1 na obu końcach łuku są niedopuszczalne. Końce muszą być różne, a parametry skończone i reprezentowalne.

Podział łuku zastępuje go dwoma odcinkami przechodzącymi przez punkt `t=0,5`; zmienia to kształt obrysu. Nowy wierzchołek otrzymuje G0.

## Styczne i klasy ciągłości

[`EdgeTangents.cpp`](../../cad_core/src/model/EdgeTangents.cpp) udostępnia pochodne na początku i końcu każdej geometrii. Dla odcinka wynoszą `B-A`, a dla Béziera `3*(P1-P0)` i `3*(P3-P2)`. Pochodna łuku uwzględnia kąt przebiegu oraz wektor promienia.

G0 wynika ze wspólnego ID wierzchołka. G1 wymaga zgodnego kierunku i zwrotu niezerowych stycznych. C1 wymaga równości obu składowych pochodnych. Przy połączeniu odcinka z Bézierem C1 wyznacza długość przyległego ramienia kontrolnego równą jednej trzeciej długości odcinka.

Konwersja na Béziera nadaje G1 tylko ustawieniom oznaczonym `Default`; jawne ustawienia `Explicit` są zachowywane. Konwersja odcinka na krzywą usuwa jego ograniczenia długości i równoległości.

## Wspólne próbkowanie

[`sampleEdge`](../../cad_core/src/model/EdgeSampling.cpp) zwraca końce odcinka albo łamaną krzywej, domyślnie ze 100 segmentów. Te same punkty wykorzystują renderer, test odległości od krawędzi i test wnętrza. Dzięki temu wskazywany obrys odpowiada jego przybliżeniu na ekranie. Stała liczba segmentów nie zapewnia adaptacyjnej kontroli błędu przybliżenia.
