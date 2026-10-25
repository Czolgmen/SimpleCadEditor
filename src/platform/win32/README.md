# Warstwa WinAPI/GDI

```text
src/platform/win32/
  AppState.h
  BackBuffer.h
  BackBuffer.cpp
  GdiRenderer.h
  GdiRenderer.cpp
  LengthDialog.h
  LengthDialog.cpp
```

Pliki należą do projektu aplikacji `Projecr1_GK`, a nie biblioteki `cad_core`. Filtry Visual Studio odzwierciedlają tę strukturę. Katalog `src` jest dodany do ścieżek nagłówków aplikacji i IntelliSense.

## Odpowiedzialności

- `GdiRenderer.h/.cpp`: `cad::win32::drawDocument` rysuje krawędzie i znaczniki wierzchołków oraz wyróżnia przekazane ID zaznaczonego wierzchołka, krawędzi lub wielokąta. `worldToScreen` przelicza współrzędne świata na ekran, ze skalą 1:1, marginesem i odwróconą osią Y.
- `AppState.h`: posiada przenośną [`EditorSession`](../../editor/README.md) oraz bufor obrazu. Sesja posiada dokument, zaznaczenie i stan gestu. Stan aplikacji powstaje raz w `wWinMain`, a wskaźnik jest przekazywany do okna przez `WM_NCCREATE` i przechowywany w `GWLP_USERDATA`.
- `BackBuffer.h/.cpp`: posiada pamięciowy DC i odgórny, 32-bitowy `DIBSection`. Automatycznie przywraca poprzednią bitmapę DC i zwalnia swoje zasoby. Kopiowanie i przenoszenie tego właściciela uchwytów są wyłączone.
- `LengthDialog.h/.cpp`: modalny dialog długości z zasobu `IDD_FIXED_LENGTH`. Wyświetla aktualną długość, obsługuje OK, Anuluj i zamknięcie okna, a tekst przekazuje do przenośnego `editor::parsePositiveLength`.

Przykład dołączania nagłówka z kodu aplikacji:

```cpp
#include <platform/win32/GdiRenderer.h>
```

## Obsługa gestów

`WndProc` przelicza współrzędne myszy na świat i przekazuje je do `EditorSession::pointerDown`, `pointerMove` i `pointerUp`. Przechwycenie myszy należy do adaptera WinAPI. Puszczenie przycisku przekazuje też ostatnią pozycję kursora, kończy gest i dopiero potem zwalnia przechwycenie.

Podczas gestu `WM_MOUSEMOVE` zapamiętuje najnowszą pozycję w `AppState`, zastępując starszą próbkę. Timer z interwałem 16 ms przekazuje ją do `pointerMove` i wywołuje `UpdateWindow`, aby podgląd został odmalowany przed następną serią komunikatów myszy. Puste takty nie uruchamiają solvera ani odmalowania. Interwał określa częstotliwość zlecania pracy, a nie gwarantowany czas obliczeń lub liczbę klatek. Puszczenie przycisku przelicza końcową pozycję od razu, również przed pierwszym taktem. Anulowanie, utrata przechwycenia i zamknięcie okna usuwają oczekującą próbkę oraz timer; spóźnione `WM_TIMER` są ignorowane. Przy nieudanym `SetTimer` pozostaje natychmiastowa obsługa ruchu. Ograniczenia i transakcje pozostają w przenośnej sesji; rdzeń nie zna zegara Windows.

Esc anuluje ruch do stanu sprzed gestu; poza gestem usuwa zaznaczenie. Nieoczekiwane `WM_CAPTURECHANGED`, `WM_CANCELMODE`, utrata fokusu i minimalizacja również anulują gest. Zwykłe `ReleaseCapture` po zatwierdzeniu nie wycofuje zmian. Rysowanie odczytuje dokument i zaznaczenie z sesji.

## Podział krawędzi

Kliknięcie PPM w obszarze rysowania wskazuje element w kolejności: wierzchołek, krawędź, wnętrze wielokąta. Otwiera menu kontekstowe z poleceniem „Podziel krawędź”, aktywnym dla zaznaczonej krawędzi. PPM na pustym tle usuwa zaznaczenie i pokazuje nieaktywne polecenie. Shift+F10 otwiera menu dla bieżącego zaznaczenia bez zmiany wskazanego elementu.

Polecenie wywołuje `EditorSession::splitSelectedEdge`, która korzysta z `Document::splitEdge`. Po sukcesie sesja kończy gest i zaznacza nowy wierzchołek, a adapter zwalnia przechwycenie myszy i zleca przerysowanie. Odcinek jest dzielony w połowie, a Bézier dla t=0.5 algorytmem de Casteljau, niezależnie od miejsca kliknięcia. Sesja przypina nowy punkt i rozwiązuje relacje przed zatwierdzeniem; przy niepowodzeniu zachowuje dokument i zaznaczenie. Utworzone menu jest zwalniane przez `DestroyMenu` po jego zamknięciu.

