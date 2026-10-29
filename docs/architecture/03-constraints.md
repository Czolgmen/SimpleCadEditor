# 03. Ograniczenia i solver

[Spis dokumentacji architektury](README.md)

## Reprezentacja warunków

[`FixedLengthConstraint`](../../cad_core/include/cad_core/constraints/FixedLengthConstraint.h) wskazuje odcinek i dodatnią długość docelową. [`ParallelConstraint`](../../cad_core/include/cad_core/constraints/ParallelConstraint.h) wskazuje dwa różne odcinki, także należące do różnych wielokątów. Ciągłość jest ustawieniem wierzchołka; aktywne połączenia wyznacza `ContinuityJoins`.

| Warunek | Reszta równania | Liczba składowych |
| --- | --- | --- |
| Długość | `length(B-A)-L` | 1 |
| Równoległość | `cross(u/length(u),v/length(v))` | 1 |
| G0 | Wspólny wierzchołek zapewniony przez model | 0 |
| G1 | `atan2(cross(TinUnit,ToutUnit),dot(TinUnit,ToutUnit))` | 1 |
| C1 | `Tout-Tin` | 2 |

G1 wymaga niezerowych stycznych o zgodnym zwrocie. Równoległość dopuszcza zwroty przeciwne. Długość i równoległość są dostępne wyłącznie dla odcinków. Warunki C1 odnoszą się do lokalnego parametru każdego segmentu w przedziale `[0,1]`.

## Przebieg rozwiązania

Publiczne `solveConstraints` uruchamia w [`ConstraintSolver.cpp`](../../cad_core/src/solver/ConstraintSolver.cpp) tłumioną metodę Gaussa–Newtona z krokiem typu Levenberga–Marquardta. Niewiadome obejmują współrzędne uczestniczących w relacjach wierzchołków i punktów kontrolnych. Punkty przypięte są wyłączone z niewiadomych.

Po analitycznym dopasowaniu łuków obliczane są reszty, skala geometrii i [numeryczna macierz Jacobiego](09-jacobian.md). Dla znormalizowanych danych krok jest wyznaczany z układu:

```text
(J*Jᵀ + λ*I)*y = -r
delta = Jᵀ*y
```

Gęsty układ rozwiązuje eliminacja Gaussa z częściowym wyborem elementu głównego. Próba pełnego kroku jest w razie potrzeby skracana przez backtracking; kolejne nieudane próby zwiększają tłumienie. Niemal prostopadłe odcinki mogą uruchomić deterministyczne przeszukiwanie współrzędnych. Każda przyjmowana poprawka musi spełniać tolerancje albo zmniejszać normę reszt.

Normalizacja długości i C1 służy ocenie kroku. Warunek końcowy korzysta z osobnych tolerancji surowych reszt. Parametry domyślne i szczegóły algorytmów opisuje [rozdział solvera](../documentation.md#8-mechanizm-ograniczeń-i-solver).

## Zatwierdzanie i diagnostyka

Obliczenia przebiegają na kandydacie dokumentu. Wynik zastępuje wejście dopiero po zbieżności, walidacji i kontroli dokładnego położenia punktów przypiętych. `SolveResult` rozróżnia `Converged`, `InvalidInput`, `NoConvergence` oraz `DegenerateGeometry`.

Podczas przeciągania nieudana próbka pozostawia ostatni poprawny podgląd. Polecenie edycyjne jest zatwierdzane w całości albo odrzucane. Brak zbieżności nie dowodzi sprzeczności matematycznej: może również wynikać z degeneracji, uwarunkowania lub ograniczeń lokalnej metody numerycznej. Solver nie gwarantuje rozwiązania jedynego ani globalnie najbliższego.
