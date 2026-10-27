# 06. Renderowanie

[Spis dokumentacji architektury](README.md)

## Przepływ danych

[`GdiRenderer.cpp`](../../src/platform/win32/GdiRenderer.cpp) odczytuje gotowy dokument i stan zaznaczenia. Krawędzie są próbkowane przez `sampleEdge`, przeliczane na ekran i przycinane do obszaru sceny. Pasek przycisków jest wyłączony z tego obszaru. Najpierw rysowana jest geometria, następnie oznaczenia relacji, punkty kontrolne i wierzchołki.

Wybrana metoda zmienia rasteryzację odcinków, bez zmiany modelu i ograniczeń. Każda krawędź ma niezależną grubość od 1 do 32 px; wyróżnienie zwiększa ją o 2 px. Linie pomocnicze i tekst pozostają elementami GDI.

## Metody rasteryzacji

| Tryb | Implementacja | Charakterystyka |
| --- | --- | --- |
| GDI | `GdiRenderer.cpp` | Odcinki `MoveToEx`/`LineTo` i pióra o szerokości krawędzi |
| Bresenham | [`Bresenham.cpp`](../../src/render/Bresenham.cpp), `DibRenderer.cpp` | Całkowitoliczbowy ślad we wszystkich oktantach, poszerzany kwadratowym pędzlem |
| Wu (AA), 1 px | [`Wu.cpp`](../../src/render/Wu.cpp) | Klasyczny algorytm Xiaolina Wu z ułamkowymi pokryciami sąsiednich pikseli |
| Wu (AA), szerszy obrys | `Wu.cpp`, [`DibRenderer.cpp`](../../src/platform/win32/DibRenderer.cpp) | Geometryczny obrys o zaokrąglonych końcach i pokryciu wyznaczanym z odległości |

W grubym obrysie odległość środka piksela od odcinka wyznacza pokrycie `clamp(width/2+0.5-distance,0,1)`. Maksimum pokryć sąsiednich segmentów tworzy wspólną maskę krawędzi i zaokrąglone połączenia. Grubość jest mierzona prostopadle do odcinka, co ogranicza jej zależność od nachylenia. Jest to rozszerzenie trybu AA, odrębne od klasycznego kroku Wu dla 1 px.

Maska jest mieszana z aktualnym tłem raz na krawędź. Obliczenia obejmują kanały RGB bez korekcji gamma. Bresenham nie stosuje antyaliasingu; kwadratowy pędzel może powodować zależny od kąta wygląd szerokich linii.

## Przycinanie

[`LineClipping.cpp`](../../src/render/LineClipping.cpp) realizuje algorytm Cohena–Sutherlanda na współrzędnych `double`, przed konwersją na liczby całkowite. Rasteryzacja uwzględnia szerokość pędzla lub margines AA, a końcowy zapis pikseli pozostaje ograniczony do obszaru sceny. Pozwala to uniknąć rasteryzacji bardzo długich, niewidocznych fragmentów.

## Bufor obrazu i zasoby

[`BackBuffer`](../../src/platform/win32/BackBuffer.h) posiada kontekst pamięciowy i odgórny, 32-bitowy DIB. Bezpośredni zapis pikseli używa formatu `0x00RRGGBB`. `GdiFlush` synchronizuje operacje GDI przed zapisem procesora, a `BitBlt` prezentuje gotową klatkę. Zmiana rozmiaru odtwarza bufor. W razie jego niedostępności program korzysta z bezpośredniego renderowania GDI.

Pióra są współdzielone podczas rysowania według koloru i szerokości. Przywrócenie stanu kontekstu poprzedza usuwanie utworzonych obiektów GDI. Bufory robocze rasteryzacji są wykorzystywane ponownie; maska AA zeruje po krawędzi jedynie użyte pozycje.
