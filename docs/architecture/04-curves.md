# Krzywe i ciągłość

[Spis treści](README.md)

Stan wdrożenia: model ma odcinek i `CubicBezier` z dwoma uchwytami, funkcję punktu oraz próbkowanie przyrostowe. Działają konwersje w dokumencie, sesji i menu PPM, czyszczenie ograniczeń odcinka oraz rysowanie krzywej, wieloboku kontrolnego i uchwytów. Renderer oraz testy krawędzi i wnętrza używają wspólnej łamanej z `sampleEdge`. Uchwyty mają hit test, wyłączne zaznaczenie i swobodne przeciąganie z anulowaniem gestu. Translacja figury przesuwa także oba uchwyty każdej jej krzywej. Domyślne G1, pozostałe klasy ciągłości, podział de Casteljau i łuki opisane poniżej pozostają planem. Interfejs blokuje podział Béziera do czasu implementacji właściwego algorytmu.

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

- Dla dwóch końców G0 przyjmujemy `bulge = +1` lub `-1`. Powstaje półokrąg o promieniu równym połowie długości cięciwy.
- G1 dopuszczamy najwyżej na jednym końcu łuku. Jego styczna i oba końce określają kandydat łuku; sprawdzamy również zwrot stycznej.
- G1 na obu końcach oraz C1 przy łuku odrzucamy przed obliczeniami. Aby przenieść G1 na drugi koniec, użytkownik najpierw ustawia G0 na pierwszym.
- Zbieżne końce, brak niezerowej stycznej albo brak skończonego dopuszczalnego łuku dają diagnostykę. Nie przechodzimy po cichu na odcinek.
- Początkowa konwersja odcinka bez ustalonej styczności wybiera dodatnie wygięcie. Zmiana strony łuku może być późniejszym udogodnieniem.

Jeżeli konwersja Béziera na łuk napotyka jawne C1 lub dwa końce G1, odrzucamy ją z informacją, które ustawienie trzeba zmienić. Jawne decyzje użytkownika mają pierwszeństwo przed automatycznym doborem ustawień.

## Podział i usuwanie

Odcinek dzielimy w połowie. Béziera dzielimy dla `t=0.5` algorytmem de Casteljau: geometria przed ponownym rozwiązaniem relacji zostaje zachowana. Nowy wierzchołek otrzymuje G1; zachowane klasy ciągłości na zewnętrznych końcach nadal obowiązują. C1 może wymagać korekty, ponieważ podział zmienia lokalną parametryzację segmentów.

Łuk dzielimy w jego środku parametrycznym, ale w pierwszej implementacji zastępujemy go **dwoma odcinkami**. Dwa fragmenty zachowujące kształt dawnego półokręgu byłyby ćwiartkami okręgu i przy nowych końcach G0/G0 nie spełniałyby reguły promienia z zadania. To celowe, opisane w UI założenie; nie obiecujemy zachowania krzywizny po podziale łuku.

Po usunięciu wierzchołka łączymy jego sąsiadów nowym odcinkiem. Zachowane ustawienia ciągłości na pozostałych wierzchołkach przechodzą walidację i solver. Cała zmiana może zostać odrzucona, jeżeli nie ma poprawnego wyniku. Usuwanie relacji dotyczących znikających krawędzi opisuje [model danych](02-data-model.md).
