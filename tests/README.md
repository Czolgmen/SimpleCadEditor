# Testy rdzenia i edytora

Projekt `cad_core_tests` jest programem konsolowym C++20 linkującym bibliotekę `cad_core` i kompilującym przenośny `src/editor/HitTester.cpp`. Nie wymaga pobierania zewnętrznego frameworka ani uruchamiania aplikacji WinAPI.

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

[DocumentTests.cpp](core/DocumentTests.cpp) zawiera 23 przypadki obejmujące:

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

Testy dokumentu używają publicznego API. Nie badają prywatnego układu kontenerów ani awarii alokacji pamięci.

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

Publiczne operacje dokumentu nie tworzą tych uszkodzonych stanów. Fixture walidatora celowo modyfikuje encje przez `const_cast` wskaźników zwróconych z metod `find...`. Obiekt źródłowy jest nieconst, więc takie usunięcie kwalifikatora jest dozwolone; mechanizm pozostaje wyłącznie w pliku testowym.

[HitTesterTests.cpp](editor/HitTesterTests.cpp) zawiera 14 przypadków obejmujących:

- Pusty dokument, dokładne trafienie przy zerowej tolerancji i brak trafienia poza tolerancją.
- Wybór najbliższego wierzchołka niezależnie od kolejności kandydatów, także z różnych wielokątów.
- Kołowy obszar trafienia i akceptowanie odległości dokładnie równej tolerancji.
- Odrzucanie ujemnej tolerancji oraz zachowanie pierwszego kandydata przy równych odległościach i pokrywających się wierzchołkach.
- Ujemne i ułamkowe współrzędne świata oraz wyszukiwanie w nowej pozycji po `moveVertex`.

Testy hit testingu używają skończonych współrzędnych i tolerancji. Nie testują komunikatów Windows, przechwytywania myszy ani rysowania zaznaczenia.

[TestsMain.cpp](TestsMain.cpp) uruchamia cztery zestawy i podaje wspólny wynik **73 testów**. [TestSupport.h](TestSupport.h) zawiera wspólny typ przypadku testowego oraz sprawdzenia działające w Debug i Release.
