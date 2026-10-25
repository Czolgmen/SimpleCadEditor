# Testy rdzenia i edytora

Projekt `cad_core_tests` jest programem konsolowym C++20 linkującym bibliotekę `cad_core` i kompilującym przenośne `src/editor/HitTester.cpp`, `EditorSession.cpp` oraz `LengthInput.cpp`. Nie wymaga pobierania zewnętrznego frameworka ani uruchamiania aplikacji WinAPI.

## Uruchamianie

Z katalogu repozytorium, w PowerShell:

```powershell
.\tests\run-tests.cmd
```

Skrypt znajduje MSBuild, buduje projekt z biblioteką i uruchamia testy. Plik `.cmd` uruchamia pomocniczy skrypt PowerShell z polityką wykonania ustawioną tylko dla tego procesu, bez zmiany ustawień systemu. Domyślnie wybiera Debug x64. Pozostałe konfiguracje można wskazać jawnie:

```powershell
.\tests\run-tests.cmd -Configuration Release
.\tests\run-tests.cmd -Configuration Debug -Platform Win32
```

W Visual Studio otwórz [rozwiązanie repozytorium](../Projecr1_GK.slnx), wybierz `cad_core_tests` jako projekt startowy i uruchom go przez Ctrl+F5. To zwykły program testowy, bez integracji z Test Explorer.

Program wypisuje wynik każdego przypadku oraz podsumowanie. Kod zakończenia `0` oznacza sukces, a `1` niepowodzenie testów. Sprawdzenia działają również w Release, ponieważ nie używają wyłączanego przez `NDEBUG` mechanizmu `assert`.

## Zakres

[DocumentTests.cpp](core/DocumentTests.cpp) zawiera 35 przypadków obejmujących:

- `findVertex`, `findEdge`, `findPolygon`: odczyt istniejących encji oraz `nullptr` dla nieznanych i zerowych ID, także w pustym dokumencie.
- `addPolygon`: pozycje i kolejność wierzchołków, komplet krawędzi, poprawne końce i zamknięcie cyklu, unikalne niezerowe ID.
- Trójkąt, czworokąt, wielokąt wklęsły i samoprzecinający się.
- Wiele wielokątów bez współdzielenia ID i bez zmiany wcześniej zapisanej geometrii.
- Kopiowanie współrzędnych wejściowych: późniejsza modyfikacja danych wywołującego nie zmienia dokumentu.
- Odrzucanie 0, 1 i 2 punktów oraz NaN i obu nieskończoności w każdej współrzędnej każdego wierzchołka.
- Brak zmian istniejącego wielokąta, osieroconych encji i zużycia ID po odrzuceniu niepoprawnych danych.
- Niezmienność typów wyników metod odczytu przez sprawdzenia kompilacyjne `static_assert`.
- Poprawność i przechodzenie po scenie demonstracyjnej przez `getPolygons()` oraz niezależność dokumentów tworzonych przez `createDemoScene()`.
- `moveVertex`: zmianę wyłącznie wskazanego wierzchołka przy zachowaniu ID, połączeń krawędzi, innych wierzchołków i pozostałych wielokątów.
- Kolejne przesunięcia, akceptowanie niezmienionej pozycji i kopiowanie współrzędnych wejściowych.
- Odrzucanie ruchu dla nieistniejącego ID, NaN i obu nieskończoności w każdej współrzędnej bez zmiany dokumentu.
- `movePolygon`: przesunięcie wszystkich wierzchołków o ten sam wektor, zachowanie topologii, pozostałych wielokątów i liczników ID, kumulowanie przesunięć oraz akceptowanie wektora zerowego.
- Odrzucanie nieistniejącego wielokąta, NaN i obu nieskończoności w każdej współrzędnej przesunięcia.
- Brak częściowych zmian, gdy dopiero ostatni wierzchołek powoduje przepełnienie wyniku w X lub Y, dla obu znaków, albo odwołuje się do brakującego ID.

- `removeFixedLength`: usuwanie pojedynczej, pierwszej, środkowej i ostatniej relacji, zachowanie pozostałych relacji z wielu wielokątów oraz geometrii, ponowne usuwanie i dodanie relacji oraz odrzucanie nieznanych i nieograniczonych krawędzi bez zmian magazynu.

