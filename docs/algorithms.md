# Algorytmy geometryczne, numeryczne i renderujące

Dokument przedstawia metody zastosowane w implementacji edytora wielokątów. Każdy opis obejmuje lokalizację w kodzie, zastosowanie, pseudokod oraz zasadę działania. Uwzględniono przyjęte uproszczenia i różnice między algorytmem klasycznym a jego rozszerzeniem w projekcie.

Pseudokod przedstawia zasadniczy przebieg obliczeń. Pomija część kontroli poprawności identyfikatorów, alokacji i wartości skończonych, które występują w C++. Zapis `brak` oznacza odrzucenie operacji lub brak wyniku, zależnie od funkcji.

| Zapis | Znaczenie |
| --- | --- |
| `A`, `B`, `P`, `Q` | Punkty albo ich wektory położenia w 2D |
| `dot(u,v)` | Iloczyn skalarny: `ux*vx + uy*vy` |
| `cross(u,v)` | Iloczyn wektorowy 2D, czyli liczba `ux*vy - uy*vx` |
| `length(v)` | Długość wektora, w kodzie często liczona przez `hypot` |
| `perp(v)` | Obrót wektora o 90°: `(-vy, vx)` |
| `clamp(t,0,1)` | Ograniczenie liczby do przedziału `[0,1]` |
| `F` | Wektor błędów ograniczeń przed normalizacją |
| `r`, `J`, `s` | Znormalizowane błędy, ich Jacobian i skala geometrii |
| `<-` | Przypisanie wartości w pseudokodzie |

## Mapa algorytmów

| Nr | Algorytm lub mechanizm | Główna funkcja | Plik względem katalogu repozytorium |
| --- | --- | --- | --- |
| 1 | Schemat Hornera | `bezierPoint` | `cad_core/src/geometry/Bezier.cpp` |
| 2 | Różnice progresywne | `sampleBezier` | `cad_core/src/geometry/Bezier.cpp` |
| 3 | de Casteljau | `splitBezierHalf` | `cad_core/src/geometry/Bezier.cpp` |
| 4 | Parametryzacja łuku przez bulge | `computeArcParameters`, `sampleArc` | `cad_core/src/geometry/Arc.cpp` |
| 5 | Analityczne dopasowanie łuku do stycznej | `bulgeFromTangent`, `updateArcBulges` | `cad_core/src/geometry/Arc.cpp`, `cad_core/src/model/ArcContinuity.cpp` |
| 6 | Wyznaczanie stycznych i błędów relacji | `edgeStartTangent`, `continuityErrors`, `evaluateErrors` | `cad_core/src/model/EdgeTangents.cpp`, `cad_core/src/constraints/*`, `cad_core/src/solver/ConstraintSolver.cpp` |
| 7 | Rzut ortogonalny na odcinek | `pointSegmentDistanceSquared` | `src/editor/HitTester.cpp` |
| 8 | Wyszukiwanie najbliższej krawędzi | `hitTestEdge` | `src/editor/HitTester.cpp` |
| 9 | Ray casting, reguła even-odd | `pointInPolygon` | `src/editor/HitTester.cpp` |
| 10 | Cohen–Sutherland | `clipLineToViewport` | `src/render/LineClipping.cpp` |
| 11 | Bresenham | `rasterizeLine` | `src/render/Bresenham.cpp` |
| 12 | Kwadratowy pędzel i buforowanie klatki | `drawBresenhamLine`, `drawDocument`, `BackBuffer::present` | `src/platform/win32/DibRenderer.cpp`, `GdiRenderer.cpp`, `BackBuffer.cpp` |
| 13 | Jacobian przez różnice skończone w przód | `computePointJacobian` | `cad_core/src/solver/Jacobian.cpp` |
| 14 | Eliminacja Gaussa z częściowym wyborem elementu głównego | `solveLinearSystem` | `cad_core/src/solver/DenseLinearAlgebra.cpp` |
| 15 | Tłumiony krok Gaussa-Newtona, typu Levenberga-Marquardta | `dampedCorrection` | `cad_core/src/solver/ConstraintSolver.cpp` |
| 16 | Backtracking i pętla solvera | `solvePointConstraintSystem` | `cad_core/src/solver/ConstraintSolver.cpp` |
| 17 | Awaryjne przeszukiwanie współrzędnych | `tryPerpendicularPointStep` | `cad_core/src/solver/ConstraintSolver.cpp` |
| 18 | Koalescencja ruchu myszy | `WndProc`: `WM_MOUSEMOVE`, `WM_TIMER`, `WM_LBUTTONUP` | `Projecr1_GK.cpp` |
| 19 | Xiaolin Wu - antyaliasing | `rasterizeWuLine`, `drawWuPolyline` | `src/render/Wu.cpp`, `src/platform/win32/DibRenderer.cpp` |

Przykładowy przebieg rysowania krzywej:

```text
krawędź modelu -> sampleEdge -> punkty łamanej -> współrzędne ekranowe
    -> Cohen–Sutherland -> GDI / Bresenham / Wu (AA) -> gotowa klatka w buforze
```

Przykładowy przebieg przeciągania uchwytu:

```text
próbka kursora -> próba zmiany dokumentu -> błędy ograniczeń
    -> Jacobian -> tłumiona poprawka -> próby skrócenia kroku
    -> sprawdzenie zbieżności -> zatwierdzenie albo odrzucenie próby
```

## 1. Schemat Hornera

**Lokalizacja w kodzie:** [Bezier.cpp](../cad_core/src/geometry/Bezier.cpp), funkcja `cad::bezierPoint`.

