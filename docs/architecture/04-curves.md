# Krzywe i ciągłość

[Spis treści](README.md)

Stan wdrożenia: model ma odcinek, `CubicBezier` z dwoma uchwytami i `CircularArc` z parametrem bulge. Dostępne są funkcja punktu i próbkowanie przyrostowe Béziera oraz pełne pochodne na końcach odcinka, Béziera i łuku (`EdgeTangents`, std::optional<Vec2>). Działają konwersje w dokumencie, sesji i menu PPM, czyszczenie ograniczeń odcinka oraz rysowanie krzywej, wieloboku kontrolnego i uchwytów. Renderer oraz testy krawędzi i wnętrza używają wspólnej łamanej z `sampleEdge`. Uchwyty mają hit test, wyłączne zaznaczenie i przeciąganie z rozwiązywaniem ciągłości i anulowaniem gestu. Translacja figury przesuwa także oba uchwyty każdej jej krzywej. Wierzchołki przechowują klasę G0/G1/C1 i pochodzenie Default/Explicit, a konwersja zapisuje domyślne G1 na końcach bez nadpisywania jawnych wyborów. Prywatna pętla `solvePointConstraintSystem` rozwiązuje G1/C1 razem z długościami i równoległością, przesuwając wierzchołki i uchwyty oraz zachowując dokładnie przypięty punkt. Próby działają na kopiach; dokument jest zatwierdzany wyłącznie po zbieżności. Publiczne API i EditorSession używają tej pętli podczas przeciągania punktów oraz konwersji krawędzi. Menu PPM przy wierzchołku pozwala jawnie wybrać G0/G1/C1 i wskazuje aktualną klasę. Geometria i Document::splitEdge obsługują podział de Casteljau dla t=0.5 z zachowaniem kształtu, ID pierwszej połowy i istniejących ustawień ciągłości. Nowy wierzchołek ma G1/Default; niepoprawne uchwyty są odrzucane przed zmianami. Sesja i menu udostępniają podział odcinka oraz Béziera. Operacja działa na kopii, rozwiązuje wszystkie relacje z nowym wierzchołkiem przypiętym w miejscu podziału i zatwierdza wynik wyłącznie po zbieżności. Odrzucenie zachowuje dokument, zaznaczenie i stan interakcji. Geometria udostępnia sampleArc dla skończonego, niezerowego bulge: mniejsze łuki, półokręgi i większe łuki w obu kierunkach. Wspólne computeArcParameters wyznacza środek, promień, kąt początkowy i zakres kąta bez generowania próbek; przy niepoprawnych lub nieprzedstawialnych parametrach zwraca nullopt. sampleArc korzysta z tego wyniku i zgłasza invalid_argument, gdy parametry lub wygenerowane współrzędne są niepoprawne. Środek okręgu jest odsunięty prostopadle od środka cięciwy, a zakres kąta wynosi 4*atan(bulge). Walidacja odrzuca nieprzedstawialny środek, promień i próbki. Model krawędzi i wspólne próbkowanie uwzględniają łuk. Walidator sprawdza jego podstawowe parametry i dozwolone klasy ciągłości, lecz nie zgodność stycznych. Geometria udostępnia arcStartTangent i arcEndTangent jako pełne pochodne dla t w [0,1], z nullopt dla błędnych parametrów, nieskończonych składowych lub zerowego wektora. EdgeTangents deleguje obliczenia łuku do tych funkcji. Obliczanie błędów ciągłości z dokumentu propaguje brak stycznej, uwzględnia G1 przy łukach i odrzuca ich C1; G0 nie wymaga pochodnych. Zerowe pochodne odcinka i Béziera pozostają dozwolone dla C1. Zbieranie połączeń i solver G1 łuków są podłączone do sesji. Konwersje łuków są dostępne w modelu, sesji i menu PPM. Podział łuków jest dostępny w modelu, sesji i komendzie WinAPI; zastępuje łuk dwoma odcinkami przez t=0.5, zachowując transakcyjne rozwiązywanie relacji i dokładne przypięcie nowego G0/Default.

## Wspólna reprezentacja

Każda krawędź ma początek, koniec i geometrię: odcinek, sześcienny Bézier albo łuk okręgu. Wspólne funkcje geometrii udostępniają punkt i styczną dla parametru `t` w `[0,1]`, próbkowanie oraz pomocnicze obliczenia odległości. Nie wymagają dziedziczenia; przy trzech znanych typach wystarczy `std::variant`.

Ograniczenia zadanej długości i równoległości są dostępne tylko dla odcinków. Béziery i łuki, w tym półokręgi, obsługują wymagane klasy ciągłości, ale nie te ograniczenia krawędzi. Konwersja odcinka na krzywą usuwa jego relacje długości i równoległości w tej samej transakcji. Konwersja z powrotem nie odtwarza ich automatycznie. Ciągłość może nadal pośrednio wiązać krzywą z ograniczeniami sąsiedniego odcinka przez wspólny wierzchołek.