Testy dokumentu używają publicznego API, z wyjątkiem celowego uszkodzenia odwołania do wierzchołka w teście odrzucania ruchu. Nie badają prywatnego układu kontenerów ani awarii alokacji pamięci.

[DocumentParallelTests.cpp](core/DocumentParallelTests.cpp) zawiera 14 przypadków magazynu równoległości: dodawanie par wewnątrz i między wielokątami bez zmiany geometrii, odrzucanie duplikatów w obu kolejnościach, wiele par dla jednej krawędzi, nieistniejące ID, relacje z samą sobą, zerowe i nieskończone kierunki oraz NaN. Sprawdza usuwanie pierwszej, środkowej i ostatniej pary w obu kolejnościach, ponowne dodanie, brak zmian po odrzuceniu operacji i niezależność kopii dokumentu. Usunięcie istniejącej pary działa także po powstaniu zerowej długości lub uszkodzeniu jej odwołań. Getter jest sprawdzany jako `span<const ParallelConstraint>`. Uszkodzone odwołania i współrzędne powstają wyłącznie w fixture testów przez `const_cast` nieconst dokumentu.

Cztery przypadki dotyczą zmian topologii. Podział każdej krawędzi i usunięcie każdego wierzchołka dwóch czworokątów, również na zamknięciu konturu, usuwają wszystkie pary dotyczące zmienionych krawędzi. Pozostałe pary zachowują kolejność i poprawne odwołania; nowe krawędzie nie dziedziczą relacji. Równocześnie sprawdzane jest zachowanie niezwiązanych ograniczeń długości. Odrzucony podział lub usunięcie dla brakujących ID, uszkodzonych końców oraz ochrona trójkąta zachowują magazyn relacji i geometrię.

[Vec2Tests.cpp](geometry/Vec2Tests.cpp) zawiera 22 przypadki obejmujące:

- Konstruktor domyślny i konstruktor ze współrzędnymi.
- Dodawanie, odejmowanie, mnożenie i dzielenie, w tym ujemne skalarne wartości, zero oraz mały niezerowy dzielnik.
- Brak modyfikowania operandów i możliwość wywoływania operacji na wektorach `const`.
- Długość wektora zerowego, osiowego, o ujemnych i ułamkowych współrzędnych oraz większej skali.
- Normalizację do długości 1, zachowanie kierunku i oryginalnego wektora oraz ponowną normalizację.
- Odrzucanie normalizacji wektora zerowego, krótszego niż `1e-10` i dokładnie na tym progu; akceptowanie długości powyżej progu.
- Iloczyn skalarny, jego symetrię oraz kierunki prostopadłe i przeciwne.
- Iloczyn wektorowy, znak skrętu, antysymetrię, równoległość i wektor zerowy.

Wyniki obliczeń zmiennoprzecinkowych porównujemy z tolerancją bezwzględną `1e-12` i względną `1e-12`. Niepoprawne wyniki NaN i nieskończoności powodują niepowodzenie porównania. Przypadki te sprawdzają obecny kontrakt dla skończonych danych; nie narzucają nowej polityki wejściowych NaN, nieskończoności ani arytmetyki na granicy zakresu `double`.

[ValidationTests.cpp](core/ValidationTests.cpp) zawiera 14 przypadków obejmujących wszystkie sześć kodów `ValidationError`, identyfikatory encji w zgłoszeniach oraz:

- Poprawny pusty dokument i scenę z wieloma wielokątami, również wklęsłymi i samoprzecinającymi się.
- Zbyt mało wierzchołków, pustą listę wierzchołków oraz za mało lub za dużo krawędzi bez niepoprawnego indeksowania.
- Brakujące odwołania w listach wielokąta i na obu końcach krawędzi.
- Niepoprawne połączenia kolejnych wierzchołków i zamknięcie cyklu.
- NaN i obie nieskończoności w każdej współrzędnej każdego wierzchołka.
- Zbieranie wielu problemów z różnych wielokątów bez przerywania walidacji.
- Powtarzalność wyniku oraz zachowanie topologii i liczników ID, bez automatycznej naprawy geometrii.