**Zastosowanie:** obliczenie jednego punktu sześciennej krzywej Béziera dla parametru `t`. Obecnie funkcja jest wykorzystywana przez testy; rysowanie pobiera całą serię punktów z `sampleBezier`.

**Pseudokod:**

```text
bezierPoint(P0, P1, P2, P3, t):
    jeśli t == 0: zwróć P0
    jeśli t == 1: zwróć P3

    a <- P3 - P0 + 3*(P1-P2)
    b <- 3*(P0 - 2*P1 + P2)
    c <- 3*(P1-P0)
    współczynniki <- [a, b, c, P0]

    wynik <- współczynniki[0]
    dla i od 1 do 3:
        wynik <- wynik*t + współczynniki[i]
    zwróć wynik
```

**Zasada działania:** Krzywa jest przekształcana do bazy potęgowej `B(t)=a*t³+b*t²+c*t+P0`, a następnie obliczana w postaci `((a*t+b)*t+c)*t+P0`. Schemat Hornera ogranicza liczbę mnożeń i eliminuje osobne wyznaczanie potęg. Operacje na wektorach obliczają obie współrzędne jednocześnie. Funkcja służy obecnie do ewaluacji pojedynczych punktów w testach; renderer korzysta z próbkowania całej krzywej.

## 2. Różnice progresywne - forward differencing

**Lokalizacja w kodzie:** [Bezier.cpp](../cad_core/src/geometry/Bezier.cpp), `cad::sampleBezier`. Wywoływana przez [EdgeSampling.cpp](../cad_core/src/model/EdgeSampling.cpp), `sampleEdge`.

**Zastosowanie:** szybkie utworzenie łamanej przybliżającej Béziera. Korzystają z niej renderer, hit test krawędzi i sprawdzanie wnętrza figury.

**Pseudokod:**

```text
sampleBezier(P0, P1, P2, P3, N):
    wymagaj N > 0
    oblicz a, b, c jak w schemacie Hornera
    h <- 1/N

    punkt <- P0
    d1 <- a*h³ + b*h² + c*h
    d2 <- 6*a*h³ + 2*b*h²
    d3 <- 6*a*h³
    punkty <- [P0]

    powtórz N razy:
        punkt <- punkt + d1
        d1 <- d1 + d2
        d2 <- d2 + d3
        dopisz punkt do punkty

    ostatni punkt <- P3
    zwróć punkty
```

**Zasada działania:** Próbki odpowiadają parametrom `0,h,2h,...,1`. Dla wielomianu trzeciego stopnia trzecia różnica jest stała. Po inicjalizacji punkt i różnice kolejnych rzędów są aktualizowane przez dodawanie. Kolejność aktualizacji musi zachować poprzednie wartości niższych różnic. Pierwsza i ostatnia próbka są ustawiane dokładnie na końcach krzywej. Stały krok parametru nie oznacza jednakowej odległości między punktami ani adaptacyjnej kontroli błędu łamanej.

## 3. Algorytm de Casteljau

**Lokalizacja w kodzie:** [Bezier.cpp](../cad_core/src/geometry/Bezier.cpp), `cad::splitBezierHalf`. Używany w [Document.cpp](../cad_core/src/model/Document.cpp), `Document::splitEdge`.

**Zastosowanie:** podział Béziera w połowie parametru na dwa Béziery, z zachowaniem kształtu.

**Pseudokod:**

```text
splitBezierHalf(P0, P1, P2, P3):
    A <- środek(P0,P1)
    B <- środek(P1,P2)
    C <- środek(P2,P3)
    D <- środek(A,B)
    E <- środek(B,C)
    M <- środek(D,E)

    lewa <- [P0,A,D,M]
    prawa <- [M,E,C,P3]
    zwróć lewa, prawa
```

**Zasada działania:** Kolejne poziomy interpolacji liniowej tworzą schemat de Casteljau. Ostatni punkt `M` jest punktem krzywej dla `t=0,5`, a boczne krawędzie schematu wyznaczają punkty kontrolne obu połówek. Połówki odtwarzają pierwotny kształt z dokładnością obliczeń zmiennoprzecinkowych. Operacja modelu wstawia nowy wierzchołek i uchwyty; późniejsza korekta ograniczeń może zmienić wynik, zwłaszcza przy C1 zależnym od lokalnej parametryzacji.

## 4. Parametryzacja i próbkowanie łuku przez bulge

**Lokalizacja w kodzie:** [Arc.cpp](../cad_core/src/geometry/Arc.cpp), `cad::computeArcParameters`, `cad::sampleArc`.

**Zastosowanie:** odtworzenie okręgu i punktów łuku na podstawie końców oraz parametru `bulge`. Próbki trafiają przez `sampleEdge` do rysowania i hit testów.

**Pseudokod:**

```text
computeArcParameters(A, B, bulge):
    odrzuć A == B oraz bulge == 0
    M <- środek(A,B)
    H <- M-A
    C <- M + perp(H)*(0.5/bulge - 0.5*bulge)
    r <- length(A-C)
    phi0 <- atan2((A-C).y, (A-C).x)
    theta <- 4*atan(bulge)
    zwróć C, r, phi0, theta

sampleArc(A, B, bulge, N):
    wymagaj N > 0
    C, r, phi0, theta <- computeArcParameters(A,B,bulge)
    dla i od 0 do N:
        phi <- phi0 + theta*(i/N)
        dopisz C + r*(cos(phi),sin(phi))
    pierwszy punkt <- A
    ostatni punkt <- B
    zwróć punkty
```

