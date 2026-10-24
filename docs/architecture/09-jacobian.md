# Macierz Jacobiego

[Spis treści](README.md)

Prototyp solvera długości jest zaimplementowany w `cad_core/src/solver/ConstraintSolver.cpp`, a **`computeJacobian` w `cad_core/src/solver/Jacobian.cpp`** oblicza pochodne przez różnice skończone. Każda współrzędna jest próbkowana na osobnej kopii dokumentu. Nieudany pomiar zwraca `nullopt`, co solver traktuje jako brak zbieżności z zachowaniem dokumentu wejściowego. Solver jest uruchamiany przez `EditorSession` przy przeciąganiu wierzchołka i przy nadawaniu długości z menu PPM. Podczas przeciągania blokowany jest prowadzony punkt, a przy nadawaniu długości pierwszy koniec krawędzi.

## Co oznacza macierz

Niech `F[i]` oznacza błąd długości ograniczenia `i`. Niewiadome to kolejno współrzędne wierzchołków z `variableVertices`: `x0, y0, x1, y1, ...`. Wierzchołek blokowany przez użytkownika nie trafia do tej listy.

`J[i][j]` opisuje, jak szybko zmienia się błąd `i`, gdy zwiększamy współrzędną `j`. Jest to pochodna cząstkowa. Dodatnia wartość oznacza lokalny wzrost błędu, ujemna jego spadek. Zmiana wierzchołka nieuczestniczącego w danej relacji daje zero w jej wierszu.

Dla `m` ograniczeń i `n` ruchomych wierzchołków wynik to `m` wierszy i `2*n` kolumn. `DenseMatrix` jest aliasem `std::vector<std::vector<double>>`, więc `J[i][j]` oznacza wiersz `i`, kolumnę `j`.

## Argumenty funkcji

| Argument | Znaczenie |
| --- | --- |
| `document` | Aktualny kandydat, którego nie wolno zmieniać podczas pomiarów |
| `constraints` | Ograniczenia w kolejności wierszy |
| `variableVertices` | Ruchome wierzchołki w kolejności par kolumn |
| `baseErrors` | Już obliczone błędy dla `document`, w jednostkach świata |
| `geometryScale` | Wspólna skala sceny, pomocna przy wyborze małego przesunięcia |

`evaluateErrors(probe, constraints)` jest gotową funkcją pomocniczą. Zwraca `optional<vector<double>>` z błędami w tej samej kolejności lub `nullopt`, gdy obliczenie się nie udało.

## Zaimplementowany algorytm

1. Utwórz macierz wypełnioną zerami o rozmiarze `constraints.size()` na `2 * variableVertices.size()`.
2. Dla każdego ruchomego wierzchołka wykonaj osobny pomiar X i Y. Kolumna X ma numer `2 * vertexIndex`, a Y `2 * vertexIndex + 1`.
3. Odczytaj oryginalną pozycję i utwórz świeżą kopię `Document probe = document` na potrzeby danej kolumny.
4. Zmień tylko mierzoną współrzędną o mały krok `sqrt(numeric_limits<double>::epsilon()) * geometryScale`. Niepoprawna skala, niedopasowana liczba błędów, nieskończone błędy bazowe albo krok, który sam uległ niedomiarowi do zera, powodują zwrot `nullopt`.
5. Dodawanie odbywa się w `double`: jeśli nowa współrzędna jest równa starej mimo dodatniego kroku, użyj `std::nextafter(original, infinity)`, aby dostać najbliższą większą reprezentowalną liczbę. Jeśli przesunięcie lub nowa współrzędna są nieskończone albo krok jest zerowy, zwróć `nullopt`.
6. Przesuń wierzchołek przez `probe.moveVertex(...)`. W razie niepowodzenia zwróć `nullopt`.
7. Oblicz `sampleErrors = evaluateErrors(probe, constraints)`. Gdy nie ma wyniku, zwróć `nullopt`.
8. Wyznacz **rzeczywisty krok** jako `newCoordinate - originalCoordinate` i dla każdego ograniczenia wypełnij bieżącą kolumnę:

   ```text
   J[row][column] = (sampleErrors[row] - baseErrors[row]) / actualStep
   ```

9. Sprawdź skończoność obliczonych elementów. Zwróć gotową macierz po wypełnieniu wszystkich kolumn.

Świeża kopia na każdą kolumnę zapobiega sumowaniu przesunięć: przy pomiarze Y zaczynamy od oryginalnej pozycji, a nie od punktu z wcześniej przesuniętym X. `nullopt` oznacza nieudany pomiar, a nie spełnione ograniczenie.

W tej wersji są to różnice jednostronne. Dobór kroku jest przybliżeniem numerycznym; przy skrajnie dużej translacji i bardzo krótkich odcinkach rozdzielczość `double` ogranicza dokładność. Po uruchomieniu prototypu sprawdzimy również takie sceny i ewentualnie zastosujemy współrzędne lokalne lub różnice centralne.

## Przykład

Zablokowany punkt `A = (0,0)`, ruchomy `B = (6,8)`, wymagana długość `5`. Długość wynosi `10`, więc `baseErrors[0] = 5`.

- Po małym przesunięciu B w X długość wzrasta w przybliżeniu o `0.6*h`.
- Po małym przesunięciu B w Y długość wzrasta w przybliżeniu o `0.8*h`.

Wynik powinien być bliski macierzy `[[0.6, 0.8]]`. Solver wykorzysta ją do wyznaczenia ruchu w stronę `B = (3,4)`. Sam Jacobian nie przesuwa docelowego kandydata.

## Skalowanie i testy

W funkcji zwracamy pochodne **surowego błędu względem współrzędnych świata**. Nie dzielimy ich dodatkowo przez `geometryScale`. Reszta solvera używa `F/scale` i korekty współrzędnych znormalizowanych `deltaWorld/scale`; przy wspólnej skali pochodna pozostaje taka sama: `d(F/scale)/d(x/scale) = dF/dx`.

Testy w `tests/solver/JacobianTests.cpp` porównują macierz ze znanymi pochodnymi, sprawdzają kolejność wierszy i kolumn, zera dla niepowiązanych wierzchołków, niezależność próbek, skalowanie i reprezentowalny krok przy dużej translacji. Obejmują także niepoprawne dane i nieudane pomiary.

Testy solvera nadal wykorzystują zadane macierze do izolowanego sprawdzania tłumienia, doboru kroku i wycofania operacji. Dodatkowo pełna ścieżka publicznego `solveFixedLengths`, z obliczaniem Jacobiego, jest sprawdzana na ukośnej krawędzi z blokowaniem obu końców osobno, trójkącie z trzema długościami, wielu wielokątach, relacjach nadmiarowych i konfliktowych, limicie iteracji i różnych skalach sceny.