Publiczne operacje dokumentu nie tworzą tych uszkodzonych stanów. Fixture walidatora celowo modyfikuje encje przez `const_cast` wskaźników zwróconych z metod `find...`. Obiekt źródłowy jest nieconst, więc takie usunięcie kwalifikatora jest dozwolone; mechanizm pozostaje wyłącznie w testach. Tę samą technikę wykorzystują przypadki brakujących odwołań w testach przesuwania i hit testingu.

[HitTesterTests.cpp](editor/HitTesterTests.cpp) zawiera 42 przypadki obejmujące:

- Pusty dokument, dokładne trafienie przy zerowej tolerancji i brak trafienia poza tolerancją.
- Wybór najbliższego wierzchołka niezależnie od kolejności kandydatów, także z różnych wielokątów.
- Kołowy obszar trafienia i akceptowanie odległości dokładnie równej tolerancji.
- Odrzucanie ujemnej tolerancji oraz zachowanie pierwszego kandydata przy równych odległościach i pokrywających się wierzchołkach.
- Ujemne i ułamkowe współrzędne świata oraz wyszukiwanie w nowej pozycji po `moveVertex`.
- `hitTestPolygon`: pusty dokument, pojedynczy trójkąt, figury wklęsłe i samoprzecinające się, obie orientacje oraz promień przechodzący przez wierzchołki.
- Regułę parzystości, także dla konturu obiegającego ten sam kwadrat dwukrotnie, oraz udział krawędzi zamykającej figurę.
- Wybór ostatniego rysowanego trafionego wielokąta przy nakładaniu oraz dostępność wszystkich figur rozłącznych.
- Ujemne i ułamkowe współrzędne, trafienia po `movePolygon` oraz odrzucanie figury z brakującą krawędzią lub jej końcem.
- `hitTestEdge`: pusty dokument, trafienia w każdy bok i jego końce, zerową i ujemną tolerancję, akceptowanie granicy tolerancji i odrzucanie punktów poza nią.
- Wybór najbliższej krawędzi niezależnie od kolejności i wielokąta oraz zachowanie pierwszego kandydata przy remisie i pokrywających się bokach.
- Rzut ograniczony do obu końców odcinka, ukośne odcinki w obu kierunkach, ujemne i ułamkowe współrzędne oraz krawędź zerowej długości. Przypadki te sprawdzają również pomocnicze `pointSegmentDistanceSquared` przez publiczne API hit testingu.
- Trafienia po przesunięciu końców i całego wielokąta oraz pomijanie brakujących krawędzi i ich końców bez przerywania wyszukiwania pozostałych boków.

Testy hit testingu używają skończonych współrzędnych i tolerancji. Testy wielokątów sprawdzają punkty poza samym brzegiem; zasada zaznaczania punktów dokładnie na krawędzi lub w wierzchołku pozostaje do ustalenia. Nie testują komunikatów Windows, przechwytywania myszy ani rysowania zaznaczenia.

[SplitEdgeTests.cpp](core/SplitEdgeTests.cpp) zawiera 11 przypadków obejmujących:

- Podział pierwszej, środkowej i zamykającej krawędzi oraz ponowne dzielenie obu połówek.
- Pozycję nowego wierzchołka, kolejność ID, połączenia krawędzi, zachowanie istniejących wierzchołków i pozostałych wielokątów oraz poprawne zużycie liczników ID.
- Zachowanie ID oryginalnej krawędzi dla `start -> środek`; druga połowa dostaje nowe ID.
- Odrzucanie nieznanego ID, brakujących końców, braku właściciela i niezgodności z cyklem wierzchołków bez zmian modelu i zużycia ID.
- Skończony środek przy skrajnych skończonych współrzędnych, krawędź zerowej długości oraz odrzucanie NaN i obu nieskończoności w każdej współrzędnej obu końców.
- Usunięcie wyłącznie ograniczenia dzielonej krawędzi, również dla pierwszej, środkowej, zamykającej i zerowej krawędzi. Obie połówki pozostają bez relacji, także ta zachowująca oryginalne ID.
- Zachowanie wartości i kolejności ograniczeń pozostałych krawędzi i innych wielokątów oraz całego magazynu przy podziale krawędzi bez relacji.
- Zachowanie wszystkich ograniczeń po odrzuceniu nieznanego ID, brakującego końca, braku właściciela, niepoprawnego połączenia i nieskończonych współrzędnych lub NaN. Istniejące przypadki odrzucenia zostały rozszerzone o magazyn relacji.