**Zasada działania:** Parametr `bulge=tan(θ/4)` koduje skierowany kąt łuku; moduł równy 1 oznacza półokrąg. Środek leży na symetralnej cięciwy, a jego przesunięcie wynika z wygięcia. Po wyznaczeniu środka i promienia próbki oblicza parametryzacja kołowa. Znak kąta określa zwrot. Dokładne końce zastępują wartości uzyskane z funkcji trygonometrycznych. Zbieżne końce, zerowe wygięcie oraz nieprzedstawialne parametry są odrzucane.

## 5. Analityczne dopasowanie łuku do stycznej

**Lokalizacja w kodzie:** [Arc.cpp](../cad_core/src/geometry/Arc.cpp), prywatne `bulgeFromTangent` oraz publiczne `arcBulgeFromStartTangent` i `arcBulgeFromEndTangent`. Integracja: [ArcContinuity.cpp](../cad_core/src/model/ArcContinuity.cpp), `cad::updateArcBulges`.

**Zastosowanie:** utrzymanie G1 przy końcu łuku bez dodawania `bulge` jako niewiadomej numerycznego solvera.

**Pseudokod:**

```text
bulgeFromTangent(A, B, T, atStart):
    D <- kierunek B-A po dodatnim przeskalowaniu
    U <- kierunek T po dodatnim przeskalowaniu
    jeśli atStart: sine <- cross(U,D)
    inaczej:       sine <- cross(D,U)
    cosine <- dot(U,D)
    jeśli sine == 0: zwróć brak

    alpha <- atan2(sine,cosine)
    jeśli cosine < 0:
        b <- (hypot(sine,cosine)-cosine)/sine
    inaczej:
        b <- tan(alpha/2)
    sprawdź, czy b opisuje poprawny łuk
    zwróć b

updateArcBulges(document):
    dla każdego łuku:
        jeśli oba końce mają G0:
            proponuj bulge = +1 albo -1, zachowując dotychczasowy znak
        jeśli dokładnie jeden koniec ma G1:
            pobierz styczną odpowiedniej krawędzi sąsiedniej
            dopasuj bulgeFromTangent
        odrzuć niedozwolone ustawienia lub niepoprawne wyniki
    zatwierdź wszystkie propozycje razem na kopii dokumentu
```

**Zasada działania:** Zależność między styczną a cięciwą pozwala wyznaczyć wygięcie bez iteracyjnego rozwiązywania dodatkowej zmiennej. W przyjętej orientacji odpowiedni kąt `α` prowadzi do `bulge=tan(α/2)`. Skalowanie kierunków ogranicza ryzyko przepełnienia, a równoważna tożsamość trygonometryczna poprawia obliczenia dla dużych łuków. Przy G0/G0 zachowywany jest znak półokręgu; przy pojedynczym G1 wygięcie wynika z sąsiada. C1 i G1/G1 są niedopuszczalne. Dopasowanie jest ponawiane przy perturbacjach Jacobiego oraz ocenie prób solvera.

## 6. Wyznaczanie stycznych i błędów ograniczeń

**Lokalizacja w kodzie:** [EdgeTangents.cpp](../cad_core/src/model/EdgeTangents.cpp), `edgeStartTangent`, `edgeEndTangent`; [FixedLengthConstraint.cpp](../cad_core/src/constraints/FixedLengthConstraint.cpp), `fixedLengthError`; [ParallelConstraint.cpp](../cad_core/src/constraints/ParallelConstraint.cpp), `parallelError`; [ContinuityConstraint.cpp](../cad_core/src/constraints/ContinuityConstraint.cpp), `continuityErrors`. Wektor błędów składa `evaluateErrors` w [ConstraintSolver.cpp](../cad_core/src/solver/ConstraintSolver.cpp).

**Zastosowanie:** reprezentacja warunków geometrycznych jako reszt równań dla solvera. Wartość zerowa oznacza spełnienie równania, a ocena numeryczna wykorzystuje odpowiednią tolerancję.

**Pseudokod:**

```text
styczne krawędzi względem t z [0,1]:
    odcinek: początek i koniec = B-A
    Bézier:  początek = 3*(P1-P0), koniec = 3*(P3-P2)
    łuk:     styczna w P = theta*perp(P-C)

błąd długości:
    zwróć length(B-A)-L

błąd równoległości:
    zwróć cross(unit(B1-A1),unit(B2-A2))

błędy ciągłości(Tin,Tout):
    G0: zwróć pustą listę
    G1: zwróć [atan2(cross(unit(Tin),unit(Tout)), dot(unit(Tin),unit(Tout)))]
    C1: zwróć [(Tout-Tin).x, (Tout-Tin).y]

evaluateErrors(document):
    dopisz błędy długości w kolejności relacji
    dopisz błędy równoległości w kolejności relacji
    dopisz błędy ciągłości w kolejności zebranych połączeń
```

**Zasada działania:** Reszty przekształcają wymagania geometryczne w składowe wektora równań. Długość wykorzystuje różnicę ze znakiem: aktualne 10 przy zadanym 12 daje `-2`. Równoległość jest mierzona znormalizowanym iloczynem wektorowym, dlatego dopuszcza przeciwne zwroty odcinków. G1 mierzy skierowany kąt między niezerowymi stycznymi i odróżnia zgodny zwrot od przeciwnego. C1 porównuje obie składowe pełnych pochodnych dla lokalnego parametru `[0,1]`. Zbieżność jest oceniana osobnymi tolerancjami, a nie dokładną równością zmiennoprzecinkową.

