# Warstwa WinAPI/GDI

```text
src/platform/win32/
  AppState.h
  GdiRenderer.h
  GdiRenderer.cpp
```

Pliki należą do projektu aplikacji `Projecr1_GK`, a nie biblioteki `cad_core`. Filtry Visual Studio odzwierciedlają tę strukturę. Katalog `src` jest dodany do ścieżek nagłówków aplikacji i IntelliSense.

## Miejsca na implementację

- `GdiRenderer.h`: deklaracja `cad::win32::drawDocument(HDC, const cad::Document&)`.
- `GdiRenderer.cpp`: szkielet funkcji, obecnie bez rysowania. Tutaj dopisz przechodzenie po krawędziach i wierzchołkach oraz pomocniczą funkcję `worldToScreen`.
- `AppState.h`: miejsce na stan aplikacji posiadający dokument sceny, tworzony raz podczas inicjalizacji.

Przykład dołączania nagłówka z kodu aplikacji:

```cpp
#include <platform/win32/GdiRenderer.h>
```

Po zaimplementowaniu renderera trzeba jeszcze przechować scenę w stanie aplikacji i wywołać `cad::win32::drawDocument` w `WM_PAINT`, pomiędzy `BeginPaint` i `EndPaint`. Punkt wejścia i procedura okna pozostają na tym etapie w `Projecr1_GK.cpp`.