## Bézier trzeciego stopnia

`P0` i `P3` to wierzchołki; `P1` i `P2` to punkty kontrolne. Początkowa konwersja odcinka ustawia:

```text
P1 = P0 + (P3 - P0) / 3
P2 = P0 + 2 * (P3 - P0) / 3
```

Taka krzywa początkowo pokrywa się z odcinkiem. Następnie solver uwzględnia ciągłość sąsiednich wierzchołków, co może zmienić jej kształt oraz sąsiednią geometrię.

Przy konwersji ustawiamy G1 na końcach, których ciągłość ma pochodzenie `Default`. Jawnie wybrane G0/G1/C1 zachowujemy, o ile nowa konfiguracja jest dopuszczalna. Jeżeli przypisanie G1 koliduje z regułą sąsiedniego łuku, cała operacja zwraca diagnostykę zamiast zmieniać wcześniejszy wybór po cichu.

Jeśli po konwersji przy wierzchołku pozostają wyłącznie odcinki, zapisane ustawienie ciągłości jest nieaktywne. Nie wymusza wtedy współliniowości zwykłych boków wielokąta. Jawny wybór pozostaje zapamiętany na wypadek ponownego włączenia krzywej.

Pochodne na końcach:

```text
B'(0) = 3 * (P1 - P0)
B'(1) = 3 * (P3 - P2)
```

Styczne zawsze opisujemy zgodnie z kierunkiem obiegu wielokąta: przy wierzchołku porównujemy końcową pochodną krawędzi wchodzącej i początkową pochodną krawędzi wychodzącej.

- G0: wspólne położenie końca i początku.
- G1: zgodny kierunek niezerowych pochodnych, ich długości mogą się różnić.
- C1: identyczne wektory pochodnych dla parametryzacji każdego segmentu na `[0,1]`.

Dla odcinka pochodna wynosi `end - start`. Dlatego C1 na styku odcinka z Bézierem wymaga uchwytu o długości jednej trzeciej długości odcinka. Samo ustawienie uchwytu na wspólnej prostej zapewnia tylko warunek kierunkowy G1.

## Algorytm przyrostowy Béziera

Zgodnie z zadaniem renderer nie opiera rysowania Béziera na bibliotecznym `PolyBezier`. Rdzeń generuje próbki przez bazę potęgową i różnice progresywne:

```text
B(t) = a*t^3 + b*t^2 + c*t + d
a = -P0 + 3*P1 - 3*P2 + P3
b = 3*P0 - 6*P1 + 3*P2
c = -3*P0 + 3*P1
d = P0

h = 1 / N
point = d
delta1 = a*h^3 + b*h^2 + c*h
delta2 = 6*a*h^3 + 2*b*h^2
delta3 = 6*a*h^3

emit(point)
repeat N times:
    point += delta1
    delta1 += delta2
    delta2 += delta3
    emit(point)
```

Ostatni punkt ustawiamy dokładnie na `P3`, ograniczając widoczny dryf numeryczny. Liczbę kroków dobieramy na podstawie tolerancji w przestrzeni świata przekazanej przez widok i oszacowania drugiej pochodnej, z limitem liczby próbek. Hit testing używa tego samego próbkowania i tolerancji. Widok przelicza tolerancję ekranową na jednostki świata; rdzeń nie zna kamery.

## Łuki okręgów

Proponowana reprezentacja: końce oraz parametr `bulge = tan(theta/4)`, gdzie `theta` jest skierowanym kątem łuku. Znak wybiera stronę wygięcia i kierunek. Środek, promień i styczne są wyliczane, nie zapisywane jako drugie niezależne źródło danych.

Geometria udostępnia `arcBulgeFromStartTangent` i `arcBulgeFromEndTangent`. Dla cięciwy `d = end - start` oraz stycznej `t` pierwsza funkcja liczy `alpha = atan2(cross(t,d), dot(t,d))`, a druga odwraca kolejność wektorów. Wynik to `tan(alpha/2)`; dla większych łuków używana jest równoważna tożsamość połowy kąta, aby zachować dokładność blisko pełnego obrotu. Dodatnie skalowanie stycznej nie zmienia wyniku. Zerowa lub równoległa styczna, zbieżne końce, dane nieskończone i nieprzedstawialne parametry okręgu zwracają `nullopt`. Funkcje są używane przy aktualizacji bulge podczas prób solvera i próbkowania Jacobiego.

