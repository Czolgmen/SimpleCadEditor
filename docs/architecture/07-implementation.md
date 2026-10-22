# Układ projektu i wdrożenie

[Spis treści](README.md)

## Proponowane pliki

Pierwsza biblioteka ma już [projekt i strukturę katalogów `cad_core`](../../cad_core/README.md): publiczne nagłówki w `cad_core/include/cad_core`, a implementacje w `cad_core/src`, z podziałem na `geometry` i `model`. Jest to szkielet do dalszej implementacji. [Rozwiązanie w katalogu repozytorium](../../Projecr1_GK.slnx) zawiera aplikację oraz bibliotekę. Poniższe drzewo pokazuje docelowy podział odpowiedzialności; część `src/core` odpowiada osobnemu projektowi `cad_core`.

```text
src/
  core/
    geometry/       Vec2, tolerancje, segmenty, styczne, próbkowanie
    model/          Id, Document, Polygon, Vertex, Edge, walidacja
    constraints/    typy relacji, budowa równań, graf zależności
    solver/         ConstraintSolver, skalowanie, algebra liniowa
    operations/     podział, usuwanie, konwersje, transakcje
    demo/           DemoSceneBuilder
  editor/
    EditorSession, EditCommand, Selection, InteractionState
    HitTester, Viewport, EditorView
  render/
    SceneRenderer, RenderFrame, Style, Bresenham
  platform/win32/
    WinMain, MainWindow, AppState, InputAdapter
    GdiBackend, GdiResources, BackBuffer, dialogi
tests/
  core/             geometria, relacje, solver, topologia
  editor/           gesty i transakcje bez okna
docs/
  architecture/     ten plan
  user-guide.md     późniejsza instrukcja obsługi
  algorithms.md     późniejszy opis faktycznie wdrożonych algorytmów
```

Nazwy w drzewie oznaczają odpowiedzialności, nie nakaz tworzenia osobnego pliku dla każdego drobnego typu. Łączymy niewielkie, powiązane struktury w czytelne nagłówki. Nagłówki publiczne nie zawierają `using namespace` ani zależności od `framework.h`.

## Jednostki budowania

Docelowo `cad_core`, `cad_editor` i `cad_render` to przenośne biblioteki statyczne. Aplikacja Windows linkuje je i kompiluje adapter WinAPI/GDI. Testy rdzenia nie linkują bibliotek Windows.

W pierwszym kroku zachowujemy obecny projekt Visual Studio i dodajemy projekty bibliotek do rozwiązania, wspólnie ustawiając C++20 oraz poziom ostrzeżeń. Przenosimy szablon z `Projecr1_GK.cpp` do adaptera okna stopniowo, pozostawiając jeden punkt wejścia `wWinMain` i istniejące zasoby `.rc`.

Oddzielne targety pomagają egzekwować granice zależności już podczas kompilacji. CMake można dodać, gdy będzie potrzebne budowanie rdzenia poza MSVC; nie jest warunkiem rozpoczęcia pracy ani częścią funkcji wymaganych w zadaniu. Jeśli włączymy PCH w aplikacji, nie narzucamy nagłówka Windows bibliotekom przenośnym.

## Scena demonstracyjna

`DemoSceneBuilder` udostępnia krótkie operacje na danych i zwraca ID do dalszej konfiguracji. Przykładowy, orientacyjny zapis:

```cpp
auto first = builder.addPolygon({{80, 80}, {260, 80}, {240, 220}, {80, 200}});
auto second = builder.addPolygon({{360, 80}, {540, 80}, {540, 220}, {360, 220}});
builder.setBezier(first.edge(2));
builder.addFixedLength(first.edge(0), 180.0);
builder.addParallel(first.edge(0), second.edge(0));
return builder.buildValidated();
```

