# Warstwa WinAPI/GDI

```text
src/platform/win32/
  AppState.h
  BackBuffer.h
  BackBuffer.cpp
  GdiRenderer.h
  GdiRenderer.cpp
```

Pliki należą do projektu aplikacji `Projecr1_GK`, a nie biblioteki `cad_core`. Filtry Visual Studio odzwierciedlają tę strukturę. Katalog `src` jest dodany do ścieżek nagłówków aplikacji i IntelliSense.

## Odpowiedzialności

- `GdiRenderer.h/.cpp`: `cad::win32::drawDocument` rysuje krawędzie i znaczniki wierzchołków oraz wyróżnia przekazane ID zaznaczonego wierzchołka, krawędzi lub wielokąta. `worldToScreen` przelicza współrzędne świata na ekran, ze skalą 1:1, marginesem i odwróconą osią Y.
- `AppState.h`: posiada dokument sceny, bufor obrazu oraz stan zaznaczenia i przeciągania. Stan powstaje raz w `wWinMain`, a wskaźnik jest przekazywany do okna przez `WM_NCCREATE` i przechowywany w `GWLP_USERDATA`.
- `BackBuffer.h/.cpp`: posiada pamięciowy DC i odgórny, 32-bitowy `DIBSection`. Automatycznie przywraca poprzednią bitmapę DC i zwalnia swoje zasoby. Kopiowanie i przenoszenie tego właściciela uchwytów są wyłączone.

Przykład dołączania nagłówka z kodu aplikacji:

```cpp
#include <platform/win32/GdiRenderer.h>
```

## Podział krawędzi

Kliknięcie PPM w obszarze rysowania wskazuje element w kolejności: wierzchołek, krawędź, wnętrze wielokąta. Otwiera menu kontekstowe z poleceniem „Podziel krawędź”, aktywnym dla zaznaczonej krawędzi. PPM na pustym tle usuwa zaznaczenie i pokazuje nieaktywne polecenie. Shift+F10 otwiera menu dla bieżącego zaznaczenia bez zmiany wskazanego elementu.

Polecenie wywołuje `Document::splitEdge`. Po sukcesie czyści stan gestu, zaznacza nowy wierzchołek i zleca przerysowanie. Podział następuje w połowie boku, niezależnie od miejsca kliknięcia na nim. Utworzone menu jest zwalniane przez `DestroyMenu` po jego zamknięciu.

## Usuwanie wierzchołka

Zaznaczony wierzchołek można usunąć klawiszem Delete albo poleceniem „Usuń wierzchołek” w menu PPM. Polecenie jest aktywne dla wierzchołka figury mającej więcej niż trzy punkty. Delete przy innym zaznaczeniu lub na trójkącie nie zmienia dokumentu; pozycja menu jest wtedy nieaktywna.

`Document::deleteVertex` usuwa punkt i obie dochodzące krawędzie, łączy sąsiadów nowym odcinkiem i zachowuje kolejność pozostałych elementów. Po sukcesie aplikacja zwalnia przechwycenie myszy, czyści zaznaczenie oraz gest i zleca przerysowanie.

## Przebieg odmalowania

Punkt wejścia i procedura okna są w `Projecr1_GK.cpp`. `WM_PAINT`, pomiędzy `BeginPaint` i `EndPaint`:

1. Pobiera aktualny obszar klienta i przygotowuje pasujący bufor przez `BackBuffer::resize`.
2. Wypełnia całe tło bufora systemowym kolorem `COLOR_WINDOW`.
3. Wywołuje `drawDocument` dla DC bufora.
4. Przenosi gotową klatkę na ekran przez `BackBuffer::present`, używające `BitBlt`.

Bufor tego samego rozmiaru jest używany ponownie. `WM_SIZE` zleca odmalowanie, a nowy rozmiar bitmapy jest ustawiany przy następnym `WM_PAINT`. Minimalizacja zwalnia bufor; po przywróceniu okna jest on tworzony ponownie.

Tło jest częścią gotowej klatki, więc klasa okna nie ma własnego pędzla tła, a `WM_ERASEBKGND` potwierdza obsłużenie komunikatu bez osobnego wypełniania okna. Jeśli alokacja bufora lub kopiowanie klatki zawiedzie, `WM_PAINT` czyści i rysuje bezpośrednio na ekranie.

`WM_NCDESTROY` zwalnia bufor i usuwa wskaźnik do stanu z danych okna. Destruktor `BackBuffer` również zwalnia zasoby, a wielokrotne wywołanie `reset()` jest bezpieczne.