## 7. Rzut ortogonalny i odległość punktu od odcinka

**Lokalizacja w kodzie:** [HitTester.cpp](../src/editor/HitTester.cpp), `cad::editor::pointSegmentDistanceSquared`.

**Zastosowanie:** pomiar odległości kursora od jednego odcinka. Funkcja nie zaznacza krawędzi; dostarcza wynik dla `hitTestEdge`.

**Pseudokod:**

```text
pointSegmentDistanceSquared(P,A,B):
    v <- B-A
    w <- P-A
    len2 <- dot(v,v)
    jeśli len2 == 0:
        zwróć dot(w,w)

    t <- clamp(dot(w,v)/len2,0,1)
    Q <- A+t*v
    d <- P-Q
    zwróć dot(d,d)
```

**Zasada działania:** Punkt prostej ma postać `Q=A+t*v`. Warunek prostopadłości `dot(P-Q,v)=0` prowadzi do `t=dot(P-A,v)/dot(v,v)`. Ograniczenie `t` do `[0,1]` uwzględnia końce odcinka: jeżeli rzut leży poza nim, najbliższym punktem jest odpowiedni koniec. Wynikiem jest kwadrat odległości, co eliminuje pierwiastkowanie przy porównywaniu kandydatów. Odcinek zbieżny do punktu jest obsługiwany osobno, bez dzielenia przez zero.

## 8. Wyszukiwanie najbliższej krawędzi - hit testing

**Lokalizacja w kodzie:** [HitTester.cpp](../src/editor/HitTester.cpp), `hitTestEdge`. Zaznaczenie zapisuje `EditorSession::selectAt` w [EditorSession.cpp](../src/editor/EditorSession.cpp).

**Zastosowanie:** ustalenie, którą krawędź wskazuje kursor, jeśli znajduje się dostatecznie blisko jej wyświetlanego kształtu.

**Pseudokod:**

```text
hitTestEdge(document,P,tolerance):
    jeśli tolerance < 0: zwróć brak
    najbliższa <- brak
    najlepszaOdległość2 <- tolerance²

    dla każdego wielokąta i każdej jego krawędzi:
        punkty <- sampleEdge(geometria,początek,koniec)
        dla każdej pary kolejnych punktów A,B:
            d2 <- pointSegmentDistanceSquared(P,A,B)
            jeśli d2 <= tolerance² oraz (brak wyniku lub d2 < najlepszaOdległość2):
                najbliższa <- identyfikator krawędzi
                najlepszaOdległość2 <- d2
    zwróć najbliższa
```

**Zasada działania:** Algorytm liniowo przegląda segmenty wszystkich obrysów i zachowuje najmniejszy kwadrat odległości mieszczący się w tolerancji. Odcinek modelu tworzy jeden segment, a krzywa wiele segmentów z `sampleEdge`. Remis zachowuje wcześniej znalezioną krawędź. Test zwraca identyfikator; stan zaznaczenia zapisuje sesja edytora. Wspólne próbkowanie sprawia, że wybór odpowiada wyświetlanej łamanej, a nie cięciwie krzywej.

## 9. Ray casting z regułą parzystości — even–odd

**Lokalizacja w kodzie:** [HitTester.cpp](../src/editor/HitTester.cpp), `pointInPolygon` i wywołująca ją `hitTestPolygon`.

**Zastosowanie:** wykrywanie kliknięcia we wnętrzu figury, aby można było zaznaczyć i przesuwać cały wielokąt.

**Pseudokod:**

```text
pointInPolygon(document,polygon,P):
    inside <- false
    dla każdej krawędzi obrysu:
        punkty <- sampleEdge(krawędź)
        dla każdej pary kolejnych punktów A,B:
            jeśli (A.y > P.y) != (B.y > P.y):
                crossingX <- A.x + (P.y-A.y)*(B.x-A.x)/(B.y-A.y)
                jeśli crossingX > P.x:
                    inside <- nie inside
    zwróć inside
```

**Zasada działania:** Poziomy promień wychodzący z badanego punktu przecina obrys. Każde przecięcie zmienia parzystość: nieparzysta liczba oznacza wnętrze. Półotwarty warunek dla zakresu Y zapobiega podwójnemu zliczeniu wspólnego wierzchołka; segmenty poziome nie wyznaczają przecięcia. Reguła określa wnętrze również dla samoprzecięć. Test nie zawiera tolerancji brzegowej, ponieważ wybór krawędzi odbywa się wcześniej. Przy nakładaniu figur `hitTestPolygon` sprawdza je w odwrotnej kolejności mapy ID.

## 10. Algorytm Cohena–Sutherlanda

**Lokalizacja w kodzie:** [LineClipping.cpp](../src/render/LineClipping.cpp), `cad::render::clipLineToViewport`. Wywołania znajdują się w [DibRenderer.cpp](../src/platform/win32/DibRenderer.cpp) i [GdiRenderer.cpp](../src/platform/win32/GdiRenderer.cpp).

**Zastosowanie:** odrzucenie niewidocznego odcinka albo przycięcie go do prostokąta sceny przed rasteryzacją i konwersją współrzędnych na `int`.

**Pseudokod:**