Przypadki uszkodzonych odwołań używają tego samego testowego `const_cast` co walidator. Testy nie symulują błędów alokacji pamięci.

[DeleteVertexTests.cpp](core/DeleteVertexTests.cpp) zawiera 12 przypadków obejmujących:

- Usuwanie każdego wierzchołka czworokąta, w tym pierwszego i ostatniego, z zachowaniem cyklu, pozycji pozostałych punktów i niezależności innych wielokątów.
- Usunięcie punktu oraz obu dochodzących krawędzi z magazynów, utworzenie nowego odcinka między sąsiadami i zużycie wyłącznie jednego nowego ID krawędzi.
- Powtarzane usuwanie do trójkąta, ochronę każdego punktu trójkąta, nieistniejące i już usunięte ID oraz usunięcie punktu dodanego przez `splitEdge`.
- Brak zmian i zużycia ID po odrzuceniu brakujących sąsiadów, krawędzi, niepoprawnych połączeń, niespójnych list i niejednoznacznej własności.
- Odrzucanie NaN i obu nieskończoności w każdej współrzędnej usuwanego punktu i jego sąsiadów oraz poprawną obsługę zbieżnych pozycji.
- Usuwanie ograniczeń obu znikających krawędzi i brak relacji na nowej krawędzi łączącej sąsiadów. Wszystkie cztery wierzchołki czworokąta są sprawdzane przy braku ograniczeń na obu krawędziach, ograniczeniu tylko jednej z nich i ograniczeniu obu.
- Zachowanie wartości i kolejności relacji pozostałych krawędzi i innych wielokątów oraz ograniczeń po odrzuconej operacji: ochronie trójkąta, nieznanym ID, brakujących odwołaniach, niepoprawnych połączeniach, niejednoznacznej własności i niepoprawnych współrzędnych. Przypadki ponawianego usuwania i zbieżnych pozycji również obejmują relacje.

Uszkodzone stany są tworzone wyłącznie w testach przez `const_cast` obiektu źródłowego, który jest nieconst. Testy nie symulują błędów alokacji pamięci.

[EditorSessionTests.cpp](editor/EditorSessionTests.cpp) zawiera 48 przypadków wykonywanych bez okna i bibliotek Windows:

- Priorytet i wyłączność zaznaczenia, kliknięcie tła, menu kontekstowe bez gestu i niemodyfikowalne wyniki odczytu.
- Przeciąganie punktu z przesunięciem miejsca złapania i figury względem początku gestu, przy zachowaniu pozostałych obiektów.
- Zatwierdzanie ruchu, anulowanie obu rodzajów gestu, Esc, bezpieczne późne anulowanie oraz niezależne migawki kolejnych gestów.
- Odrzucanie błędnych współrzędnych i tolerancji bez utraty gestu, kontynuowanie po odrzuconym wejściu i zachowanie ostatniego poprawnego podglądu.
- Podział i usuwanie z właściwym zaznaczeniem i dostępnością poleceń, ochronę trójkąta oraz polecenia i zmianę zaznaczenia w trakcie ruchu.
- Zachowanie zatwierdzonej topologii po anulowaniu późniejszego przeciągania.
- Przeciąganie z długością: dokładne położenie prowadzonego punktu z przesunięciem miejsca złapania, dopasowanie sąsiada, kontynuację od ostatniego podglądu oraz brak ruchu pozostałych punktów.
- Zachowanie ostatniego poprawnego podglądu po odrzuceniu przez solver, dalszy ruch po odrzuceniu, zatwierdzenie po puszczeniu przycisku oraz anulowanie i Esc odtwarzające pozycje i relacje. Translacja całego wielokąta również zachowuje długości.
- Polecenia dodania, aktualizacji i usunięcia długości na zaznaczonej krawędzi: blokadę jej początku, brak duplikatów, wartość domyślną z geometrii, brak zmian przy błędnej długości lub braku zbieżności, wymaganie odpowiedniego zaznaczenia i swobodne przeciąganie po usunięciu relacji.
- Samą równoległość oraz wspólne długości i równoległość podczas kolejnych podglądów ruchu, z dokładnym położeniem prowadzonego punktu i przesunięciem miejsca złapania. Pary obejmują przeciwne kierunki w jednej figurze i krawędzie różnych figur.
- Zachowanie ostatniego podglądu, relacji i zaznaczenia po degeneracji kandydata, dalszy ruch albo zatwierdzenie po odrzuceniu oraz anulowanie i Esc odtwarzające cały dokument.
- Ustawianie i aktualizację długości z jednoczesnym zachowaniem równoległości, odrzucenie konfliktu długości z równoległością bez zmiany dokumentu oraz translację figury zachowującą relację między wielokątami.

