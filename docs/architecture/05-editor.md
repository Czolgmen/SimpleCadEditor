# 05. Sesja edytora i interakcja

[Spis dokumentacji architektury](README.md)

## Stan sesji

[`EditorSession`](../../src/editor/EditorSession.h) łączy dokument z zaznaczeniem, stanem przeciągania oraz wyborem pierwszej krawędzi relacji równoległości. Interfejs operuje na pozycjach i identyfikatorach, bez uchwytów okien Windows. [`Selection`](../../src/editor/Selection.h) i [`DragState`](../../src/editor/DragState.h) opisują bieżący wybór oraz dane gestu, w tym migawkę początkową i przesunięcie miejsca uchwycenia względem kursora.

## Wykrywanie elementów

[`HitTester.cpp`](../../src/editor/HitTester.cpp) realizuje testy w kolejności: punkty kontrolne, wierzchołki, krawędzie, wnętrza wielokątów. Najbliższa krawędź jest wybierana przez porównanie kwadratów odległości od segmentów jej łamanej. Wnętrze wyznacza ray casting z regułą parzystości. Przy nakładaniu figur test wnętrza uwzględnia kolejność przeciwną do kolejności ich rysowania.

Tolerancja wskazania jest niezależna od grubości obrysu. Test krawędzi wykorzystuje przybliżenie krzywej przez wspólną funkcję `sampleEdge`.

## Cykl przeciągania

1. Naciśnięcie LPM identyfikuje element i zapisuje dokument sprzed gestu.
2. Ruch kursora wyznacza proponowaną pozycję z zachowaniem miejsca uchwycenia.
3. Próba ruchu wierzchołka lub uchwytu uruchamia solver na ostatnim zaakceptowanym stanie. Prowadzony punkt jest dokładnie przypięty.
4. Poprawny wynik staje się podglądem. Nieudana próbka zachowuje poprzedni podgląd i zgłasza ostrzeżenie.
5. Zwolnienie LPM zatwierdza ostatni poprawny stan. Esc odtwarza migawkę sprzed gestu.

Ruch uchwytu najpierw preferuje nieruchome końce jego krzywej, a następnie dopuszcza ich korektę w ramach wspólnego limitu iteracji. Przesunięcie całego wielokąta jest sztywną translacją obliczaną względem początku gestu; obejmuje również punkty kontrolne.

## Polecenia transakcyjne

Dodawanie i usuwanie relacji, konwersje, podział oraz usuwanie wierzchołka są wykonywane na kandydacie. Zatwierdzenie obejmuje zmianę modelu i ponowne rozwiązanie pozostałych zależności. Odrzucenie nie zużywa identyfikatorów ani nie pozostawia częściowych zmian. Grubość krawędzi jest właściwością prezentacji i nie wymaga rozwiązania geometrii.

## Adapter WinAPI

[`WndProc`](../../Projecr1_GK.cpp) przelicza współrzędne myszy, zarządza przechwyceniem i wywołuje sesję. Podczas przeciągania kolejne ruchy zastępują oczekującą próbkę; timer przelicza najnowszą pozycję. Żądany interwał 1 ms nie jest gwarancją częstotliwości systemowej. Zwolnienie przycisku wymusza obsługę końcowej próbki. Utrata przechwycenia, fokusu lub minimalizacja anuluje gest.

Zapis i odczyt wykorzystują dialogi platformowe. Poprawne wczytanie zastępuje sesję nowym dokumentem i zeruje stan edycji; wybrana metoda renderowania pozostaje zachowana.