## Usuwanie wierzchołka

Zaznaczony wierzchołek można usunąć klawiszem Delete albo poleceniem „Usuń wierzchołek” w menu PPM. Polecenie jest aktywne dla wierzchołka figury mającej więcej niż trzy punkty. Delete przy innym zaznaczeniu lub na trójkącie nie zmienia dokumentu; pozycja menu jest wtedy nieaktywna.

Adapter wywołuje `EditorSession::deleteSelectedVertex`. Sesja korzysta z `Document::deleteVertex`, które usuwa punkt i obie dochodzące krawędzie, łączy sąsiadów nowym odcinkiem i zachowuje kolejność pozostałych elementów. Usunięcie jest wykonywane na kopii dokumentu, a następnie solver rozwiązuje pozostałe relacje z poprzednim sąsiadem przypiętym w dotychczasowym miejscu. Dopiero po zbieżności sesja zatwierdza dokument, czyści zaznaczenie i kończy gest, a adapter zwalnia przechwycenie myszy i zleca przerysowanie. Nieudane rozwiązanie nie usuwa wierzchołka.

## Stała długość krawędzi

PPM na krawędzi otwiera polecenie „Ustaw stałą długość...”. Dla krawędzi z relacją jest to „Zmień stałą długość...”; dostępne jest też „Usuń ograniczenie długości”. Polecenia pozostają nieaktywne dla innych zaznaczeń. Shift+F10 działa dla bieżącego zaznaczenia krawędzi.

Dialog domyślnie podaje aktualną długość geometryczną krawędzi. Akceptuje dodatnie liczby skończone, kropkę albo przecinek dziesiętny i zapis wykładniczy. Niepoprawny tekst pozostawia okno otwarte z komunikatem, a Anuluj, Esc i zamknięcie okna nie zmieniają modelu. Pierwszy koniec krawędzi pozostaje w miejscu, a pozostałe punkty dopasowuje solver. Brak zbieżności pozostawia dokument bez zmian i wyświetla komunikat.

Aktywną relację oznacza zielona etykieta `L=...` przy środku krawędzi. Renderer przywraca poprzednią czcionkę, kolor tekstu i tryb tła DC. Usunięcie relacji usuwa etykietę i pozostawia geometrię; podział krawędzi lub usunięcie wierzchołka czyści relacje zmienionych krawędzi. Przeciąganie końca odcinka uruchamia solver, zachowując prowadzony punkt pod kursorem. Przy odrzuconym ruchu pozostaje ostatni poprawny podgląd.

## Równoległość krawędzi

PPM na krawędzi → „Ustaw równoległość z...” rozpoczyna wybór drugiej krawędzi przez LPM. Pierwsza jest pomarańczowa; Esc, PPM lub utrata fokusu anulują wybór. Zatwierdzenie przez `EditorSession` dodaje parę dopiero po sukcesie wspólnego solvera. Obie krawędzie dostają wspólne oznaczenie `P1`, `P2` itd.

Podmenu „Usuń równoległość” zawiera wyłącznie pary wskazanej krawędzi, oznaczone tymi samymi numerami co na scenie. Brak relacji oznacza nieaktywne polecenie. Można otworzyć menu przez PPM na dowolnej krawędzi pary albo Shift+F10 dla bieżącego zaznaczenia. Adapter zapamiętuje ID drugich krawędzi na czas otwarcia menu i przekazuje wybraną do `removeSelectedEdgeParallel`. Dynamiczne identyfikatory pozycji są obsługiwane bezpośrednio po `TrackPopupMenu`, przed 16-bitowym pakowaniem `WM_COMMAND`. Rodzic zwalnia podmenu przy `DestroyMenu`.

Usunięcie jednej pary nie uruchamia solvera, nie przesuwa punktów i zachowuje długości oraz pozostałe pary. Przerysowanie usuwa jej oznaczenia i odświeża numerację ocalałych relacji.

## Konwersja odcinek ↔ Bézier

Menu PPM i Shift+F10 zawiera „Zamień na Béziera” dla zaznaczonego odcinka i „Zamień na odcinek” dla krzywej. Przy innym zaznaczeniu obie pozycje są nieaktywne. Udana konwersja zachowuje ID i zaznaczenie, kończy interakcję oraz zleca odmalowanie. Odrzucenie wyświetla komunikat i zachowuje dokument. Konwersja na Béziera usuwa długość oraz równoległości tej krawędzi; powrót nie odtwarza ich automatycznie.

