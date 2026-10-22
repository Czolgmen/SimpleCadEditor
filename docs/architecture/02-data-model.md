# Model danych

[Spis treści](README.md)

## Współrzędne i identyfikatory

Geometria używa `Vec2 { double x, y; }` we współrzędnych świata. Kamera odpowiada za przeliczenie na ekran, skalę, przesunięcie i odwrócenie osi Y. Zaokrąglenie do piksela następuje dopiero podczas rasteryzacji. Początkowo jedna jednostka świata odpowiada jednemu pikselowi przy skali 100%.

Każdy wielokąt, wierzchołek, krawędź i ograniczenie ma własny typ ID oparty np. na `uint64_t`. Identyfikator nie jest indeksem w `vector` i nie zmienia się po usunięciu innego elementu. W obrębie dokumentu identyfikatory nie są ponownie wykorzystywane.

## Encje

```cpp
struct Vertex {
    VertexId id;
    Vec2 position;
    ContinuitySetting continuity;
};

struct LineSegment {};
struct CubicBezier { Vec2 control1, control2; };
struct CircularArc { double bulge; };

using EdgeGeometry = std::variant<LineSegment, CubicBezier, CircularArc>;

struct Edge {
    EdgeId id;
    VertexId start;
    VertexId end;
    EdgeGeometry geometry;
};

struct Polygon {
    PolygonId id;
    std::vector<VertexId> vertices;
    std::vector<EdgeId> edges;
};
```

`Document` posiada rejestry encji i rejestr ograniczeń. Wielokąt przechowuje kolejność identyfikatorów; dane wierzchołków i krawędzi istnieją tylko w jednym miejscu. Wyjściowo różne wielokąty nie współdzielą wierzchołków. Relacje mogą łączyć ich krawędzie.

Wielokąt o `n` wierzchołkach ma `n` krawędzi. Krawędź `edges[i]` prowadzi od `vertices[i]` do `vertices[(i+1)%n]`. Wymagamy co najmniej trzech wierzchołków, ale dopuszczamy przecięcia krawędzi. Przecięcie nie tworzy nowego wierzchołka ani dodatkowej relacji.

Punkty kontrolne należą do segmentu Béziera; nie są wierzchołkami topologicznymi. Wskazuje się je przez `(EdgeId, ControlPointIndex)`. Końce odcinka, Béziera i łuku zawsze pochodzą z powiązanych wierzchołków, co zapewnia G0 bez dodatkowego równania.

`ContinuitySetting` zawiera klasę `G0/G1/C1` i pochodzenie `Default/Explicit`. Pozwala to odróżnić ustawienie automatyczne od decyzji użytkownika. Szczegóły propagacji i konwersji opisuje [dokument o krzywych](04-curves.md).

## Ograniczenia

```cpp
struct FixedLength { EdgeId edge; double length; };
struct Parallel { EdgeId first, second; };
using ConstraintData = std::variant<FixedLength, Parallel>;

struct Constraint {
    ConstraintId id;
    ConstraintData data;
};
```

Jedna krawędź będąca odcinkiem może występować w wielu różnych ograniczeniach. Duplikat tej samej relacji odrzucamy, natomiast nowa relacja o innym znaczeniu przechodzi przez solver. Ciągłość jest zapisana przy wierzchołku, a jej równania są generowane razem z ograniczeniami odcinków.

Zgodnie z ustaleniem użytkownika `FixedLength` może wskazywać wyłącznie `LineSegment`, a oba końce relacji `Parallel` muszą wskazywać odcinki. Béziery i łuki nie mają ograniczeń długości ani równoległości, również na swoich cięciwach. Zachowują wymagane ustawienia ciągłości w wierzchołkach.

## Zmiany topologii i integralność

Wszystkie zmiany przechodzą przez operacje dokumentu. Warstwa UI nie dopisuje samodzielnie elementów do kontenerów.

- Podział usuwa starą krawędź i jej ograniczenia. Tworzy nowy wierzchołek oraz dwie nowe krawędzie z nowymi ID.
- Usunięcie wierzchołka usuwa obie jego krawędzie incydentne i relacje, które ich dotyczą. Powstaje jedna nowa krawędź. Usunięcie wierzchołka trójkąta jest odrzucane.
- Relacja między wielokątami znika w całości, jeżeli znika jedna z jej krawędzi. Druga krawędź zachowuje pozostałe relacje.
- Ocalałe krawędzie zachowują ID. Sprawdzamy ich nowe sąsiedztwo i ciągłość wierzchołków; nie przepinamy relacji na arbitralnie wybrany fragment usuniętej krawędzi.
- Zmiana typu istniejącej krawędzi zachowuje ID. Przy konwersji odcinka na Béziera lub łuk usuwamy wszystkie ograniczenia długości i równoległości dotyczące tego odcinka, także relacje z innymi wielokątami. Usunięcie jest częścią tej samej transakcji; nieudana konwersja przywraca również relacje.
- Konwersja krzywej na odcinek nie odtwarza dawnych ograniczeń. Użytkownik może nadać je ponownie. Każda konwersja wymaga poprawnych ustawień ciągłości i wykonalnego wyniku solvera.

Indeksy `VertexId -> krawędzie incydentne` i `EdgeId -> ograniczenia` przyspieszają wyszukiwanie zależności. Są danymi pochodnymi, odbudowywanymi po zmianie topologii; nie stanowią drugiego źródła prawdy.

`validateDocument()` sprawdza cykle, referencje, skończone współrzędne, dopuszczalne typy ciągłości, brak odwołań do usuniętych encji oraz to, czy każda relacja długości lub równoległości dotyczy wyłącznie odcinków. Kopia dokumentu wystarczy jako początkowy mechanizm transakcji dla niewielkiej sceny demonstracyjnej.