```text
code(P):
    kod <- 0
    jeśli P.x < xmin: ustaw bit LEFT
    jeśli P.x > xmax: ustaw bit RIGHT
    jeśli P.y < ymin: ustaw bit TOP
    jeśli P.y > ymax: ustaw bit BOTTOM
    zwróć kod

clip(A,B):
    ca <- code(A)
    cb <- code(B)
    powtarzaj:
        jeśli (ca OR cb) == 0: zwróć odcinek A,B
        jeśli (ca AND cb) != 0: zwróć brak

        wybierz zewnętrzny koniec: A, jeśli ca != 0, inaczej B
        wybierz granicę według jego bitów: TOP, BOTTOM, RIGHT, LEFT
        Q <- przecięcie prostej AB z tą granicą
        odrzuć Q zawierające NaN lub nieskończoność
        zastąp wybrany koniec punktem Q i przelicz jego kod
```

**Zasada działania:** Czterobitowe kody opisują położenie końców względem prostokąta. Zerowa suma bitowa OR oznacza przyjęcie odcinka, a niezerowa część wspólna AND jego odrzucenie. W pozostałych przypadkach zewnętrzny koniec jest zastępowany przecięciem z odpowiednią granicą i kod jest obliczany ponownie. Obliczenia poprzedzają konwersję na `int`. Granice lokalnego widoku odpowiadają środkom skrajnych pikseli. Niepoprawne lub przepełnione obliczenie przecięcia powoduje odrzucenie.

## 11. Algorytm Bresenhama

**Lokalizacja w kodzie:** [Bresenham.cpp](../src/render/Bresenham.cpp), `cad::render::rasterizeLine`.

**Zastosowanie:** wyznaczenie pikseli odcinka w trybie własnej rasteryzacji. Te same odcinki tworzą też przybliżenia krzywych.

**Pseudokod:**

```text
rasterizeLine(A,B):
    x,y <- A.x,A.y
    dx <- abs(B.x-A.x)
    dy <- -abs(B.y-A.y)
    sx <- jeśli A.x < B.x to 1, inaczej -1
    sy <- jeśli A.y < B.y to 1, inaczej -1
    error <- dx+dy

    powtarzaj:
        dopisz piksel (x,y)
        jeśli (x,y) == B: zakończ
        e2 <- 2*error
        jeśli e2 >= dy:
            error <- error+dy
            x <- x+sx
        jeśli e2 <= dx:
            error <- error+dx
            y <- y+sy
```

**Zasada działania:** Całkowitoliczbowa zmienna błędu określa, czy kolejny krok zmieni X, Y czy obie współrzędne. Znaki przyrostów zapewniają obsługę wszystkich ośmiu oktantów. Podwojony błąd jest zapamiętywany przed aktualizacjami, aby obie decyzje dotyczyły tego samego punktu. Implementacja uwzględnia oba końce, odcinek zerowy oraz arytmetykę 64-bitową dla różnic. Algorytm wyznacza ślad jednopikselowy; poszerzenie realizuje renderer.

## 12. Kwadratowy pędzel i buforowanie klatki

**Lokalizacja w kodzie:** [DibRenderer.cpp](../src/platform/win32/DibRenderer.cpp), `drawBresenhamLine`; [GdiRenderer.cpp](../src/platform/win32/GdiRenderer.cpp), `drawDocument`; [BackBuffer.cpp](../src/platform/win32/BackBuffer.cpp), `BackBuffer::resize`, `BackBuffer::present`; [Projecr1_GK.cpp](../Projecr1_GK.cpp), obsługa `WM_PAINT`.

**Zastosowanie:** nadanie grubości śladowi Bresenhama i pokazanie kompletnej klatki bez rysowania kolejnych etapów bezpośrednio na ekranie. To mechanizmy renderowania, nie kolejne warianty Bresenhama.

**Pseudokod:**

```text
drawBresenhamLine(A,B,kolor,grubość):
    przytnij A,B do obszaru sceny w double
    zaokrąglij poprawne końce do pikseli
    ślad <- rasterizeLine(A,B)
    dla każdego piksela (x,y) śladu:
        left <- x - floor(grubość/2)
        top <- y - floor(grubość/2)
        kwadrat <- [left,left+grubość) x [top,top+grubość)
        przytnij kwadrat do sceny
        wypełnij jego piksele kolorem pod indeksem y*width+x

rysowanie klatki:
    przygotuj lub ponownie wykorzystaj bufor DIB
    narysuj tło
    jeśli wybrano Bresenhama i bufor jest dostępny:
        GdiFlush()
        narysuj całą geometrię przez zapis do pikseli DIB
    inaczej:
        narysuj geometrię przez MoveToEx/LineTo
    narysuj przez GDI uchwyty, etykiety i pozostałe nakładki
    pokaż gotowy bufor przez BitBlt
```

**Zasada działania:** Kwadratowy pędzel rozszerza każdy punkt śladu Bresenhama do obszaru o zadanej szerokości. Zapisy są ograniczone do obszaru sceny. Taki obrys może zmieniać pozorną grubość wraz z kątem; tryb AA wykorzystuje inną konstrukcję geometryczną. Cała klatka powstaje w pamięciowym DIB, a `BitBlt` prezentuje ją po zakończeniu rysowania. `GdiFlush` poprzedza bezpośredni zapis pikseli po operacjach GDI. Przy niedostępności bufora stosowane jest rysowanie GDI.

## 13. Jacobian przez różnice skończone w przód

**Lokalizacja w kodzie:** [Jacobian.cpp](../cad_core/src/solver/Jacobian.cpp), przeciążenie `computePointJacobian` przyjmujące połączenia ciągłości. Funkcje `computeJacobian` i `computeConstraintJacobian` korzystają z tej implementacji przez prostsze interfejsy.

**Zastosowanie:** przybliżenie pochodnych błędów względem współrzędnych wolnych punktów. Jacobian mówi, jak lokalnie zmieni się każde równanie po małym ruchu konkretnej współrzędnej.