[FixedLengthConstraintTests.cpp](constraints/FixedLengthConstraintTests.cpp) zawiera 17 przypadków obejmujących:

- Zerowy, dodatni i ujemny błąd długości oraz zachowanie małego niezerowego błędu bez zaokrąglania do zera.
- Krawędzie osiowe, odwrócone, przesunięte, o ułamkowych współrzędnych i zerowej długości.
- Bardzo małe dodatnie długości oraz duże długości mieszczące się w zakresie `double`.
- Odrzucanie nieistniejących krawędzi i ich końców, niepoprawnych długości docelowych oraz NaN i obu nieskończoności we współrzędnych obu końców.
- Przepełnienie różnicy współrzędnych i długość wykraczającą poza zakres `double` mimo skończonych różnic.
- Odczyt aktualnej geometrii po przesunięciu wierzchołka, wybór wskazanej krawędzi między wielokątami i brak zmian dokumentu oraz ograniczenia podczas obliczeń.

Porównania uwzględniają tolerancję bezwzględną i względną; bardzo mały wynik jest najpierw skalowany, aby tolerancja bezwzględna nie ukrywała błędnego zera. Brakujące odwołania oraz współrzędne z NaN i nieskończonościami powstają wyłącznie w fixture testów przez `const_cast` nieconst dokumentu.

[ParallelConstraintTests.cpp](constraints/ParallelConstraintTests.cpp) zawiera 18 przypadków obejmujących:

- Równoległe i przeciwne kierunki osiowe oraz ukośne, znaki błędu dla prostopadłości i znane błędy dla innych kątów.
- Zmianę znaku po zamianie krawędzi lub odwróceniu jednego kierunku oraz zachowanie wyniku po odwróceniu obu.
- Niezależne skale długości od `1e-200` do `1e200`, przesunięcia i ułamkowe współrzędne, krawędzie z różnych wielokątów oraz sąsiednie krawędzie ze wspólnym wierzchołkiem.
- Odrzucanie tej samej krawędzi, brakujących krawędzi i każdego z czterech końców, zerowej długości jednego lub obu odcinków oraz NaN i obu nieskończoności w każdej współrzędnej każdego końca.
- Przepełnienie różnicy współrzędnych i niereprezentowalną długość na dowolnej krawędzi, zachowanie małego niezerowego błędu i odczyt aktualnej geometrii po przesunięciu.
- Brak zmian geometrii, topologii, przekazanej relacji i zapisanych ograniczeń długości po poprawnych i odrzuconych obliczeniach.

Testy równoległości porównują skończone wyniki z tolerancją `1e-12`; dla bardzo małego błędu stosują `1e-24`. Niepoprawne odwołania i nieskończone współrzędne powstają wyłącznie w testach przez `const_cast` nieconst dokumentu. Ten zestaw sprawdza funkcję błędu; wspólne rozwiązywanie bada `MixedConstraintSolverTests`. Równoległość nie jest jeszcze podłączona do UI.

[DenseLinearAlgebraTests.cpp](solver/DenseLinearAlgebraTests.cpp) zawiera 8 przypadków: znany układ 3x3, zamianę wierszy przy wyborze elementu głównego, osobliwość, niepoprawne wymiary, NaN i nieskończoności oraz przepełnienie obliczeń, różne skale współczynników, pusty układ i zachowanie danych wejściowych.

