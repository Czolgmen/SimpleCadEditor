# Rysowanie i backend GDI

[Spis treści](README.md)

## Przygotowanie klatki

`SceneRenderer` przyjmuje `EditorView` oraz parametry widoku i przygotowuje `RenderFrame`: listę prymitywów z pozycjami ekranowymi i stylem. Typy prymitywów mogą obejmować odcinek, łamaną, serię pikseli, znacznik i tekst. Nie przechowują uchwytów GDI.

Renderowanie przebiega warstwami:

1. Krawędzie wielokątów.
2. Oznaczenia ograniczeń odcinków z etykietami długości i parami symboli równoległości.
3. Wieloboki kontrolne Béziera, linią przerywaną.
4. Wierzchołki, punkty kontrolne, zaznaczenie i oznaczenia ciągłości.
5. Podgląd przeciągania i diagnostyka niespełnionych relacji.

Oznaczenia wielu relacji na jednym odcinku otrzymują kolejne odsunięcia od jego środka, aby się nie pokrywały. Oznaczenia przechowują powiązane `ConstraintId`, co umożliwia wybór i usunięcie relacji. Na Bézierach i łukach nie rysujemy oznaczeń długości ani równoległości; oznaczenia ciągłości pozostają przy ich wierzchołkach.

## Algorytmy

Gotowy algorytm w `src/render/Bresenham.h/.cpp` przyjmuje całkowite PixelPoint i dopisuje piksele do przekazanego wektora, umożliwiając ponowne wykorzystanie jego pamięci. Działa bez Windows i geometrii dokumentu, a źródło jest podpięte do aplikacji oraz testów. Testy obejmują wszystkie oktanty i niezależne sprawdzenie względem równania prostej. Gotowe src/render/LineClipping.h/.cpp przycina odcinki Cohena–Sutherlanda we współrzędnych ekranowych double do [0, width-1] × [0, height-1], przed zaokrągleniem do pikseli. Kod jest podpięty do aplikacji i niezależnie sprawdzany w testach; błędne rozmiary, niefinitywne wejście lub przecięcie są odrzucane. Gotowa funkcja drawBresenhamLine w src/platform/win32/DibRenderer.h/.cpp łączy przycinanie, zaokrąglanie, Bresenhama i zapis koloru 0x00RRGGBB do pamięci BackBuffer. Wektor roboczy zachowuje pojemność między odcinkami; synchronizacja GdiFlush przed zapisem CPU należy do właściciela partii rysowania. Bezpośredni zapis do DIB jest sprawdzony w testach WinAPI. Renderer korzysta z tej funkcji dla odcinków oraz próbek Béziera i łuków. Natywne radio buttony GDI/Bresenham na pasku nad sceną zmieniają wyłącznie AppState.lineAlgorithm i zlecają odmalowanie; domyślny jest GDI. Krawędzie są rysowane przed dekoracjami, z jednym GdiFlush przed całą partią zapisów CPU. Własny tryb używa kwadratowego pędzla o szerokości 2 pikseli dla zwykłej krawędzi i 4 dla zaznaczenia lub wyboru równoległości; kolor i priorytet wyróżnienia są zachowane. Prostokąt sceny ogranicza również pędzel, więc piksele nie nadpisują paska. Przy braku odpowiedniego bufora pozostaje rysowanie GDI. Współrzędne odcinków obu trybów są przycinane przed konwersją na liczby całkowite; niewidoczne znaczniki pomijają tę konwersję.

| Element | Implementacja |
| --- | --- |
| Odcinek, tryb biblioteczny | Backend GDI: `MoveToEx` / `LineTo` lub `Polyline` |
| Odcinek, tryb własny | Całkowitoliczbowy Bresenham dla wszystkich oktantów |
| Bézier | Próbkowanie przyrostowe w rdzeniu, następnie połączenie próbek |
| Łuk | Próbkowanie z parametryzacji okręgu, następnie połączenie próbek |
| Uchwyty i punkty | Proste znaczniki backendu |

Przełącznik algorytmu dotyczy odcinków geometrii, w tym odcinków łamanych aproksymujących krzywe. Próbkowanie Béziera zawsze pozostaje przyrostowe. Linie pomocnicze i obrysy znaczników mogą używać biblioteki, aby nie mieszać wymaganego algorytmu geometrii z dekoracjami UI.

Bresenham znajduje się poza adapterem Windows. Zwraca piksele lub ich serie do neutralnego celu rasteryzacji. Nie wywołuje `SetPixel` dla każdego piksela. Backend zapisuje wynik do pamięci bufora DIB; GDI wykonuje pozostałe polecenia na tym samym buforze. Backend ustala poprawną synchronizację dostępu GDI i CPU, w razie potrzeby przez `GdiFlush` przed zapisem CPU.

Przed rasteryzacją przycinamy geometrię do widoku, aby duże współrzędne nie powodowały nieograniczonych pętli ani przepełnień obliczeń całkowitoliczbowych. Sam odcinek zerowej długości może być narysowany jako punkt, chociaż nie stanowi poprawnej krawędzi dla relacji wymagającej kierunku.

## Buforowanie i zasoby

`GdiBackend` posiada `MemoryDC` oraz 32-bitowy, odgórny `DIBSection`. Przy zmianie rozmiaru odtwarza bufor, a przy `WM_PAINT` rysuje całą klatkę i kopiuje ją na ekran przez `BitBlt`. Pełne odmalowanie wystarczy dla małej sceny; przyrostowe obszary odświeżania są opcjonalne.

Obecna implementacja wydziela te zasoby do [klasy `BackBuffer`](../../src/platform/win32/BackBuffer.h), posiadanej przez `AppState`. `WM_PAINT` ponownie wykorzystuje bufor lub dopasowuje go do nowego rozmiaru, czyści tło i rysuje scenę. `WM_ERASEBKGND` nie wykonuje osobnego czyszczenia ekranu. Szczegóły cyklu życia opisuje [dokumentacja adaptera Windows](../../src/platform/win32/README.md).

Małe klasy RAII opakowują obiekty GDI: pióra, pędzle, bitmapy, fonty i przywracanie poprzednio zaznaczonego obiektu DC. Obiektu zaznaczonego do DC nie wolno usuwać przed przywróceniem poprzedniego. Zasoby systemowe pożyczone od Windows nie są traktowane jak własne obiekty do usunięcia.

Adapter pilnuje zgodności współrzędnych wejścia i renderera przy DPI innym niż 100%. Rysowanie i hit testing korzystają z tej samej transformacji i tolerancji próbkowania. Nie zapisujemy współrzędnych ekranowych do dokumentu.

## Nowy backend

Inny frontend może wykonać `RenderFrame` własnym API lub przygotować własną wizualizację z `EditorView`. Aby zachować wybór algorytmu z zadania, backend powinien obsłużyć zarówno odcinki biblioteczne, jak i przekazane piksele. GDI pozostaje szczegółem aktualnej aplikacji.