**Pseudokod:**

```text
computePointJacobian(document,variables,F,s):
    h <- sqrt(epsilon_double)*s
    probe <- kopia dokumentu
    dopasuj łuki w probe i zapamiętaj bazowe bulge
    J <- macierz zer: liczba błędów x (2*liczba punktów)

    dla każdego punktu variables i osi X,Y:
        z <- pierwotna współrzędna
        shifted <- z+h
        jeśli shifted == z: shifted <- nextafter(z,+nieskończoność)
        actualH <- shifted-z
        ustaw shifted w probe i ponownie dopasuj łuki

        dla zależnych wierszy błędu:
            difference <- błąd(probe)-F[wiersz]
            jeśli G1: sprowadź difference modulo 2*pi do [-pi,pi]
            jeśli długość lub C1:
                J[wiersz,kolumna] <- difference/actualH
            inaczej:
                J[wiersz,kolumna] <- difference/(actualH/s)

        odtwórz pierwotną pozycję punktu i bazowe bulge
    zwróć J
```

**Zasada działania:** Każda kolumna odpowiada niezależnej perturbacji jednej współrzędnej. Wiersze obejmują długości, równoległości i ciągłość; G0 nie dodaje wiersza, G1 dodaje jeden, a C1 dwa. Dla zmiennych `q=position/s` długość i C1 są również dzielone przez skalę, więc oba czynniki skracają się w ilorazie różnicowym. Reszty bezwymiarowe są dzielone przez `actualH/s`. `nextafter` zapewnia reprezentowalną zmianę współrzędnej, a różnica kąta G1 jest zawijana modulo `2π`. Po każdej próbie odtwarzane są pozycja i bazowe wygięcia łuków. Grupy zależności ograniczają liczbę ocenianych reszt; nieaktualna baza wymusza pełne przeliczenie.

## 14. Eliminacja Gaussa z częściowym wyborem elementu głównego

**Lokalizacja w kodzie:** [DenseLinearAlgebra.cpp](../cad_core/src/solver/DenseLinearAlgebra.cpp), `solveLinearSystem`.

**Zastosowanie:** rozwiązanie gęstego, kwadratowego układu liniowego `A*x=b`. W solverze służy do znalezienia pomocniczego wektora `y` dla tłumionej poprawki.

**Pseudokod:**

```text
solveLinearSystem(A,b):
    n <- liczba wierszy
    sprawdź wymiary i skończoność danych
    tolerance <- max(abs(elementy A))*epsilon_double*n

    dla kolumny k od 0 do n-1:
        pivot <- indeks największego abs(A[i,k]) dla i >= k
        jeśli abs(A[pivot,k]) <= tolerance: zwróć brak
        zamień wiersze k,pivot w A oraz w b
        dla wiersza i od k+1 do n-1:
            factor <- A[i,k]/A[k,k]
            A[i,k] <- 0
            dla j od k+1 do n-1:
                A[i,j] <- A[i,j]-factor*A[k,j]
            b[i] <- b[i]-factor*b[k]

    dla i od n-1 w dół do 0:
        x[i] <- (b[i]-suma(A[i,j]*x[j] dla j>i))/A[i,i]
    zwróć x
```

**Zasada działania:** Eliminacja zeruje elementy pod przekątną, zachowując rozwiązanie przez równoważne operacje na wierszach. Następnie podstawianie wsteczne wyznacza niewiadome od ostatniego wiersza. Częściowy wybór elementu głównego zamienia wiersze tak, aby dzielnikiem był element o największym module w bieżącej kolumnie. Próg numerycznego zera zależy od skali macierzy. Niepowodzenie może wynikać z osobliwości, uwarunkowania lub niepoprawnych danych; nie jest samodzielnym dowodem sprzeczności geometrii.

## 15. Tłumiony krok Gaussa–Newtona, typu Levenberga–Marquardta

**Lokalizacja w kodzie:** [ConstraintSolver.cpp](../cad_core/src/solver/ConstraintSolver.cpp), `dampedCorrection`. Pomocnicze funkcje `geometryScale` i `scaledErrorNorm` przygotowują skalę oraz miarę błędu.

**Zastosowanie:** wyznaczenie proponowanych przesunięć punktów na podstawie macierzy Jacobiego i aktualnych reszt. Obliczenie poprawki poprzedza ocenę próbnej geometrii i zatwierdzenie dokumentu.

**Pseudokod:**

```text
dampedCorrection(J,F,kinds,s,lambda):
    dla każdego wiersza i:
        jeśli długość albo C1: r[i] <- F[i]/s
        inaczej:              r[i] <- F[i]

    A <- J*transpose(J) + lambda*I
    y <- solveLinearSystem(A,-r)
    jeśli brak y: zwróć brak
    delta <- transpose(J)*y
    zwróć delta
```

```text
||J*delta+r||² + lambda*||delta||²
```

**Zasada działania:** Linearyzacja `r(q+δ)≈r(q)+J*δ` prowadzi do minimalizacji `||J*δ+r||²+λ*||δ||²`. Dodatnie tłumienie ogranicza wielkość poprawki i stabilizuje układ także przy zależnych równaniach. Implementacja używa równoważnej postaci dualnej: `(J*Jᵀ+λ*I)*y=-r`, następnie `δ=Jᵀ*y`. Rozmiar układu odpowiada liczbie reszt; symetria pozwala obliczyć jedną połowę macierzy. Poprawka ma jednostki znormalizowane, więc przesunięcie świata wynosi `s*δ`. Jest to własna implementacja kroku typu Levenberga–Marquardta z doborem tłumienia i redukcją kroku. Samo wyznaczenie poprawki nie zatwierdza dokumentu.