[ConstraintSolverTests.cpp](solver/ConstraintSolverTests.cpp) zawiera 29 przypadków: walidację i wczesne zakończenia publicznego API oraz algorytm korekty **z zadanymi macierzami, przez prywatny punkt wstrzykiwania zależności**. Sprawdza oba kierunki blokowania, rozdzielenie korekty między wolne końce, sprzężone długości, wiele wielokątów, relacje nadmiarowe i konfliktowe, limit iteracji, niepoprawne macierze, odrzucanie kroków, zmniejszanie kroku, wyjątki i skalowanie. Osiem przypadków sprawdza pełną ścieżkę publicznego API z obliczanym Jacobianem: ukośną krawędź z blokowaniem obu końców osobno, sprzężony trójkąt, wiele wielokątów, nadmiarowe długości, wycofanie konfliktu i przekroczenia limitu oraz małe i duże skale.

[MixedConstraintSolverTests.cpp](solver/MixedConstraintSolverTests.cpp) zawiera 19 przypadków publicznego `solveConstraints`: puste i już spełnione relacje, oba zwroty równoległych krawędzi, walidację opcji, geometrii i metadanych, zerowe i przepełnione kierunki, samą równoległość oraz wspólne długości i równoległość. Obejmuje wspólny wierzchołek, zamknięty cykl z nadmiarowymi równaniami, trzy wielokąty, niepowiązany punkt blokowany, początkową prostopadłość, oddzielne tolerancje, limit i zerowy budżet iteracji, konflikty, różne skale i relacje przekazane przez span z dokumentu. Sukces jest sprawdzany niezależnie przez funkcje błędów, a odrzucenie przez porównanie dokumentu z kopią wejściową. Próba wyjścia z prostopadłości także musi respektować budżet i wycofanie operacji.

[JacobianTests.cpp](solver/JacobianTests.cpp) zawiera 21 przypadków obejmujących znane pochodne i ich znaki, kolejność wierszy i kolumn, niezależne próbki i zachowanie danych wejściowych, małą i dużą skalę, krok przez `nextafter`, brakujące wierzchołki, niepoprawne dane i błędy próbkowania oraz puste wymiary. Dziewięć przypadków sprawdza wspólny Jacobian długości i równoległości: znane pochodne przy wspólnym wierzchołku, cztery końce krawędzi z różnych wielokątów, zgodne skalowanie wierszy, zamianę pary, zachowanie surowych danych, błędne wejście, degenerację próbki i rzeczywisty krok znormalizowany. Dotychczasowe testy długości nadal działają przez `computeJacobian`. Szczegóły algorytmu opisuje [dokumentacja Jacobiego](../docs/architecture/09-jacobian.md).

[LengthInputTests.cpp](editor/LengthInputTests.cpp) zawiera 6 przypadków: liczby dziesiętne i wykładnicze, przecinek dziesiętny, białe znaki i plus, niepoprawny tekst oraz częściowy odczyt, wartości niedodatnie, NaN, nieskończoności, przekroczenie zakresu i zachowanie małych oraz dużych liczb reprezentowalnych. Natywny dialog i rysowanie nie należą do przenośnego zestawu konsolowego.

[EditorParallelTests.cpp](editor/EditorParallelTests.cpp) zawiera 18 przypadków wyboru i usuwania par w przenośnej sesji: wymaganie niezerowej krawędzi, brak równoczesnego przeciągania, zachowanie trybu po puszczeniu przycisku, anulowanie i ponowny wybór, pary wewnątrz i między figurami, oba zwroty kierunków, dokładną blokadę początku pierwszej krawędzi oraz zachowanie długości. Obejmuje nieistniejący lub zerowy drugi segment, wybór tej samej krawędzi, duplikaty w obu kolejnościach, konflikt z długościami i pełne wycofanie geometrii oraz relacji. Sprawdza też ponowną próbę po odrzuceniu, usuwanie zapamiętanego ID po innych poleceniach, niepoprawne wejście i przeciąganie z nową parą po zatwierdzeniu.

Sześć przypadków usuwania sprawdza wybór pierwszej, środkowej i ostatniej pary z obu pozycji argumentów, zachowanie geometrii, długości i pozostałych par oraz ponowne usunięcie. Odrzucone polecenie zachowuje zaznaczenie, przeciąganie i wybór drugiej krawędzi. Istniejącą parę można usunąć także po degeneracji kierunku, bez uruchamiania solvera. Sukces kończy wybór pary, a późniejsze anulowanie nie odtwarza usuniętej relacji. Przeciąganie po usunięciu respektuje pozostałe ograniczenia, lecz nie usuniętą parę; można ją później dodać ponownie.

