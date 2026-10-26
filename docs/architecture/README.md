# Architektura edytora wielokątów

Status: plan realizowany etapami. Działają model i walidacja, scena demonstracyjna, renderer GDI z buforem oraz wybór, przeciąganie, podział i usuwanie elementów. Przenośna [`EditorSession`](../../src/editor/README.md) obsługuje edycję i anulowanie gestów. Wspólny solver długości i równoległości z numerycznym [obliczaniem Jacobiego](09-jacobian.md) działa w rdzeniu oraz podczas przeciągania i zmiany długości w aplikacji. Menu PPM i dialog umożliwiają dodawanie, zmianę i usuwanie długości. Polecenie „Ustaw równoległość z...” pozwala wskazać drugą krawędź i zatwierdza parę dopiero po sukcesie solvera; Esc anuluje wybór. Renderer oznacza długości i pary przy krawędziach. Dokument usuwa relacje zmienionych krawędzi przy podziale i usuwaniu wierzchołka. Menu „Usuń równoległość” pozwala usunąć wybraną parę wskazanej krawędzi bez zmiany geometrii. Model Béziera, próbkowanie przyrostowe i konwersja z menu PPM są gotowe; GDI pokazuje krzywą i uchwyty. Renderer oraz hit testy krawędzi i wnętrza korzystają ze wspólnego próbkowania `sampleEdge`. Uchwyty można zaznaczać i przeciągać z anulowaniem, a translacja całej figury przesuwa także jej punkty kontrolne. Działają zapis ustawień ciągłości i domyślnego G1 przy konwersji. Prywatna, przetestowana pętla solvera rozwiązuje G1/C1 wraz z długościami i równoległością, z dokładnym przypięciem wierzchołka lub uchwytu i wycofaniem nieudanych operacji. Publiczne API i EditorSession używają jej podczas przeciągania oraz konwersji krawędzi. Menu PPM przy wierzchołku udostępnia wybór G0/G1/C1 z oznaczeniem aktualnej klasy. Model dzieli Béziera w t=0.5 algorytmem de Casteljau, zachowując kształt i nadając nowemu wierzchołkowi domyślne G1. Podział odcinków i Bézierów w sesji działa transakcyjnie z rozwiązaniem ciągłości i przypięciem nowego wierzchołka. Usuwanie wierzchołka również rozwiązuje relacje na kopii, z przypięciem poprzedniego sąsiada. Próbkowanie ogólnych łuków przez niezerowy bulge jest dostępne w geometrii i testach. Model krawędzi i wspólne próbkowanie uwzględniają łuki; walidacja sprawdza podstawowe parametry i dozwolone klasy ciągłości. EdgeTangents obsługuje pełne pochodne odcinka, Béziera i łuku przez std::optional<Vec2>. Obliczanie błędów ciągłości z dokumentu uwzględnia łuki, odrzuca ich C1 i propaguje błędy stycznych. G0 pomija pochodne po sprawdzeniu odwołań i pozycji. Modelowe updateArcBulges przywraca półokręgi G0/G0 i dopasowuje pojedyncze G1 do stycznej sąsiada, zatwierdzając wszystkie wygięcia razem. Przy dwóch łukach wygięcie łuku o mniejszym ID pozostaje referencją. Kolektor i solver uwzględniają G1 łuków. Wygięcia są dopasowywane przed oceną reszt na kopii początkowej, w próbach kroku i w Jacobim; próbki odtwarzają również bulge. Sesja obsługuje przeciąganie i G0/G1 łuków, a menu wyłącza ich C1. Menu PPM przełącza odcinek, Béziera i łuk. Konwersja na łuk zachowuje metadane, odrzuca C1 i G1/G1, usuwa relacje odcinka oraz rozwiązuje pozostałe zależności na kopii. Podział łuku działa w modelu, sesji i komendzie WinAPI: tworzy dwa odcinki przez t=0.5 z nowym G0/Default, rozwiązuje pozostałe zależności z tym punktem przypiętym i zatwierdza wyłącznie poprawny wynik. Działa własny Bresenham z przycinaniem przed zaokrągleniem i zapisem do DIB. Radio buttony GDI/Bresenham przełączają rysowanie odcinków i próbek krzywych bez zmiany dokumentu; dekoracje pozostają w GDI. Krawędzie są rysowane przed uchwytami i etykietami, z synchronizacją GDI/CPU oraz fallbackiem GDI przy niedostępnym buforze.

Podstawa: [treść zadania](../instuction.md). Wybrany wariant **A–K**: relacja równoległości dwóch krawędzi i domyślna ciągłość **G1** po włączeniu segmentu Béziera. Użytkownik nadal może jawnie wybrać G0, G1 lub C1 w miejscach dopuszczonych przez zadanie.

Stack: **C++20 + WinAPI + GDI**, z rdzeniem geometrii i mechanizmem edycji niezależnymi od Windows. Dokumenty opisują także docelowe moduły i API, które nie są jeszcze wdrożone.

## Dokumenty

1. [Podział na warstwy i kierunek zależności](01-layers.md) — granice modułów i podłączenie innego frontendu.
2. [Model danych](02-data-model.md) — scena, topologia, identyfikatory i własność danych.
3. [Ograniczenia i solver](03-constraints.md) — wspólne rozwiązywanie relacji, przeciąganie i konflikty.
4. [Krzywe i ciągłość](04-curves.md) — Bézier, łuki, G0/G1/C1 i zmiany topologii.
5. [Edycja i obsługa wejścia](05-editor.md) — operacje użytkownika, zaznaczanie i przepływ zdarzeń.
6. [Rysowanie i GDI](06-rendering.md) — rasteryzacja, wymagane algorytmy i adapter Windows.
7. [Układ projektu i wdrożenie](07-implementation.md) — pliki, etapy i weryfikacja.
8. [Założenia i pokrycie wymagań](08-requirements.md) — interpretacje niejednoznaczności oraz mapa funkcjonalności.
9. [Macierz Jacobiego](09-jacobian.md) — kontrakt, różnice skończone i przykład pochodnych w prototypie solvera.

## Decyzje przewodnie

- Rdzeń przechowuje współrzędne w `double` i udostępnia operacje na obiektach o stabilnych identyfikatorach.
- Wielokąt to zamknięty, uporządkowany cykl wierzchołków i krawędzi. Samoprzecięcia są dozwolone.
- Zadana długość i równoległość dotyczą wyłącznie odcinków. Béziery i łuki obsługują ciągłość w wierzchołkach, bez ograniczeń długości i równoległości.
- Ograniczenia odcinków oraz ciągłość krzywych trafiają do jednego solvera, ponieważ mogą być sprzężone przez wspólne wierzchołki. Kolejne niezależne „poprawki” geometrii nie zapewnią zgodności wszystkich relacji.
- Edycja jest transakcyjna: wynik zostaje zatwierdzony dopiero po sprawdzeniu topologii i ograniczeń.
- WinAPI tłumaczy zdarzenia systemowe, a GDI wyświetla przygotowany obraz. Żadna z tych warstw nie definiuje zasad geometrii.
- Budujemy zakres potrzebny do zadania: edytor geometrii 2D, bez systemu wtyczek, ECS, modelowania 3D czy pełnego jądra CAD.

Największym ryzykiem jest zachowanie solvera przy wielu relacjach i ciągłości. Dlatego jego prototyp powstaje wcześnie, przed rozbudową interfejsu.
