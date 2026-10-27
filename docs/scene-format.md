# Specyfikacja formatu sceny `.cadscene`

Format jest tekstowy. Aktualna wersja ma nagłówek `CAD_SCENE 2`. Odczyt obsługuje również wersję 1, której krawędzie nie mają pola grubości i otrzymują domyślne 2 px. Zapis zawsze używa wersji 2. Separatorem dziesiętnym jest kropka, niezależnie od ustawień regionalnych systemu. Zapis wartości `double` wykorzystuje precyzję `max_digits10`, umożliwiając odtworzenie reprezentowanej wartości. Białe znaki oddzielają pola. Kolejność sekcji jest stała, a nieznana wersja lub nadmiarowe dane po `END` są odrzucane.

Serializacja należy do rdzenia niezależnego od platformy: [SceneIO.h](../cad_core/include/cad_core/io/SceneIO.h), [SceneIO.cpp](../cad_core/src/io/SceneIO.cpp). Interfejs przyjmuje strumienie i nie zależy od Windows. [SceneFiles.cpp](../src/platform/win32/SceneFiles.cpp) odpowiada za dialogi, ścieżki Unicode i zapis do pliku tymczasowego z późniejszym zastąpieniem celu.

## Struktura i kolejność rekordów

| Sekcja / rekord | Pola |
| --- | --- |
| `CAD_SCENE 2` | Sygnatura i aktualna wersja |
| `NEXT_IDS` | Następny identyfikator wierzchołka, krawędzi i wielokąta |
| `VERTICES n` | Liczba rekordów `V` |
| `V` | ID, X, Y, ciągłość `G0/G1/C1`, źródło `DEFAULT/EXPLICIT` |
| `EDGES n` | Liczba rekordów `E` |
| `E ... LINE` | ID krawędzi, ID początku, ID końca, grubość, `LINE` |
| `E ... BEZIER` | ID krawędzi, ID początku, ID końca, grubość, `BEZIER`, X1, Y1, X2, Y2 |
| `E ... ARC` | ID krawędzi, ID początku, ID końca, grubość, `ARC`, bulge |
| `POLYGONS n` | Liczba rekordów `P` |
| `P` | ID wielokąta, liczba boków k, k ID wierzchołków, k ID krawędzi, w kolejności obrysu |
| `LENGTHS n` | Liczba rekordów `L` |
| `L` | ID krawędzi, zadana długość |
| `PARALLELS n` | Liczba rekordów `R` |
| `R` | ID pierwszej i drugiej krawędzi |
| `END` | Koniec sceny; po rekordzie dopuszczalne są wyłącznie białe znaki |

## Warunki poprawności

Identyfikatory muszą być dodatnie i unikalne w swoim typie. Liczniki muszą być większe od zapisanych ID. Wierzchołki i krawędzie należą dokładnie do jednego miejsca obrysu; obiekty osierocone i brakujące odwołania są odrzucane. Pary równoległości nie mogą się powtarzać także w odwróconej kolejności.

Grubość jest liczbą całkowitą w zakresie 1-32, w pikselach ekranu. W rekordach `E` wersji 1 pomija się to pole; wszystkie pozostałe pola mają tę samą kolejność. Wyróżnienie zaznaczenia jest tymczasowe i nie zmienia zapisanej grubości.

Odczyt i zapis sprawdzają topologię, poprawność krzywych i ustawień ciągłości oraz błędy ograniczeń przy tolerancjach domyślnych `SolveOptions`. Dane nie są dopasowywane solverem: odczyt odtwarza zapisane pozycje. Limity wynoszą 16 MiB na plik i 100000 rekordów na sekcję.

## Przykład: trójkąt z jedną zadaną długością

```text
CAD_SCENE 2
NEXT_IDS 4 4 2
VERTICES 3
V 1 0 0 G0 DEFAULT
V 2 10 0 G0 DEFAULT
V 3 0 10 G0 DEFAULT
EDGES 3
E 1 1 2 2 LINE
E 2 2 3 5 LINE
E 3 3 1 2 LINE
POLYGONS 1
P 1 3 1 2 3 1 2 3
LENGTHS 1
L 1 10
PARALLELS 0
END
```

## Zakres danych trwałych

Plik nie przechowuje zaznaczenia, gestu myszy, otwartych okien pomocy, timera, rozmiaru okna ani wybranego algorytmu renderowania (GDI/Bresenham/Wu). Nie jest formatem wymiany z zewnętrznymi systemami CAD.

## Zapis i odczyt w aplikacji

Odczyt buduje dokument tymczasowy i publikuje go dopiero po poprawnym zakończeniu walidacji. Błąd nie zastępuje bieżącej sceny. Zapis najpierw serializuje i sprawdza dokument, następnie zapisuje dane do unikalnego pliku tymczasowego w katalogu docelowym. Po zakończeniu zapisu i opróżnieniu buforów plik zastępuje cel. Nieudana operacja usuwa własny plik tymczasowy i zachowuje wcześniejszy plik docelowy.

Dialogi platformowe obsługują ścieżki Unicode.