[BezierTests.cpp](geometry/BezierTests.cpp) zawiera 16 przypadków dla `bezierPoint` i przyrostowego `sampleBezier`. Obejmuje dokładne końce, znane punkty dla `t=0.25`, `0.5` i `0.75`, liczbę próbek `steps+1`, jeden odcinek oraz różne liczby kroków do 1024. Każda próbka jest porównywana z niezależnym skalarnym wzorem Bernsteina, bez współczynników bazy potęgowej i różnic progresywnych. Krzywe obejmują proste, wygięcia, zmiany kierunku, powtarzające się punkty kontrolne i zbieżne końce bez utraty wnętrza krzywej. Sprawdzane są też odwrócenie kolejności punktów, translacja i skale od `1e-90` do `1e90`, a dla `steps=0` wymagany jest `std::invalid_argument`.

Porównania Béziera odrzucają nieskończone wyniki i używają tolerancji `1e-10` względem skali współrzędnych punktów kontrolnych. Końce sprawdzane są dokładnie. Testy dotyczą skończonych danych i `t` w `[0,1]`; nie określają polityki dla wejściowego NaN, przepełnienia współczynników ani liczby próbek przekraczającej możliwości kontenera.

[EdgeConversionTests.cpp](core/EdgeConversionTests.cpp) zawiera 16 przypadków konwersji odcinek ↔ Bézier. Sprawdza położenie uchwytów w jednej i dwóch trzecich krawędzi, wszystkie pozycje w cyklu dwóch wielokątów, zachowanie ID, końców, pozostałych krzywych i topologii. Konwersja usuwa wyłącznie długość oraz wszystkie pary konwertowanej krawędzi, także między figurami i przy obu kolejnościach argumentów. Ponowna konwersja zachowuje zmodyfikowane uchwyty; powrót do odcinka nie odtwarza relacji, lecz pozwala nadać nowe. Oba ewaluatory i API dokumentu odrzucają ograniczenia na Bézierach, również początkowo prostych.

Przypadki nieistniejących ID, brakujących końców, NaN, obu nieskończoności i przepełnienia obliczonych uchwytów sprawdzają pełne zachowanie dokumentu oraz magazynu relacji po odrzuceniu. Zbieżne końce dają obecnie stałą krzywą. Konwersje nie zużywają ID encji. Celowo uszkodzone końce, współrzędne i edytowane uchwyty są przygotowywane przez `const_cast` nieconst dokumentu wyłącznie w fixture testów; porównania obejmują też wariant geometrii i oba uchwyty. Testy nie obejmują jeszcze ciągłości G1/C1 ani konwersji w interfejsie.

Osiem dodatkowych przypadków `EditorSessionTests` obejmuje konwersję zaznaczonej krawędzi i usuwanie jej relacji, powrót do odcinka, wymaganie zaznaczenia krawędzi, zachowanie interakcji po odrzuceniu oraz jej zakończenie po sukcesie. Ponowna konwersja zachowuje uchwyty, a operacja na istniejącym odcinku zachowuje relacje. Długość, rozpoczynanie równoległości i podział odcinka są niedostępne na Bézierze, lecz wracają po konwersji do odcinka. Porównanie dokumentów sesji obejmuje typ geometrii i oba uchwyty.

[EdgeSamplingTests.cpp](core/EdgeSamplingTests.cpp) zawiera 5 przypadków wspólnego próbkowania: dwa końce odcinka, domyślne 101 próbek Béziera, żądaną liczbę kroków i znane punkty wewnętrzne, krzywą ze zbieżnymi końcami oraz odrzucenie zerowej liczby kroków dla Béziera. `steps` określa rozdzielczość krzywej; odcinek zawsze daje dwa końce.