`GdiRenderer` używa wspólnego z hit testami `sampleEdge`: dla Béziera łączy domyślne 101 próbek w 100 odcinków, rysuje szary przerywany wielobok kontrolny i kwadratowe uchwyty o promieniu 5. Pióro pomocnicze jest tworzone raz na klatkę i zwalniane po przywróceniu poprzedniego pióra. Początkowe uchwyty leżą w jednej i dwóch trzecich odcinka, więc po konwersji geometria wygląda nadal jak prosta. Na Bézierze niedostępne są długość i rozpoczynanie równoległości; podział krzywej jest dostępny. LPM wybiera uchwyt przed wierzchołkiem i rozpoczyna jego przeciąganie; wybrany uchwyt ma niebieski obrys. Esc i utrata przechwycenia odtwarzają poprzednią geometrię. Przesuwanie całej figury obejmuje jej uchwyty. Podział krzywej używa de Casteljau, a sesja rozwiązuje G1/C1 przed zatwierdzeniem.

## Powiadomienie o odrzuconym ruchu

Jeśli sesja odrzuci położenie podczas przeciągania, adapter pokazuje na dole sceny nieblokujący pasek: „Nie udało się uzyskać spójnego stanu. Zachowano ostatni poprawny stan. Spróbuj innego położenia.” Nie otwiera dialogu i nie zwalnia przechwycenia myszy, więc można kontynuować ruch. Poprawny podgląd usuwa powiadomienie. Przy puszczeniu przycisku na odrzuconym położeniu pasek pozostaje widoczny; znika po rozpoczęciu następnej operacji, Esc lub anulowaniu. Utrata fokusu i przechwycenia podczas gestu również go usuwa.

Stan powiadomienia należy do `AppState`; rdzeń i przenośna sesja nie znają komunikatu ani GDI. Pasek jest rysowany po geometrii do tego samego bufora, z przywróceniem ustawień DC. Nieudane dodanie równoległości lub ustawienie długości nadal używa pojedynczego dialogu z informacją o zachowaniu dokumentu.

## Ciągłość wierzchołka

PPM na wierzchołku przy przynajmniej jednym Bézierze udostępnia podmenu „Ciągłość wierzchołka”. Shift+F10 otwiera je dla bieżącego zaznaczenia. Opcje mają wzajemnie wykluczające się oznaczenie radiowe:

- G0 — wspólny punkt, bez warunku styczności.
- G1 — zgodny kierunek stycznych.
- C1 — zgodne pochodne, wraz z ich długościami.

Adapter pobiera dostępność i aktualną klasę przez `EditorSession::selectedVertexContinuity()`. Dla uchwytu, krawędzi, wnętrza figury, pustego zaznaczenia i wierzchołka pomiędzy dwoma odcinkami podmenu jest nieaktywne. Odczyt menu nie zmienia ustawienia; dopiero wybór polecenia zapisuje jawną decyzję użytkownika.

Polecenia `IDM_CONTINUITY_G0`, `IDM_CONTINUITY_G1` i `IDM_CONTINUITY_C1` wywołują `setSelectedVertexContinuity()`. Sesja przypina zaznaczony wierzchołek i rozwiązuje wszystkie ograniczenia na kopii dokumentu. Po sukcesie adapter zwalnia przechwycenie myszy i zleca przerysowanie. Nieudana zmiana pokazuje jeden komunikat o nieuzyskaniu spójnego stanu; dokument i poprzednia klasa pozostają zachowane. Menu oraz jego podmenu są zwalniane przez `DestroyMenu()`.

## Przebieg odmalowania

Punkt wejścia i procedura okna są w `Projecr1_GK.cpp`. `WM_PAINT`, pomiędzy `BeginPaint` i `EndPaint`:

1. Pobiera aktualny obszar klienta i przygotowuje pasujący bufor przez `BackBuffer::resize`.
2. Wypełnia całe tło bufora systemowym kolorem `COLOR_WINDOW`.
3. Wywołuje `drawDocument` dla DC bufora.
4. Przenosi gotową klatkę na ekran przez `BackBuffer::present`, używające `BitBlt`.

Bufor tego samego rozmiaru jest używany ponownie. `WM_SIZE` zleca odmalowanie, a nowy rozmiar bitmapy jest ustawiany przy następnym `WM_PAINT`. Minimalizacja zwalnia bufor; po przywróceniu okna jest on tworzony ponownie.

Tło jest częścią gotowej klatki, więc klasa okna nie ma własnego pędzla tła, a `WM_ERASEBKGND` potwierdza obsłużenie komunikatu bez osobnego wypełniania okna. Jeśli alokacja bufora lub kopiowanie klatki zawiedzie, `WM_PAINT` czyści i rysuje bezpośrednio na ekranie.

`WM_NCDESTROY` zwalnia bufor i usuwa wskaźnik do stanu z danych okna. Destruktor `BackBuffer` również zwalnia zasoby, a wielokrotne wywołanie `reset()` jest bezpieczne.
