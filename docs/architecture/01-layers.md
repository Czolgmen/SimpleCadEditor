# 01. Warstwy i zależności

[Spis dokumentacji architektury](README.md)

## Podział odpowiedzialności

| Warstwa | Lokalizacja | Odpowiedzialność |
| --- | --- | --- |
| Rdzeń | `cad_core/` | Geometria, dokument, walidacja, ograniczenia, solver, serializacja i scena demonstracyjna |
| Edytor | `src/editor/` | Zaznaczenie, wykrywanie wskazanych elementów, przeciąganie i transakcyjne polecenia |
| Rasteryzacja | `src/render/` | Przycinanie odcinków oraz wyznaczanie pikseli Bresenhama i Wu bez zależności od Windows |
| Adapter Windows | `src/platform/win32/`, `Projecr1_GK.cpp` | Obsługa komunikatów, dialogi, GDI, DIB, prezentacja i pliki |

Rdzeń nie korzysta z WinAPI. Edytor zależy od publicznego interfejsu rdzenia, a algorytmy rasteryzacji operują na współrzędnych i danych pikseli. Adapter Windows łączy te części. Zależności nie prowadzą z rdzenia do interfejsu.

```text
WinAPI / AppState
    ├── EditorSession → Document → geometria i solver
    ├── GdiRenderer / DibRenderer → Document i sampleEdge
    └── SceneFiles → SceneIO → Document
```

## Własność i czas życia

[`AppState`](../../src/platform/win32/AppState.h) posiada sesję edytora, bufor obrazu, wybraną metodę renderowania i stan adaptera okna. [`EditorSession`](../../src/editor/EditorSession.h) posiada dokument, zaznaczenie i stan gestu. [`Document`](../../cad_core/include/cad_core/model/Document.h) posiada encje oraz ograniczenia; odwołania między encjami wykorzystują identyfikatory.

Program działa w jednym wątku. Zdarzenie wejściowe inicjuje zmianę stanu; solver kończy obliczenia przed publikacją poprawnego dokumentu. `WM_PAINT` odczytuje gotowy stan i nie uruchamia solvera.

## Możliwość wymiany interfejsu

Nowy interfejs powinien przekazywać do sesji zdarzenia wskaźnika we współrzędnych świata, realizować dialogi i wyświetlać dokument. Model, reguły edycji, format sceny i obliczenia numeryczne pozostają wspólne. Obecna implementacja GDI/DIB wymaga zastąpienia kodem właściwym dla docelowej platformy; projekt nie zawiera gotowego drugiego interfejsu.