[CurveHitTesterTests.cpp](editor/CurveHitTesterTests.cpp) zawiera 15 przypadków trafiania w próbkowane krawędzie i wnętrza figur krzywoliniowych. Sprawdza punkty krzywej oddalone od cięciwy, trafienia między próbkami, odrzucenie samej cięciwy i wieloboku kontrolnego, tolerancję, globalny wybór najbliższej krawędzi i remisy. Testy wnętrza obejmują wybrzuszenie na zewnątrz, wcięcie do środka, obie orientacje i krzywą zamykającą, promień przez wierzchołki łamanej i styczność, kilka krzywych w jednym konturze oraz parzystość konturu obiegniętego dwukrotnie. Sprawdzane są też uszkodzone końce, wybór ostatniej zawierającej figury i różne skale oraz przesunięcia. Współrzędne uchwytów są ustawiane przez `const_cast` nieconst dokumentu wyłącznie w fixture, do czasu dodania API edycji uchwytów. Oczekiwania wnętrza dotyczą punktów poza brzegiem; testy korzystają z przybliżenia krzywej wspólnego z rendererem.

[ControlPointHitTesterTests.cpp](editor/ControlPointHitTesterTests.cpp) zawiera 16 przypadków `hitTestControlPoint`: identyfikację pierwszego i drugiego uchwytu, dokładne trafienia przy zerowej tolerancji i pomiar od kursora zamiast początku układu. Sprawdza najbliższy uchwyt w jednej krzywej, między krawędziami i wielokątami, remisy i zbieżne uchwyty, kołową tolerancję z dokładną granicą oraz odrzucenie ujemnej, nieskończonej i NaN tolerancji. Obejmuje też nieskończone i NaN współrzędne kursora przy zwykłej tolerancji, brakujące krawędzie i końce, pojedynczy niepoprawny uchwyt przy drugim poprawnym, uchwyty zbieżne z wierzchołkami i zmianę pozycji lub powrót do odcinka. Zwykłe odcinki nie zwracają uchwytów. Fixture zmienia uchwyty i uszkadza odwołania przez `const_cast` nieconst dokumentu. Testy nie obejmują jeszcze pierwszeństwa uchwytu w `Selection` ani przeciągania.

[ControlPointMovementTests.cpp](core/ControlPointMovementTests.cpp) zawiera 10 przypadków modelu: ruch obu uchwytów do pozycji bezwzględnej bez zmiany pozostałej geometrii i relacji, błędne ID, odcinek zamiast Béziera, nieprawidłową wartość enuma, NaN i nieskończoności oraz pozycję docelową będącą odwołaniem do innego punktu dokumentu. Translacja figury obejmuje wszystkie jej wierzchołki i uchwyty, zachowuje inne figury i relacje, obsługuje kolejne ruchy, wektor zerowy i przesunięcie będące odwołaniem do przesuwanego wierzchołka lub uchwytu. Brakująca encja, niepoprawny uchwyt i przepełnienie pierwszego lub drugiego uchwytu późniejszej krzywej odrzucają operację bez częściowych zmian.

[ControlPointDragTests.cpp](editor/ControlPointDragTests.cpp) zawiera 10 przypadków sesji: wyłączne zaznaczenie uchwytu z pierwszeństwem nad wierzchołkiem, krawędzią i wnętrzem, wyczyszczenie zaznaczenia przez tło, przeciąganie obu uchwytów z przesunięciem miejsca złapania oraz pozycje bezwzględne kolejnych podglądów. Zatwierdzenie, Esc, anulowanie, odrzucenie wejścia, kontynuacja i niezależność migawek kolejnych gestów są sprawdzane razem z pozycjami obu uchwytów, wierzchołkami i metadanymi relacji. Translacja krzywoliniowej figury jest liczona od początku gestu, a jej anulowanie odtwarza wszystkie uchwyty. Polecenia wymagające innego zaznaczenia zachowują aktywny gest uchwytu.

[CurveTestSupport.h](CurveTestSupport.h) współdzieli scenę i porównanie pełnego stanu między tymi dwoma zestawami. Fixture ustawia geometrię bez używania testowanej metody ruchu. Porównanie obejmuje także uchwyty innych krzywych i figur oraz kolejność relacji.

[TestsMain.cpp](TestsMain.cpp) uruchamia dwadzieścia trzy zestawy i podaje wspólny wynik **422 testów**. [TestSupport.h](TestSupport.h) zawiera wspólny typ przypadku testowego oraz sprawdzenia działające w Debug i Release.
