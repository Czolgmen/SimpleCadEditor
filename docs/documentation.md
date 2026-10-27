# Edytor wielokątów — dokumentacja projektu

Projekt z zakresu grafiki komputerowej obejmuje implementację interaktywnego edytora geometrii dwuwymiarowej. Aplikacja umożliwia edycję wielokątów, których krawędzie są odcinkami, krzywymi Béziera trzeciego stopnia lub łukami okręgów. Moduł numerycznego rozwiązywania ograniczeń (solver) utrzymuje zadane długości, równoległość odcinków oraz dopuszczalne warunki ciągłości.

Implementacja realizuje wariant **A-K**: relację równoległości i domyślną ciągłość **G1** przy konwersji krawędzi na krzywą Béziera. Projekt wykorzystuje **C++20, WinAPI i GDI**. Własna rasteryzacja obejmuje algorytmy Bresenhama i Xiaolina Wu. Model, geometria, solver oraz logika sesji edytora nie zależą od interfejsu Windows.

Dokument opisuje zakres funkcjonalny, obsługę, architekturę i metody weryfikacji. Uzupełniają go [specyfikacja algorytmów z pseudokodem](algorithms.md), [opis architektury](architecture/README.md), [format sceny](scene-format.md) oraz [dokumentacja testów](../tests/README.md).

Skrócona instrukcja jest dostępna przez **F1** i **Pomoc → Obsługa programu**. Przycisk **Dokumentacja** oraz **Pomoc → Dokumentacja algorytmów...** otwierają pełną dokumentację w osobnym oknie. Treść jest dołączana do zasobów EXE podczas kompilacji. Oba okna obsługują zmianę rozmiaru, przewijanie i kopiowanie tekstu; nie wymagają internetu ani zewnętrznych plików Markdown.

## Spis treści