Builder dodaje ustawienia do kandydata, a `buildValidated()` rozwiązuje zależności całej konfiguracji i zwraca jawny błąd, jeśli scena nie jest poprawna. Powyższy zapis jest szkicem API, nie zweryfikowaną numerycznie sceną. Finalna scena powinna dodatkowo pokazywać łuk oraz różne klasy ciągłości; każdy przypadek musi przejść solver przed uruchomieniem interakcji.

## Etapy i kryteria ukończenia

| Etap | Zakres | Kryterium |
| --- | --- | --- |
| 1. Rdzeń i granice | ID, dokument, geometria, walidator, biblioteki, scena dwóch wielokątów | Rdzeń kompiluje się bez nagłówków Windows; dokument przechodzi walidację |
| 2. Wczesny prototyp solvera | Długość, równoległość, dokładny punkt prowadzony, proste G1 Béziera | Testy obejmują sprzężone relacje, różne wielokąty i brak zbieżności |
| 3. Edycja odcinków | Wybór elementu, drag, podział, usuwanie, polecenia i transakcje | Modyfikacja nie zostawia niepoprawnych ID; anulowanie odtwarza dokument |
| 4. Bézier i ciągłość | Uchwyty, G0/G1/C1, konwersje, podział, sampler przyrostowy | Ruch uchwytów i wierzchołków zachowuje wymagane relacje w wykonalnych scenach |
| 5. Łuki | Parametr łuku, G0/G1, konwersje, dopuszczalne ustawienia ciągłości obu końców | Półokrąg dla G0/G0 i jawne odrzucanie niedozwolonych kombinacji |
| 6. Rysowanie i UI | GDI, DIB, Bresenham, radio buttony, dialogi i oznaczenia | Oba algorytmy są wybieralne; wszystkie wymagane operacje dostępne z UI |
| 7. Demonstracja i dokumentacja | Scena końcowa, F1, opis założeń i algorytmów | Każdy punkt specyfikacji ma scenariusz demonstracji |

Prosty renderer GDI powstaje już przy etapie 1–3, aby dało się obserwować geometrię; etap 6 domyka rasteryzację i integrację. Wariant A–K nie wymaga prostopadłości, więc nie odkładamy niezbędnych funkcji na rzecz dodatkowej relacji.

## Weryfikacja

Testujemy zachowanie, nie układ prywatnych klas:

- ID i topologia pozostają spójne po podziale/usunięciu; znikają relacje na usuniętych krawędziach, również między wielokątami.
- Dokładny punkt prowadzony nie jest przesuwany przez solver. Dodanie niewykonalnej relacji nie zmienia dokumentu.
- Długość i równoległość są zachowane przy wspólnej edycji; uwzględniamy układy niedookreślone i nadmiarowe.
- Długość i równoległość można przypisać tylko odcinkom. Walidator odrzuca próbę nadania ich Bézierowi lub łukowi, również przez API bez UI.
- Konwersja odcinka na krzywą usuwa jego relacje, także między wielokątami, i pozostawia pozostałe relacje bez zmian. Nieudana konwersja przywraca usunięte relacje; powrót do odcinka nie odtwarza ich automatycznie.
- G1 sprawdza także zwrot, a C1 wartość pochodnej; testujemy styki Bézier–Bézier i odcinek–Bézier.
- Próbki Béziera są zgodne z bezpośrednią ewaluacją w granicach tolerancji; końce są dokładne. Bresenham obejmuje wszystkie oktanty i odcinek zerowej długości.
- Łuk G0/G0 ma wymagany promień; niedopuszczalna ciągłość i geometria zdegenerowana zwracają wynik diagnostyczny.
- Anulowanie gestu i nieudane operacje odtwarzają stan. Powiększenie widoku nie zmienia geometrii ani wyniku weryfikacji relacji.

Ręcznie sprawdzamy przechwycenie myszy poza oknem, utratę aktywności, DPI, zmianę rozmiaru, dialogi, usuwanie oznaczeń i przełączanie algorytmu. Dokumentację aktualizujemy do faktycznej implementacji, zwłaszcza jej tolerancji i ograniczeń solvera.
