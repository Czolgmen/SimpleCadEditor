# 08. Zakres funkcjonalny i decyzje projektowe

[Spis dokumentacji architektury](README.md)

## Zakres projektu

Implementacja odpowiada wariantowi **A–K**: relacji równoległości odcinków i domyślnej ciągłości G1 przy konwersji na Béziera. Długość i równoległość dotyczą wyłącznie odcinków. Béziery i łuki uczestniczą w warunkach ciągłości. Interfejs jest oparty na WinAPI i GDI, a kod modelu, obliczeń i edycji pozostaje niezależny od Windows.

| Funkcjonalność | Główne miejsce realizacji |
| --- | --- |
| Wiele wielokątów, w tym samoprzecinających się | `Document`, `HitTester` |
| Przesuwanie wierzchołków, uchwytów i całych figur | `EditorSession`, `DragState`, solver |
| Podział i usuwanie wierzchołków | `Document`, transakcje sesji |
| Stała długość z dialogu | `FixedLengthConstraint`, `LengthDialog`, solver |
| Równoległość, także między figurami | `ParallelConstraint`, sesja i solver |
| Jednoczesne relacje i usuwanie pojedynczego ograniczenia | Kontenery relacji w `Document`, menu kontekstowe |
| Bézier trzeciego stopnia i dwa uchwyty | `EdgeGeometry`, `Bezier`, renderer |
| G0/G1/C1, domyślne G1 i jawne ustawienia | `ContinuitySetting`, `ContinuityConstraint` |
| Łuki G0/G1 i analityczne dopasowanie | `Arc`, `ArcContinuity`, walidacja |
| Biblioteczne odcinki i własny Bresenham | `GdiRenderer`, `Bresenham`, `DibRenderer` |
| Przyrostowe próbkowanie Béziera | `sampleBezier` |
| Antyaliasing i indywidualna grubość krawędzi | `Wu`, `Edge::strokeWidth`, renderery |
| Zapisz jako i Załaduj | `SceneIO`, `SceneFiles` |
| Dwie figury demonstracyjne z relacjami i krzywymi | `DemoScene.cpp` |
| Instrukcja i opis algorytmów w aplikacji | `HelpDialog`, zasoby dokumentacji |

## Przyjęte reguły edycji

Konwersja odcinka na krzywą usuwa relacje długości i równoległości tej krawędzi. Powrót do odcinka nie przywraca dawnych relacji. Jawna ciągłość ocalałych wierzchołków jest zachowywana i ponownie rozwiązywana po zmianie sąsiedztwa.

Podział korzysta z parametru `t=0,5`; w przypadku Béziera nie oznacza to połowy długości łukowej. Łuk jest zastępowany dwoma odcinkami przez punkt środkowy parametru. Usunięcie wierzchołka zastępuje obie krawędzie jednym odcinkiem. Figura musi zachować co najmniej trzy wierzchołki.

Niewykonalna próba edycji zachowuje ostatni poprawny stan. Translacja całego wielokąta zachowuje jego kształt i nie przesuwa pozostałych figur. Wnętrze samoprzecinającego się obrysu jest określane regułą parzystości.

## Granice implementacji

Program nie zawiera kreatora nowych wielokątów w interfejsie; sceny mogą być definiowane w kodzie lub wczytywane z pliku. Nie zaimplementowano pełnej historii undo/redo, przyciągania, siatki ani wymiany z zewnętrznymi formatami CAD. Anulowanie obejmuje bieżący gest. Próbkowanie krzywych ma stałą liczbę segmentów, a gęsty solver jest przeznaczony przede wszystkim do niewielkich scen projektu.

Ocena zgodności funkcjonalnej jest wspierana przez [testy automatyczne](../../tests/README.md) i [scenariusz prezentacji](../documentation.md#10-scenariusz-prezentacji). Liczba testów nie stanowi dowodu poprawności dla wszystkich możliwych danych.