## 16. Backtracking i pętla solvera

**Lokalizacja w kodzie:** [ConstraintSolver.cpp](../cad_core/src/solver/ConstraintSolver.cpp), `solvePointConstraintSystem`, `applyCorrection`, `allSatisfied`. Publiczne wejścia to przeciążenia `solveConstraints`.

**Zastosowanie:** powtarzanie poprawek do uzyskania spójnej geometrii i odrzucanie ruchów, które pogarszają wynik lub dają niepoprawny stan.

**Pseudokod:**

```text
solve(document,fixedPoint,options):
    candidate <- kopia dokumentu po analitycznym dopasowaniu łuków
    sprawdź wejście i zbierz relacje
    variables <- punkty relacji bez fixedPoint i dodatkowo stałych wierzchołków
    F <- evaluateErrors(candidate)
    s <- geometryScale(candidate)
    lambda <- 0.001
    iterations <- 0

    powtarzaj:
        jeśli wszystkie błędy mieszczą się w swoich tolerancjach:
            ponownie sprawdź geometrię i dokładne przypięcie punktów
            zatwierdź candidate jako document
            zwróć sukces
        jeśli wyczerpano limit albo geometria jest zdegenerowana:
            zwróć niepowodzenie bez zmiany document

        J <- computePointJacobian(candidate,variables,F,s)
        currentNorm <- norma znormalizowanych F
        accepted <- false

        maksymalnie 10 prób tłumienia:
            delta <- dampedCorrection(J,F,kinds,s,lambda)
            alpha <- 1
            jeśli delta jest poprawna, maksymalnie 16 prób kroku:
                trial <- pozycje candidate + alpha*s*delta
                dopasuj łuki i oceń trial
                jeśli poprawna i (norma spadła lub wszystkie tolerancje spełnione):
                    candidate,F <- trial,błędy trial
                    lambda <- max(1e-12,0.3*lambda)
                    accepted <- true
                    przerwij próby
                alpha <- alpha/2
            jeśli accepted: przerwij próby tłumienia
            lambda <- 10*lambda

        jeśli nie accepted:
            accepted <- wynik próby awaryjnego kroku przy prostopadłości
            jeśli accepted: lambda <- 0.001
        jeśli nadal brak poprawy: zwróć niepowodzenie
        iterations <- iterations+1
```

**Zasada działania:** Backtracking ocenia pełny krok, a następnie jego kolejne połowy na rzeczywistej geometrii. Nieudane skracanie zwiększa tłumienie i powoduje ponowne wyznaczenie poprawki. Jacobian jest aktualizowany po przyjęciu ruchu, a nie po każdym skróceniu tego samego kroku. Akceptacja wymaga spełnienia tolerancji lub zmniejszenia normy znormalizowanych reszt. Limit iteracji dotyczy zaakceptowanych poprawek, a nie czasu ani wszystkich prób. Ostateczne zatwierdzenie wymaga poprawnej topologii, relacji i przypiętych punktów. Brak zbieżności pozostawia dokument wejściowy bez zmian; metoda lokalna nie gwarantuje rozwiązania globalnego ani jedynego.

## 17. Awaryjne przeszukiwanie współrzędnych - coordinate search

**Lokalizacja w kodzie:** [ConstraintSolver.cpp](../cad_core/src/solver/ConstraintSolver.cpp), `tryPerpendicularPointStep`. W starszej ścieżce testowej odpowiada jej `tryPerpendicularStep`.

**Zastosowanie:** próba wyjścia z problematycznego ustawienia niemal prostopadłych odcinków, gdy krok oparty na Jacobianie nie poprawił wyniku.

**Pseudokod:**

```text
tryPerpendicularPointStep(candidate,F,variables,s):
    jeśli żaden błąd równoległości nie ma abs(error) >= 1-1e-8:
        zwróć false
    currentNorm <- norma znormalizowanych F

    dla każdego wolnego punktu:
        dla osi X,Y:
            dla znaku -1,+1:
                fraction <- 0.0001
                maksymalnie 8 prób:
                    trial <- candidate z jedną współrzędną zmienioną o znak*fraction*s
                    użyj nextafter, jeśli dodanie nic nie zmieniło
                    dopasuj łuki i policz wszystkie błędy
                    jeśli geometria poprawna i norma < currentNorm:
                        zatwierdź trial jako candidate
                        zwróć true
                    fraction <- fraction/2
    zwróć false
```

**Zasada działania:** Reszta równoległości ma postać sinusa kąta. Przy 90° jej moduł osiąga maksimum, a pochodna względem kąta jest zerowa, przez co linearyzacja może nie wskazać użytecznego ruchu. Deterministyczne próby w kierunkach dodatniej i ujemnej osi X lub Y pozwalają opuścić takie ustawienie. Akceptowana jest pierwsza dopuszczalna próba zmniejszająca łączną normę reszt, a tłumienie wraca do `0,001`. Mechanizm jest stosowany po niepowodzeniu kroku Jacobiego i nie zastępuje podstawowej metody solvera.

## 18. Koalescencja ruchu myszy - latest sample wins

**Lokalizacja w kodzie:** [Projecr1_GK.cpp](../Projecr1_GK.cpp), `WndProc`: obsługa `WM_MOUSEMOVE`, `WM_TIMER`, `WM_LBUTTONUP`; stan przechowuje [AppState.h](../src/platform/win32/AppState.h).

