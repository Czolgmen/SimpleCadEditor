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
- `selectedEdgeLength()` zwraca aktualną długość zaznaczonej krawędzi jako wartość domyślną dialogu, a `selectedFixedLength()` odczytuje zadaną długość relacji. Brak odpowiedniego zaznaczenia daje `nullopt`.
- `setSelectedEdgeLength(length)` dodaje lub aktualizuje relację na kopii dokumentu i uruchamia solver z zablokowanym początkiem krawędzi. Zatwierdza geometrię i magazyn relacji dopiero przy `Converged`; odrzucenie zachowuje dokument i zaznaczenie. `removeSelectedEdgeLength()` usuwa relację bez zmiany geometrii. Oba udane polecenia zachowują zaznaczenie krawędzi i kończą gest.

`LengthInput.h/.cpp` parsuje dodatnią, skończoną długość z tekstu niezależnie od ustawień regionalnych. Akceptuje kropkę lub przecinek dziesiętny, otaczające białe znaki, znak plus i zapis wykładniczy. Odrzuca częściowo poprawne dane, NaN, nieskończoności, wartości niedodatnie i liczby poza zakresem `double`. Nie zależy od Windows.

Pozycje i tolerancja są w jednostkach świata. Frontend przelicza współrzędne ekranu oraz promień trafienia, przechwytuje i zwalnia mysz, zleca rysowanie i pokazuje komunikaty błędów. Sesja odrzuca nieskończone współrzędne, NaN i ujemną lub nieskończoną tolerancję.

## Gesty

`DragState` to `std::variant<DraggingVertex, DraggingPolygon>`; brak transakcji oznacza bezczynność. Nie da się jednocześnie przeciągać punktu i figury.

Transakcja przechowuje kopię początkowego dokumentu wraz z ograniczeniami. Podgląd wierzchołka powstaje z ostatniego poprawnego dokumentu: punkt zachowuje przesunięcie miejsca złapania i otrzymuje dokładną pozycję z kursora, a solver dopasowuje pozostałe punkty do zapisanych długości. Nieudana próba zachowuje ostatni poprawny podgląd i pozwala kontynuować gest. Figura otrzymuje translację od początkowej pozycji kursora na kopii stanu początkowego, co zachowuje długości bez sumowania przyrostów. Kopiowanie dokumentu na każde przesunięcie jest celową, prostą implementacją dla małych scen z zadania.

`cancelInteraction` i aktywne Esc przywracają wszystkie pozycje oraz relacje sprzed gestu, w tym wierzchołki przesunięte przez solver. Zwolnienie przycisku po odrzuconym ruchu zatwierdza ostatni poprawny podgląd. Oddzielna wizualizacja niepoprawnego kandydata pozostaje przyszłym etapem.

W WinAPI zakończenie gestu następuje przed `ReleaseCapture`. Dzięki temu następujące po nim `WM_CAPTURECHANGED` nie wycofuje zatwierdzonej zmiany. Nieoczekiwana utrata przechwycenia lub fokusu anuluje trwającą transakcję.
