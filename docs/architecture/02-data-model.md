# 02. Model danych

[Spis dokumentacji architektury](README.md)

## Geometria i identyfikatory

Położenie jest reprezentowane przez `Vec2` z dwiema współrzędnymi typu `double`. Model używa układu świata z osią Y skierowaną w górę. Adapter widoku odwraca oś Y i uwzględnia margines; obecnie jedna jednostka świata odpowiada jednemu pikselowi. Konwersja na całkowite współrzędne następuje w warstwie renderowania.

[`Id.h`](../../cad_core/include/cad_core/model/Id.h) definiuje odrębne typy `VertexId`, `EdgeId` i `PolygonId`, oparte na liczbach 64-bitowych. Zero oznacza identyfikator nieprawidłowy. Identyfikatory są stabilne, nie są indeksami kontenerów i nie są ponownie używane po usunięciu elementu. Relacje długości i równoległości wskazują krawędzie przez ich ID.

## Encje i kontenery

| Typ | Przechowywane informacje |
| --- | --- |
| `Vertex` | ID, położenie, klasa ciągłości i źródło ustawienia `Default/Explicit` |
| `Edge` | ID, ID początku i końca, wariant geometrii oraz `strokeWidth` |
| `Polygon` | ID i uporządkowane listy identyfikatorów wierzchołków oraz krawędzi |
| `Document` | Mapy encji, listy ograniczeń i liczniki kolejnych identyfikatorów |

`EdgeGeometry` jest wariantem `LineSegment`, `CubicBezier` albo `CircularArc`. Odcinek nie wymaga dodatkowych parametrów. Bézier przechowuje dwa punkty kontrolne, a łuk parametr `bulge`. Końce wszystkich geometrii są wspólnymi wierzchołkami dokumentu. Grubość krawędzi wynosi od 1 do 32 px i nie uczestniczy w obliczeniach ograniczeń.

Kontenery encji mają postać `std::map<Id, Entity>`, a ograniczenia są przechowywane w wektorach. Publiczny interfejs dokumentu udostępnia odczyt danych oraz kontrolowane operacje modyfikujące.

## Niezmienniki topologii

Wielokąt ma co najmniej trzy wierzchołki i tyle samo krawędzi. Krawędź na pozycji `i` łączy wierzchołek `i` z następnym, z zamknięciem cyklu. Odwołania muszą istnieć i mieć zgodne identyfikatory. Pełna kontrola sceny przy odczycie sprawdza również wyłączną przynależność elementów do obrysów i brak obiektów osieroconych.

Walidacja obejmuje skończoność współrzędnych, parametry łuków, dopuszczalne ustawienia ciągłości oraz poprawność relacji. Samoprzecięcie obrysu jest dopuszczalne; wnętrze do zaznaczania wyznacza reguła parzystości.

## Operacje i trwałość danych

[`Document.cpp`](../../cad_core/src/model/Document.cpp) realizuje dodawanie, przesuwanie, konwersję, podział i usuwanie elementów oraz zmianę ograniczeń. Sesja edytora łączy operację modelu z rozwiązaniem zależności i zatwierdzeniem wyniku.

[`SceneIO`](../../cad_core/include/cad_core/io/SceneIO.h) zapisuje kompletny stan trwały dokumentu: encje, ID, liczniki, geometrię, grubości, ciągłość i ograniczenia. Stan zaznaczenia, gestu i renderera nie jest serializowany. Opis składni i kontroli danych znajduje się w [specyfikacji `.cadscene`](../scene-format.md).