- Dla dwóch końców G0 przyjmujemy `bulge = +1` lub `-1`. Powstaje półokrąg o promieniu równym połowie długości cięciwy.
- G1 dopuszczamy najwyżej na jednym końcu łuku. Jego styczna i oba końce określają kandydat łuku; sprawdzamy również zwrot stycznej.
- G1 na obu końcach oraz C1 przy łuku odrzucamy przed obliczeniami. Aby przenieść G1 na drugi koniec, użytkownik najpierw ustawia G0 na pierwszym.
- Zbieżne końce, brak niezerowej stycznej albo brak skończonego dopuszczalnego łuku dają diagnostykę. Nie przechodzimy po cichu na odcinek.
- Początkowa konwersja odcinka bez ustalonej styczności wybiera dodatnie wygięcie. Zmiana strony łuku może być późniejszym udogodnieniem.

`Document::convertEdgeToArc` sprawdza końce i dopuszczalne ustawienia przed zmianą danych. Nowy łuk ma bulge=1 i zachowane metadane ciągłości; istniejący poprawny łuk zachowuje swoje wygięcie. Konwersja usuwa długość i wszystkie równoległości tej krawędzi. Sesja rozwiązuje pozostałe relacje na kopii z przypiętym początkiem i zatwierdza wynik dopiero po zbieżności. Menu PPM udostępnia wszystkie trzy typy krawędzi; pozycja odpowiadająca obecnemu typowi jest nieaktywna. Nieudana konwersja na łuk wyświetla wymagania G0/G1 i najwyżej jednego końca G1. Ustawienia ciągłości nie są automatycznie nadpisywane.

Modelowa funkcja `updateArcBulges(Document&)` realizuje dobór wygięć bez przesuwania punktów. G0/G0 przywraca `copysign(1, bulge)`, G1 na początku korzysta z końcowej stycznej poprzedniej krawędzi, a G1 na końcu z początkowej stycznej następnej. Dla dwóch łuków przy wspólnym G1 wygięcie łuku o mniejszym ID jest zachowane, a drugi łuk jest dopasowany do niego. Ponieważ żaden z nich nie może mieć G1 na drugim końcu, zależność nie tworzy łańcucha kolejnych łuków. Wymagane są skończone, niezerowe dotychczasowe bulge oraz poprawne cykle, odwołania, pozycje i ustawienia łuków. Wszystkie propozycje są obliczane z geometrii wejściowej i zatwierdzane razem; odrzucenie zachowuje cały dokument. Niepoprawna pochodna na aktywnym końcu G1 również odrzuca operację. Solver wywołuje funkcję na kopii wejściowej przed walidacją (umożliwiając reset G0/G0), po próbnych poprawkach, podczas kroku wychodzącego z prostopadłości i przed końcową oceną. Jacobian dopasowuje łuki w każdej próbce oraz odtwarza współrzędne i bulge przed następną. Sceny bez łuków zachowują ścieżkę bez dopasowywania łuków. Dopasowanie analityczne może zatwierdzić zmianę bulge przy zerowym budżecie poprawek współrzędnych. Odrzucenie całego rozwiązania zachowuje pierwotne wygięcia. Sesja obsługuje G0/G1 łuków; C1 i drugi koniec G1 są odrzucane, a menu wyłącza C1.

Jeżeli konwersja Béziera na łuk napotyka jawne C1 lub dwa końce G1, odrzucamy ją z informacją, które ustawienie trzeba zmienić. Jawne decyzje użytkownika mają pierwszeństwo przed automatycznym doborem ustawień.

## Podział i usuwanie

Odcinek dzielimy w połowie. Béziera dzielimy dla `t=0.5` algorytmem de Casteljau: geometria przed ponownym rozwiązaniem relacji zostaje zachowana. Nowy wierzchołek otrzymuje G1; zachowane klasy ciągłości na zewnętrznych końcach nadal obowiązują. C1 może wymagać korekty, ponieważ podział zmienia lokalną parametryzację segmentów.

Łuk dzielimy w jego środku parametrycznym, ale w pierwszej implementacji zastępujemy go **dwoma odcinkami**. Dwa fragmenty zachowujące kształt dawnego półokręgu byłyby ćwiartkami okręgu i przy nowych końcach G0/G0 nie spełniałyby reguły promienia z zadania. To celowe, opisane w UI założenie; nie obiecujemy zachowania krzywizny po podziale łuku.

Po usunięciu wierzchołka łączymy jego sąsiadów nowym odcinkiem. Zachowane ustawienia ciągłości na pozostałych wierzchołkach przechodzą walidację i solver. Sesja wykonuje usunięcie na kopii dokumentu, z poprzednim sąsiadem usuwanego punktu przypiętym w jego dotychczasowej pozycji. Cała zmiana jest odrzucana bez zmiany dokumentu i interakcji, jeżeli solver nie znajdzie poprawnego wyniku. Usuwanie relacji dotyczących znikających krawędzi opisuje [model danych](02-data-model.md).
