# Przenośna sesja edytora

`EditorSession.h/.cpp` posiada dokument, zaznaczenie i transakcję przeciągania. Kod w tym katalogu używa C++20 oraz `cad_core`; nie dołącza Windows ani GDI. Obecnie te same źródła są kompilowane w aplikacji i programie testowym.

## API dla frontendu

- `document()` i `selection()` udostępniają stan tylko do odczytu. Odwołania do encji należy pobierać ponownie po modyfikacji sesji.
- `selectAt(position, tolerance)` wybiera wierzchołek, krawędź albo wnętrze figury, w tej kolejności. Puste tło usuwa zaznaczenie. Metoda nie rozpoczyna gestu, a trwający gest zatwierdza; służy m.in. do wskazywania celu menu kontekstowego.
- `pointerDown(position, tolerance)` wybiera element i rozpoczyna przeciąganie wierzchołka lub wielokąta. Zwraca `true`, jeśli rozpoczął gest. Kliknięcie krawędzi tylko ją zaznacza.
- `pointerMove(position)` aktualizuje podgląd i zwraca `true`, jeśli kandydat został przyjęty. Niepoprawne współrzędne lub odrzucony ruch pozostawiają ostatni poprawny dokument.
- `pointerUp()` zatwierdza ostatni poprawny podgląd i kończy gest. Frontend może wcześniej przekazać końcową pozycję przez `pointerMove`.
- `cancelInteraction()` przywraca dokument sprzed aktywnego gestu, zachowując zaznaczenie, i zwraca `true`. W stanie bezczynności niczego nie zmienia.
- `escape()` anuluje aktywny gest, a poza gestem czyści zaznaczenie.
- `canSplitSelectedEdge()` i `canDeleteSelectedVertex()` określają dostępność poleceń. `splitSelectedEdge()` zaznacza nowy punkt, a `deleteSelectedVertex()` czyści zaznaczenie. Udane polecenie kończy gest; odrzucone zachowuje jego stan.

Pozycje i tolerancja są w jednostkach świata. Frontend przelicza współrzędne ekranu oraz promień trafienia, przechwytuje i zwalnia mysz, zleca rysowanie i pokazuje komunikaty błędów. Sesja odrzuca nieskończone współrzędne, NaN i ujemną lub nieskończoną tolerancję.

## Gesty

`DragState` to `std::variant<DraggingVertex, DraggingPolygon>`; brak transakcji oznacza bezczynność. Nie da się jednocześnie przeciągać punktu i figury.

Transakcja przechowuje kopię początkowego dokumentu. Każdy podgląd powstaje z tego samego stanu: punkt zachowuje przesunięcie miejsca złapania, a figura otrzymuje translację od początkowej pozycji kursora. Kopiowanie dokumentu na każde przesunięcie jest celową, prostą implementacją dla małych scen z zadania; nie jest jeszcze zoptymalizowanym mechanizmem podglądu solvera.

W WinAPI zakończenie gestu następuje przed `ReleaseCapture`. Dzięki temu następujące po nim `WM_CAPTURECHANGED` nie wycofuje zatwierdzonej zmiany. Nieoczekiwana utrata przechwycenia lub fokusu anuluje trwającą transakcję.