**Zastosowanie:** ograniczenie liczby uruchomień solvera podczas przeciągania, aby seria starych próbek kursora nie opóźniała podglądu.

**Pseudokod:**

```text
WM_MOUSEMOVE podczas przeciągania:
    jeśli timer działa:
        pendingPosition <- aktualna pozycja kursora
    inaczej:
        przelicz i pokaż podgląd bezpośrednio

WM_TIMER właściwego timera:
    jeśli jest pendingPosition:
        P <- pendingPosition
        usuń pendingPosition
        przelicz podgląd dla P i odmaluj okno

WM_LBUTTONUP:
    przelicz końcową pozycję kursora
    zakończ gest i zatrzymaj aktualizacje
```

**Zasada działania:** Stan adaptera przechowuje wyłącznie najnowszą oczekującą pozycję kursora. Kolejne `WM_MOUSEMOVE` nadpisują ją przed obsługą timera, dzięki czemu solver nie przelicza całej historii ruchu. Żądany interwał wynosi 1 ms, lecz rzeczywista obsługa zależy od systemu i kolejki komunikatów. Zwolnienie przycisku wymusza końcową próbkę. Awaria utworzenia timera uruchamia bezpośrednią obsługę. Mechanizm scala dane ruchu we własnym stanie aplikacji i nie usuwa dowolnych komunikatów z kolejki systemowej.

## 19. Algorytm Xiaolina Wu - antyaliasing

**Lokalizacja w kodzie:** [Wu.cpp](../src/render/Wu.cpp), `cad::render::rasterizeWuLine`; mieszanie z tłem i łączenie segmentów w [DibRenderer.cpp](../src/platform/win32/DibRenderer.cpp), `drawWuPolyline`. Tryb wybiera przycisk **Wu (AA)**.

**Zastosowanie:** wygładzenie brzegów odcinków, Bézierów i łuków. Wu zwraca piksele z pokryciem, zamiast wybierać jedynie piksele całkowicie zamalowane.

**Pseudokod:**

```text
rasterizeWuLine(A,B,width):
    odrzuć niepoprawne współrzędne lub szerokość
    jeśli width > 1:
        dla pikseli w otoczeniu odcinka o promieniu width/2+0.5:
            d <- odległość środka piksela od odcinka
            alpha <- clamp(width/2+0.5-d,0,1)
            zwróć dodatnie pokrycia i zakończ
    jeśli A == B: zwróć pokrycia kwadratowego punktu
    jeśli linia stroma: zamień osie X,Y
    uporządkuj końce według rosnącego X
    gradient <- (B.y-A.y)/(B.x-A.x)
    dla każdej kolumny X od najbliższej A.x do najbliższej B.x:
        gap <- długość części [A.x,B.x] wewnątrz [X-0.5,X+0.5]
        Y <- wysokość prostej w kolumnie X
        jeśli oba końce są w tej samej kolumnie: Y <- średnia ich wysokości
        dla pikseli przecinających pas [Y-width/2,Y+width/2]:
            pokrycie <- gap * długość przecięcia pasa z pikselem
            zwróć współrzędne z przywróconymi osiami i dodatnie pokrycie

drawWuPolyline(punkty,kolor):
    dla każdego niezerowego segmentu łamanej:
        przytnij go z marginesem dla wygładzonych brzegów
        próbki <- rasterizeWuLine(segment)
        dla każdej próbki wewnątrz sceny:
            jeśli width > 1: maska[piksel] <- max(maska[piksel],próbka.pokrycie)
            inaczej: maska[piksel] <- min(1,maska[piksel]+próbka.pokrycie)
    dla każdego dotkniętego piksela:
        wynik <- maska*kolor + (1-maska)*dotychczasoweTło
```

**Zasada działania:** Dla szerokości 1 klasyczny algorytm Wu rozdziela pokrycie między piksele `floor(Y)` i `floor(Y)+1`, proporcjonalnie do części ułamkowej Y. Końce ułamkowe uwzględniają czynnik `gap`. Zamiana osi obsługuje linie strome; `floor` zachowuje poprawność dla współrzędnych ujemnych. Osobno rozpatrywane są odcinki zerowe i mieszczące się w jednej kolumnie.

Dla większych szerokości projekt stosuje geometryczny obrys o zaokrąglonych końcach. Pokrycie jest przybliżane wzorem `clamp(width/2+0.5-distance,0,1)` dla odległości środka piksela od odcinka. Szerokość jest mierzona prostopadle do odcinka, a sąsiednie segmenty są łączone maksimum pokryć. Konstrukcja ogranicza zależne od kąta zmiany szerokości i tworzy zaokrąglone połączenia. Jest rozszerzeniem trybu AA, a nie klasycznym dwupikselowym krokiem Wu.

Renderer najpierw składa maskę całej krawędzi, a następnie miesza ją z rzeczywistą zawartością DIB. Wariant jednopikselowy sumuje pokrycia z ograniczeniem do 1, natomiast szeroki obrys korzysta z maksimum. Kanały RGB są mieszane bez korekcji gamma. Wyznaczane pokrycie jest przybliżeniem, a nie dokładnym całkowaniem pola przecięcia piksela z obrysem.

## Dokumenty powiązane

- [Dokumentacja projektu](documentation.md): obsługa, model danych, solver i uruchamianie.
- [Architektura](architecture/README.md): podział odpowiedzialności i zależności.
- [Weryfikacja poprawności](../tests/README.md): zakres testów automatycznych.
