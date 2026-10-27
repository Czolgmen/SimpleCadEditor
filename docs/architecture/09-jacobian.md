# 09. Numeryczna macierz Jacobiego

[Spis dokumentacji architektury](README.md)

## Rola i wymiary

[`computePointJacobian`](../../cad_core/src/solver/Jacobian.cpp) przybliża pochodne reszt ograniczeń względem współrzędnych wolnych wierzchołków i punktów kontrolnych. Dla `m` składowych reszt i `n` punktów macierz ma `m` wierszy oraz `2*n` kolumn. Punkty przypięte nie tworzą kolumn.

Kolumny odpowiadają kolejno `x0,y0,x1,y1,...`. Wiersze obejmują długości, równoległości i aktywne warunki ciągłości. G0 nie wprowadza równania, G1 dodaje jeden wiersz, a C1 dwa. `DenseMatrix` jest reprezentowana jako `std::vector<std::vector<double>>`.

## Różnice skończone

Dla każdej współrzędnej funkcja wykonuje jednostronną perturbację i ocenia zmianę reszt:

```text
pochodna ≈ (F(x+h)-F(x))/h
h = sqrt(epsilon_double)*geometryScale
```

Obliczenia przebiegają na prywatnej kopii dokumentu. Po zmianie współrzędnej ponownie dopasowywane są łuki. Po pomiarze odtwarzane są zarówno pozycja punktu, jak i bazowe wartości `bulge`, aby kolejne kolumny opisywały ten sam stan początkowy.

Jeżeli dodanie `h` nie zmienia reprezentowanej liczby, `nextafter` wyznacza najbliższą większą wartość. Mianownik wykorzystuje faktyczne przesunięcie, a nie krok zadany. Różnica kątów G1 jest zawijana modulo `2π`, co usuwa sztuczny skok na granicy `-π/π`.

## Normalizacja

Solver używa współrzędnych `q=position/s`, gdzie `s` oznacza skalę geometrii. Reszty długości i C1 są dzielone przez `s`, natomiast równoległość i kąt G1 pozostają bezwymiarowe. Dlatego dla rzeczywistej perturbacji `actualH`:

```text
długość lub C1: J[i,j] ≈ difference/actualH
równoległość lub G1: J[i,j] ≈ difference/(actualH/s)
```

W obu przypadkach wynikiem jest pochodna znormalizowanej reszty względem znormalizowanej współrzędnej. Skala uwzględnia wymiary krawędzi, ramiona kontrolne, zadane długości i strzałki łuków; samo odsunięcie sceny od początku układu nie jest miarą jej rozmiaru.

## Zależności i obsługa błędów

Grupy zależności ograniczają ocenę do równań, na które może wpłynąć perturbowany punkt, z uwzględnieniem analitycznie dopasowywanych łuków. Nieaktualny wektor reszt bazowych powoduje przeliczenie wszystkich grup. Niepoprawny pomiar zwraca `nullopt`; dokument wejściowy pozostaje niezmieniony.

`computeJacobian` i `computeConstraintJacobian` są adapterami prostszych interfejsów. Publiczna ścieżka solvera korzysta z wersji punktowej, która uwzględnia również uchwyty i ciągłość. Szczegółowy [pseudokod](../algorithms.md#13-jacobian-przez-różnice-skończone-w-przód) i [testy](../../tests/README.md) uzupełniają opis.