1. [Uruchomienie i scena demonstracyjna](#1-uruchomienie-i-scena-demonstracyjna)
2. [Mysz, klawisze i zaznaczanie](#2-mysz-klawisze-i-zaznaczanie)
3. [Operacje i ograniczenia](#3-operacje-i-ograniczenia)
4. [Krzywe i ciągłość](#4-krzywe-i-ciągłość)
5. [Założenia i sytuacje wyjątkowe](#5-założenia-i-sytuacje-wyjątkowe)
6. [Model danych i architektura](#6-model-danych-i-architektura)
7. [Algorytmy geometryczne i rysowanie](#7-algorytmy-geometryczne-i-rysowanie)
8. [Mechanizm ograniczeń i solver](#8-mechanizm-ograniczeń-i-solver)
9. [Budowanie, testy i przekazanie programu](#9-budowanie-testy-i-przekazanie-programu)
10. [Scenariusz prezentacji](#10-scenariusz-prezentacji)

## 1. Uruchomienie i scena demonstracyjna

Po uruchomieniu aplikacja tworzy dwa wielokąty o pięciu wierzchołkach. Scena przedstawia podstawowe typy geometrii i relacji.

| Obiekt | Zawartość |
| --- | --- |
| Lewy wielokąt | Bézier z dwoma uchwytami; C1 przy pionowej krawędzi i G1 przy ukośnej; zadane długości 180 i 120 |
| Prawy wielokąt | Łuk G1 oraz półokrąg G0/G0; zadana długość dolnej krawędzi 160 |
| Relacje między figurami | Dwie pary równoległości łączące dolne i pionowe odcinki |

Funkcja `createDemoScene` w [DemoScene.cpp](../cad_core/src/demo/DemoScene.cpp) buduje model, sprawdza jego strukturę i rozwiązuje aktywne ograniczenia przed wyświetleniem. W początkowym stanie relacje są spełnione. Rozmieszczenie uwzględnia obszar sceny i pasek narzędzi w oknie o rozmiarze klienta 640 × 420 px. Zmniejszenie okna może ograniczyć widoczną część geometrii, bez zmiany jej współrzędnych.

Każda sesja rozpoczyna się od sceny demonstracyjnej. Własny stan można zachować w pliku `.cadscene`. Automatyczny zapis i automatyczne otwieranie ostatniego dokumentu nie są zaimplementowane.

## 2. Mysz, klawisze i zaznaczanie

Skróty LPM i PPM oznaczają odpowiednio lewy i prawy przycisk myszy.

| Wejście | Działanie |
| --- | --- |
| LPM na wierzchołku lub uchwycie | Zaznaczenie; przytrzymanie przycisku umożliwia przesuwanie punktu |
| LPM w pobliżu krawędzi | Zaznaczenie krawędzi |
| LPM wewnątrz wielokąta | Zaznaczenie i translacja całej figury |
| LPM na pustym tle | Usunięcie zaznaczenia |
| PPM na elemencie | Wskazanie elementu i menu kontekstowe |
| Shift+F10 | Menu bieżącego zaznaczenia |
| Esc podczas przeciągania | Odtworzenie dokumentu sprzed gestu |
| Esc podczas wskazywania pary | Anulowanie wyboru drugiego odcinka |
| Esc bez aktywnej operacji | Usunięcie zaznaczenia |
| Delete | Usunięcie wierzchołka z zachowaniem co najmniej trzech wierzchołków figury |
| Ctrl+O | Wczytanie sceny |
| Ctrl+Shift+S | Zapis sceny pod wybraną nazwą |
| F1 | Otwarcie lub przywołanie instrukcji obsługi |
| Przycisk Dokumentacja | Otwarcie lub przywołanie pełnej dokumentacji |

Wskazywanie ma ustaloną kolejność: **uchwyt -> wierzchołek -> krawędź -> wnętrze**. Tolerancja punktów i krawędzi wynosi 6 jednostek świata, co przy bieżącej skali odpowiada 6 px. Wierzchołki są oznaczone okręgami o promieniu 5 px, a uchwyty kwadratami o boku 10 px. Przy nakładaniu się wnętrz wybierana jest figura rysowana jako ostatnia.

Przeciąganie zachowuje przesunięcie między kursorem a wskazanym punktem, ograniczając zmianę położenia przy rozpoczęciu gestu. Po zaakceptowanej aktualizacji punkt pozostaje dokładnie w pozycji wynikającej z kursora i tego przesunięcia.

Aplikacja przechwytuje mysz na czas gestu. Utrata przechwycenia, utrata fokusu na rzecz innego okna lub minimalizacja anuluje przeciąganie. Przejście fokusu do przycisków trybu rysowania nie anuluje operacji. Otwarcie pomocy lub dokumentacji anuluje aktywny gest i wybór pary. Okna informacyjne można zamknąć przez Esc, przycisk Zamknij lub polecenie systemowe zamknięcia okna.

## 3. Operacje i ograniczenia

### Zapis i odczyt sceny

**Plik → Zapisz jako...** zapisuje geometrię, uchwyty, wygięcia łuków, grubości krawędzi, relacje oraz ustawienia i źródła ciągłości. Do danych trwałych należą również identyfikatory i liczniki nowych obiektów. Plik nie przechowuje zaznaczenia, stanu gestu, algorytmu renderowania ani parametrów okna.

**Plik → Załaduj...** odczytuje i weryfikuje cały plik przed zastąpieniem dokumentu. Odczyt nie przesuwa punktów i nie naprawia relacji solverem. Nieznana wersja, błędna struktura lub niespełnione ograniczenia powodują odrzucenie pliku z zachowaniem bieżącej sceny. Poprawny odczyt usuwa zaznaczenie i zachowuje wybrany tryb renderowania.

Przed otwarciem dialogu pliku aplikacja kończy gest w ostatnim zaakceptowanym stanie i anuluje wybór pary. Anulowanie dialogu nie wykonuje operacji plikowej. Wczytanie poprawnej sceny zastępuje dotychczasowy dokument bez dodatkowego potwierdzenia niezapisanych zmian.

Zapis odbywa się przez plik tymczasowy w katalogu docelowym. Zastąpienie pliku następuje po zakończeniu zapisu. Format `.cadscene` jest tekstowy i wersjonowany; obowiązują limity 16 MiB oraz 100000 rekordów na sekcję. Szczegóły określa [specyfikacja formatu](scene-format.md).

### Grubość krawędzi

Polecenie **Grubość krawędzi...** w menu PPM ustawia wartość całkowitą od **1 do 32 px** dla wskazanego odcinka, Béziera lub łuku. Domyślna wartość wynosi 2 px. Zaznaczenie i wskazanie pierwszej krawędzi relacji równoległości zwiększają szerokość renderowania o 2 px.

Grubość jest właściwością wyglądu i nie wpływa na współrzędne ani równania ograniczeń. Konwersja zachowuje wartość, obie części podziału ją dziedziczą, a odcinek po usunięciu wierzchołka przejmuje grubość krawędzi poprzedzającej. Pliki wersji 1 otrzymują wartość domyślną. Tolerancja wskazywania nadal odnosi się do geometrycznego obrysu.

### Długość odcinka

**Ustaw stałą długość...** otwiera dialog z aktualną długością. Zatwierdzenie dodatniej wartości dodaje relację i uruchamia solver z początkiem odcinka unieruchomionym w bieżącej pozycji. Pozostałe punkty mogą zmienić położenie, aby spełnić wszystkie aktywne relacje.

Pole liczbowe akceptuje kropkę, przecinek dziesiętny i zapis wykładniczy, np. `12.5`, `12,5`, `1e2`. Odrzuca wartości niedodatnie, NaN, nieskończoności i niepełne zapisy liczbowe. Istniejącą relację można zmodyfikować przez **Zmień stałą długość...** lub usunąć przez **Usuń ograniczenie długości**. Etykieta `L=...` przedstawia wartość zadaną. Usunięcie relacji nie zmienia geometrii.

### Równoległość

**Ustaw równoległość z...** rozpoczyna wybór drugiego odcinka. Pierwszy jest oznaczony kolorem pomarańczowym. Wskazanie drugiego, również z innego wielokąta, dodaje relację po poprawnym rozwiązaniu układu. Esc anuluje wybór. Powtórzona para, wybór tej samej krawędzi i niepoprawne odwołania są odrzucane.

Etykiety `P1`, `P2` itd. identyfikują pary według bieżącej kolejności relacji. Podmenu **Usuń równoległość** usuwa wybraną parę. Relacja dopuszcza przeciwne zwroty odcinków. Jeden odcinek może należeć do wielu par i jednocześnie mieć zadaną długość. Długość i równoległość dotyczą wyłącznie odcinków.

### Zmiany topologii

**Podziel krawędź** dodaje wierzchołek w ustalonym punkcie. Pozycja kliknięcia identyfikuje krawędź, a nie parametr podziału.

| Typ | Wynik |
| --- | --- |
| Odcinek | Dwa odcinki przez środek; nowy wierzchołek G0/Default |
| Bézier | Dwie krzywe dla `t=0.5`; nowy wierzchołek G1/Default |
| Łuk | Dwa odcinki przez punkt łuku dla `t=0.5`; nowy wierzchołek G0/Default |

Podział Béziera zachowuje kształt przed ponownym rozwiązaniem ograniczeń. Każda część ma własny parametr w `[0,1]`, co zmienia długości pochodnych i może wymagać korekty przy utrzymywaniu C1. Podział łuku celowo zmienia kształt przez zastąpienie go odcinkami.

**Usuń wierzchołek** zastępuje dwie sąsiednie krawędzie jednym odcinkiem. Figura musi zachować co najmniej trzy wierzchołki. Relacje zastępowanych krawędzi są usuwane, a ustawienia ciągłości pozostałych wierzchołków zostają zachowane. Wynik jest zatwierdzany po walidacji i rozwiązaniu pozostałych zależności.

## 4. Krzywe i ciągłość

### Krzywa Béziera

**Zamień na Béziera** tworzy krzywą trzeciego stopnia z końcami współdzielonymi z wielokątem i dwoma niezależnymi punktami kontrolnymi. Uchwyty i przerywany wielobok kontrolny umożliwiają edycję kształtu. Początkowo uchwyty leżą w 1/3 i 2/3 cięciwy; solver może dostosować je do ciągłości.

Konwersja nadaje domyślne G1 końcom bez wcześniejszego jawnego ustawienia. Wybór użytkownika, również jawne G0, pozostaje zachowany. Menu **Ciągłość wierzchołka** udostępnia klasy:

| Klasa | Warunek |
| --- | --- |
| G0 | Wspólny punkt końcowy |
| G1 | Zgodne kierunki i zwroty niezerowych stycznych |
| C1 | Równe wektory pochodnych, również co do długości |

Pochodne odnoszą się do lokalnego parametru każdego segmentu w `[0,1]`. C1 między odcinkiem a Bézierem wymaga odległości sąsiedniego uchwytu od końca krzywej równej jednej trzeciej długości odcinka. G1 odrzuca zerowe styczne; C1 może porównywać dwa zerowe wektory.

Przy przesuwaniu uchwytu solver początkowo zachowuje końce jego krzywej. Niezbieżna próba może zostać ponowiona z ruchomymi końcami w pozostałym budżecie iteracji. Prowadzony uchwyt pozostaje dokładnie przypięty do pozycji wynikającej z kursora.

### Łuk okręgu

Łuk dopuszcza G0 i G1, przy czym najwyżej jeden koniec może mieć G1. C1 i G1 na obu końcach uniemożliwiają konwersję. Przy G0/G0 implementacja wymusza półokrąg o promieniu równym połowie cięciwy. Pojedyncze G1 wyznacza wygięcie analitycznie ze stycznej sąsiedniej krawędzi.

Dla dwóch łuków połączonych G1 łuk o niższym ID stanowi odniesienie, a drugi jest dopasowywany. Brak skończonego łuku zgodnego ze styczną powoduje odrzucenie operacji. Końce muszą być różne. Konwersja tworzy dodatnie wygięcie; interfejs nie udostępnia osobnego polecenia zmiany jego znaku.

### Konwersje typów

Menu umożliwia konwersję między odcinkiem, Bézierem i łukiem. Przejście z odcinka na krzywą usuwa relacje długości i równoległości tej krawędzi. Konwersja odwrotna nie odtwarza tych relacji. Nieudana operacja zachowuje wcześniejszą geometrię i ograniczenia.

## 5. Założenia i sytuacje wyjątkowe

- Wielokąty są zamkniętymi cyklami. Samoprzecięcia są dopuszczalne; wnętrze określa reguła parzystości.
- Interfejs edytuje figury ze sceny początkowej i plików, bez tworzenia i usuwania całych wielokątów.
- Translacja przesuwa wszystkie wierzchołki i uchwyty figury o ten sam wektor. Zachowuje długości, kierunki, ciągłość i wygięcia; pozostałe figury nie zmieniają położenia.
- Podział dla `t=0.5` oznacza połowę przedziału parametru, a nie połowę długości krzywej.
- Odrzucona próbka ruchu pozostawia ostatni poprawny podgląd i komunikat. Zwolnienie LPM zatwierdza ten podgląd; Esc odtwarza stan sprzed gestu.
- Brak zbieżności może wynikać z konfliktu, degeneracji lub ograniczeń numerycznych. Nie dowodzi matematycznej sprzeczności. Solver nie usuwa relacji w celu wymuszenia wyniku.
- Jedna jednostka świata odpowiada jednemu pikselowi. Początek układu jest odsunięty o 20 px od lewego i dolnego brzegu sceny; oś Y jest skierowana w górę.
- Obliczenia wykorzystują `double`. Przepełnienia i wartości nieskończone powodują odrzucenie danych lub kandydata. Zakres obejmuje sceny demonstracyjne, bez deklaracji wydajności dla tysięcy obiektów.
- Projekt nie obejmuje pełnej historii undo/redo, skalowania widoku, przyciągania ani dodatkowej relacji prostopadłości. Anulowanie gestu działa niezależnie od historii zmian.

## 6. Model danych i architektura

Architektura rozdziela reprezentację geometrii, operacje edytora i adapter platformy.

| Moduł | Odpowiedzialność |
| --- | --- |
| `cad_core/model` | Własność obiektów, topologia, walidacja i operacje dokumentu |
| `cad_core/geometry` | Wektory, ewaluacja i próbkowanie krzywych |
| `cad_core/constraints`, `cad_core/solver` | Reszty równań i wspólne rozwiązywanie ograniczeń |
| `cad_core/io` | Wersjonowana serializacja przez strumienie |
| `src/editor` | Zaznaczenie, identyfikacja wskazanego elementu i cykl edycji |
| `src/render` | Przenośne przycinanie i rasteryzacja |
| `src/platform/win32`, `Projecr1_GK.cpp` | Okna, zdarzenia, dialogi, bufory i prezentacja |

`Document` przechowuje mapy obiektów i kolekcje relacji. Typy `VertexId`, `EdgeId` i `PolygonId` zawierają 64-bitowe identyfikatory; zero oznacza wartość niepoprawną. Odwołania między encjami wykorzystują ID, bez przechowywania wskaźników do elementów kontenerów.

`Vertex` zawiera pozycję i ciągłość z pochodzeniem `Default` lub `Explicit`. `Edge` określa końce, grubość renderowania i wariant `EdgeGeometry`: `LineSegment`, `CubicBezier` z dwoma uchwytami albo `CircularArc` z parametrem `bulge`. `Polygon` przechowuje uporządkowane listy wierzchołków i krawędzi tworzących cykl.

`EditorSession` jest właścicielem dokumentu, zaznaczenia i stanu gestu. Zmiany geometrii wykonywane są na kopii. Wynik zastępuje dokument po walidacji i rozwiązaniu relacji; migawka z początku gestu umożliwia anulowanie przeciągania. Zmiany wyglądu nie wymagają solvera.

WinAPI przekazuje do sesji pozycje świata i polecenia. Renderer odczytuje model i zaznaczenie. Zastąpienie interfejsu wymaga implementacji wejścia i prezentacji przy zachowaniu rdzenia i sesji. Szczegóły zawiera [opis architektury](architecture/README.md).

## 7. Algorytmy geometryczne i rysowanie

### Ewaluacja i próbkowanie Béziera

Krzywa trzeciego stopnia jest zapisywana w bazie potęgowej:

```text
B(t) = a*t³ + b*t² + c*t + d
a = P3 - P0 + 3*(P1 - P2)
b = 3*(P0 - 2*P1 + P2)
c = 3*(P1 - P0)
d = P0
```

`bezierPoint` wykorzystuje schemat Hornera do ewaluacji pojedynczego punktu. `sampleBezier` stosuje różnice progresywne przy kroku `h=1/N`:

```text
point = P0
Δ1 = a*h³ + b*h² + c*h
Δ2 = 6*a*h³ + 2*b*h²
Δ3 = 6*a*h³

point += Δ1
Δ1 += Δ2
Δ2 += Δ3
```

Trzecia różnica jest stała, dlatego aktualizacja wymaga wyłącznie dodawania. Wynik zawiera `N+1` punktów; dokładne przypisanie końców ogranicza wpływ zaokrągleń. Wspólne `sampleEdge` domyślnie tworzy 100 odcinków na krzywą. Próbki wykorzystują rysowanie i identyfikacja wskazanego elementu. Podział dla `t=0.5` realizuje algorytm de Casteljau.

### Łuki i styczne

Reprezentacja `bulge=tan(theta/4)` pozwala odtworzyć środek, promień i kąt łuku z jego końców. Próbki wynikają z parametryzacji okręgu `C+r*(cos(phi),sin(phi))`. `updateArcBulges` przywraca półokręgi G0/G0 lub dopasowuje pojedyncze G1 do stycznej sąsiada. Wygięcie jest obliczane analitycznie i nie stanowi osobnej niewiadomej solvera.

### Odległość od krawędzi i wnętrze figury

Odległość punktu od odcinka wynika z rzutu ortogonalnego z ograniczeniem parametru:

```text
t = clamp(dot(P-A,B-A)/|B-A|²,0,1)
Q = A+t*(B-A)
distanceSquared = |P-Q|²
```

Odcinek zerowy jest traktowany jak punkt. Dla krzywej analizowane są segmenty łamanej, a wybór dotyczy najbliższej krawędzi w tolerancji. Test wnętrza stosuje poziomy promień i regułę even-odd: nieparzysta liczba przecięć oznacza wnętrze, również przy samoprzecięciach.

### Przycinanie i rasteryzacja

Algorytm **Cohena-Sutherlanda** przycina odcinki w `double`. Kody bitowe opisują położenie końców względem prostokąta i umożliwiają przyjęcie, odrzucenie lub wyznaczenie przecięcia. Konwersja do współrzędnych całkowitych następuje po przycięciu.

Tryb **GDI** korzysta z `MoveToEx` i `LineTo`. Własny **Bresenham** wyznacza piksele przez całkowitoliczbową aktualizację błędu dla wszystkich ośmiu oktantów. Kwadratowy pędzel rozszerza ślad zgodnie z grubością krawędzi.

Tryb **Wu (AA)** zachowuje pozycje ułamkowe. Dla 1 px klasyczny algorytm Xiaolina Wu rozdziela pokrycie pomiędzy sąsiadów z wagami `1-frac(y)` i `frac(y)` oraz korektą końców. Szersze obrysy wykorzystują poszerzenie geometryczne z okrągłymi końcami i przybliżenie pokrycia:

```text
alpha = clamp(width/2 + 0.5 - distanceToSegment,0,1)
colorOut = alpha*colorLine + (1-alpha)*colorBackground
```

Grubość szerszego obrysu jest mierzona prostopadle do segmentu. Maksimum pokryć segmentów jednej krawędzi tworzy zaokrąglone łączenia bez zwężeń przy zmianie oktantu. Dla 1 px sumowane są częściowe przedziały końców. Mieszanie RGB odbywa się bez korekcji gamma. Zerowy segment 1 px ma pokrycie kwadratowe, a szerszy wariant okrągłe.

### Buforowanie i obsługa ruchu

Klatka powstaje w odgórnym, 32-bitowym DIB. Własne algorytmy zapisują `0x00RRGGBB` bez wywoływania `SetPixel` dla każdego punktu. `GdiFlush` kończy operacje tła przed zapisem CPU. Po geometrii rysowane są dekoracje GDI, a `BitBlt` prezentuje kompletną klatkę. Brak bufora powoduje przejście do bezpośredniego rysowania GDI.

Koalescencja ruchu zachowuje najnowszą oczekującą pozycję. Timer o zadanym interwale 1 ms aktualizuje podgląd, ograniczając przeliczanie nieaktualnych próbek. Termin obsługi zależy od systemu i kolejki. Zwolnienie LPM wymusza ocenę końcowej pozycji; błąd utworzenia timera przywraca bezpośrednią obsługę ruchu.

## 8. Mechanizm ograniczeń i solver

### Reszty równań

Każde ograniczenie jest reprezentowane przez resztę, której wartość zerowa oznacza spełnienie warunku. Odcinek długości 10 przy wartości zadanej 12 ma resztę `-2`.

| Relacja | Reszta |
| --- | --- |
| Długość | `length(B-A)-L` |
| Równoległość | `cross(unit(d1),unit(d2))` |
| G1 | `atan2(cross(unit(Tin),unit(Tout)),dot(unit(Tin),unit(Tout)))` |
| C1 | Dwie składowe wektora `Tout-Tin` |

`Tin` i `Tout` oznaczają pochodne krawędzi dochodzącej i wychodzącej. Dla odcinka pochodna wynosi `B-A`, a dla Béziera na końcach `3*(P1-P0)` i `3*(P3-P2)`. Pochodna łuku uwzględnia kąt przebiegu. G1 rozróżnia zwroty stycznych, natomiast równoległość odcinków dopuszcza zwroty przeciwne.

### Metoda rozwiązania

`solvePointConstraintSystem`, wywoływana przez publiczne `solveConstraints`, realizuje tłumioną metodę Gaussa-Newtona z krokiem typu Levenberga-Marquardta i redukcją długości kroku.

1. Przygotowanie kopii dokumentu, dopasowanie łuków i walidacja wejścia.
2. Zebranie relacji i wolnych punktów; prowadzony punkt oraz dodatkowo unieruchomione wierzchołki są wyłączone z niewiadomych.
3. Obliczenie reszt, skali geometrii i numerycznej macierzy Jacobiego.
4. Rozwiązanie gęstego układu liniowego przez eliminację Gaussa z częściowym wyborem elementu głównego:

   ```text
   (J*Jᵀ + λ*I)*y = -Fscaled
   deltaNormalized = Jᵀ*y
   deltaWorld = geometryScale*deltaNormalized
   ```

5. Ocena próbnej geometrii z ponownym dopasowaniem łuków. Redukcja kroku i zmiana tłumienia służą uzyskaniu spadku normy reszt lub spełnienia wszystkich tolerancji.
6. Przy niemal prostopadłych odcinkach dopuszczona jest awaryjna próba ruchu pojedynczej współrzędnej, przyjmowana wyłącznie po zmniejszeniu normy.
7. Kontrola relacji, topologii i dokładnego przypięcia punktów. Dopiero zbieżny wynik zastępuje dokument wejściowy.

`J` oznacza Jacobian, `Fscaled` znormalizowane reszty, `λ` tłumienie, a `I` macierz jednostkową. Wektor `y` jest pomocniczy. Limit iteracji obejmuje zaakceptowane poprawki współrzędnych, nie wszystkie próby ani czas wykonania. Rozwiązanie lokalne nie musi być jedyne ani globalnie najbliższe początkowej geometrii.

### Jacobian i normalizacja

Wiersz odpowiada pojedynczej reszcie, a kolumna współrzędnej wolnego punktu w kolejności `x0,y0,x1,y1,...`. G1 dodaje jeden wiersz, C1 dwa, a G0 żadnego. Pochodne są przybliżane różnicami skończonymi w przód na kopii dokumentu.

Krok wynosi `sqrt(epsilon_double)*geometryScale`. Jeżeli dodanie kroku nie zmienia współrzędnej, `nextafter` wyznacza najbliższą większą reprezentowalną wartość. Obliczenia wykorzystują rzeczywiste przesunięcie. Różnice kątów G1 są sprowadzane modulo `2π`; po perturbacji przywracane są pozycje i wygięcia.

Długości i C1 są normalizowane skalą; równoległość i kąty pozostają bezwymiarowe. Skala uwzględnia cięciwy, uchwyty, zadane długości i strzałki łuków. Grupy zależności ograniczają ocenę reszt do wierszy zależnych od danej współrzędnej, z kontrolą aktualności wektora bazowego.

### Parametry i statusy

| Parametr `SolveOptions` | Wartość domyślna |
| --- | --- |
| `maxIterations` | 10000 zaakceptowanych poprawek |
| `maxFixedEndpointIterations` | 50 poprawek z nieruchomymi końcami Béziera, w ramach wspólnego limitu |
| `lengthTolerance` | `1e-6` w jednostkach świata |
| `parallelTolerance` | `1e-6` dla modułu znormalizowanego iloczynu wektorowego |
| `g1Tolerance` | `1e-6` radiana |
| `c1Tolerance` | `1e-6` dla każdej składowej różnicy pochodnych |

Dopasowanie łuków nie zużywa iteracji korekty współrzędnych. Próby z nieruchomymi i ruchomymi końcami Béziera dzielą wspólny limit. `SolveResult` zawiera liczbę poprawek i status `Converged`, `InvalidInput`, `NoConvergence` albo `DegenerateGeometry`. Niezbieżna próba nie zmienia dokumentu wejściowego. Dodatkowa ścieżka `solveWithJacobian` umożliwia testowanie z zadanymi macierzami.

## 9. Budowanie, testy i przekazanie programu

Wersja wykonywalna dla Windows 10/11 x64 znajduje się w [dist/windows-x64/Projecr1_GK.exe](../dist/windows-x64/Projecr1_GK.exe). Biblioteki C++ są dołączone statycznie w Release (`/MT`). Uruchomienie wymaga EXE i bibliotek systemowych Windows, bez osobnego pakietu Visual C++ Redistributable. Pomoc, dokumentacja i ikony są osadzone w zasobach EXE, a scena początkowa powstaje w kodzie programu.

Budowanie wymaga Visual Studio z narzędziami aplikacji klasycznych C++, zestawu **v145**, Windows SDK 10 i MSBuild obsługującego `.slnx`. Dostępne konfiguracje to Debug/Release i x64/Win32. Polecenia w Developer PowerShell dla Visual Studio:

```powershell
msbuild .\Projecr1_GK.slnx /m /p:Configuration=Release /p:Platform=x64
.\x64\Release\Projecr1_GK.exe
```

Skrypt testowy odnajduje MSBuild, buduje oba zestawy i uruchamia je kolejno:

```powershell
.\tests\run-tests.cmd -Configuration Debug
.\tests\run-tests.cmd -Configuration Release
```

Zestaw przenośny zawiera **760 przypadków w 40 zestawach**, a zestaw Windows **49 przypadków integracyjnych**. Weryfikacja obejmuje model, geometrię, serializację, solver, edycję i rasteryzację oraz rzeczywistą procedurę okna, dialogi, zasoby i DIB. Kontrole pozostają aktywne w Release. Zakres określa [dokumentacja testów](../tests/README.md).

Aktualizacja dystrybucji odbywa się poleceniem:

```powershell
.\scripts\build-release.cmd
```

Skrypt buduje Release x64, kopiuje EXE do `dist/windows-x64/` i aktualizuje `SHA256SUMS.txt`. Nie wykonuje commitów ani operacji push. Zmianie źródeł lub dokumentacji dołączanej do zasobów powinna towarzyszyć aktualizacja dystrybucji.

Materiał projektu obejmuje źródła, projekty Visual Studio, zasoby graficzne i `.rc`, dokumentację, testy oraz wersję wykonywalną. Pliki `.cadscene` są niezależnymi dokumentami użytkownika. Release jest konfiguracją przeznaczoną do prezentacji i oceny wydajności.

## 10. Scenariusz prezentacji

| Etap | Funkcjonalność | Oczekiwany rezultat |
| --- | --- | --- |
| 1 | Scena demonstracyjna | Dwie figury, długości 180/120/160 i pary P1/P2 |
| 2 | Ruch uchwytów Béziera i anulowanie przez Esc | Zachowanie C1/G1 i odtworzenie stanu sprzed gestu |
| 3 | Edycja końca łuku i wybór ciągłości | Zachowanie G1; brak C1 przy łuku |
| 4 | Translacja figury | Zachowanie kształtu i relacji bez ruchu drugiego obiektu |
| 5 | Zmiana długości i edycja równoległości | Aktualizacja geometrii i oznaczeń relacji |
| 6 | GDI/Bresenham/Wu i grubość jednej krawędzi | Zmiana prezentacji bez zmiany geometrii |
| 7 | Podział Béziera i łuku, usunięcie wierzchołka | Wynik zgodny z regułami zmian topologii |
| 8 | Zapis i odczyt sceny | Odtworzenie geometrii, grubości i relacji |
| 9 | Pomoc i dokumentacja | Niezależne okna z treścią dostępną bez plików zewnętrznych |

Operacje mogą zmieniać stan wykorzystywany w kolejnych etapach. Ponowne uruchomienie odtwarza scenę demonstracyjną, a wczytanie pliku umożliwia powrót do zapisanego wariantu.
