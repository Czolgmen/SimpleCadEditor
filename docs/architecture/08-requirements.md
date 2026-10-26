# Założenia i pokrycie wymagań

[Spis treści](README.md)

## Ustalenia

- C++20, WinAPI i GDI jako pierwszy frontend.
- Niezależny od Windows rdzeń geometrii, solver oraz obsługa edycji.
- Wariant **A–K**, wskazany przez użytkownika: równoległość i domyślne G1 dla Béziera.
- Zgodnie z doprecyzowaniem użytkownika ograniczenia długości i równoległości dotyczą tylko odcinków. Béziery i łuki, w tym półokręgi, zachowują obsługę ciągłości, bez tych ograniczeń krawędzi.
- Dokumenty architektury zachowują również plany rozbudowy. Bieżący stan i rzeczywiste API opisuje [dokumentacja programu](../documentation.md); skrócona pomoc jest wbudowana w aplikację pod F1.

## Interpretacje wymagające jawnego opisania

Poniższe decyzje są propozycjami projektowymi dla miejsc, których specyfikacja nie rozstrzyga. Weryfikacja ich zgodności z oczekiwaniami prowadzącego może zmienić szczegółowe operacje i równania, ale nie wymaga zmiany podziału na warstwy.

| Temat | Proponowana decyzja | Konsekwencja |
| --- | --- | --- |
| Konwersja odcinka na krzywą | Usunięcie jego relacji długości i równoległości w ramach transakcji | Brak niedozwolonych relacji na krzywych; nieudana konwersja przywraca również relacje |
| Konwersja krzywej na odcinek | Bez automatycznego odtwarzania dawnych relacji | Nowe ograniczenia użytkownik nadaje jawnie |
| C1 | Pochodne dla parametru każdego segmentu w `[0,1]` | Uchwyt przy odcinku ma długość równą jednej trzeciej długości odcinka |
| Podział krawędzi | Punkt dla `t=0.5`, dla odcinka zwykły środek | Nie obliczamy połowy długości łukowej Béziera |
| Podział łuku | Dwa odcinki przez środkowy punkt łuku | Nie powstają dwa łuki sprzeczne z regułą promienia dla G0/G0; operacja zmienia kształt |
| Usuwanie wierzchołka | Zastąpienie dwóch sąsiednich krawędzi jednym odcinkiem | Prosty i przewidywalny model; brak arbitralnego dopasowania nowej krzywej |
| Ciągłość po zmianie sąsiedztwa | Zachowanie jawnych ustawień ocalałych wierzchołków, ponowne rozwiązanie relacji | Zmiana topologii może zostać odrzucona, jeśli nie uzyska poprawnego wyniku |
| Niewykonalny ruch | Pozostaje ostatni poprawny podgląd i ostrzeżenie; puszczenie myszy zachowuje ten podgląd, a Esc cofa cały gest | Nie można matematycznie zagwarantować dowolnego położenia i wszystkich możliwych relacji jednocześnie |
| Przesunięcie wielokąta | Sztywna translacja wskazanego obiektu | Długości, kierunki, ciągłość i bulge pozostają zachowane; inne figury nie są przesuwane |
| Nowe wielokąty | Dodawane czytelnie w kodzie sceny początkowej | Zgodnie z jawnym dopuszczeniem w punkcie 1 zadania, kreator UI nie jest wymagany |
| Wnętrze przy samoprzecięciach | Reguła parzystości | Jednoznaczny wybór wnętrza do przesuwania wielokąta |

Zakres ograniczeń nie jest już niejednoznacznością: długość i równoległość obsługujemy wyłącznie dla odcinków, zgodnie z ustaleniem powyżej. Równania ciągłości pozostają potrzebne dla krzywych i mogą wpływać na ograniczenia sąsiednich odcinków przez wspólne wierzchołki.

## Mapa wymagań

| Wymaganie | Główne miejsce realizacji | Sprawdzenie |
| --- | --- | --- |
| Dowolna liczba wielokątów i samoprzecięcia | `Document`, `DemoSceneBuilder` | Dopisanie kolejnego wielokąta bez zmiany logiki; scena samoprzecinająca |
| Edycja dowolnego wielokąta i przesuwanie całości | `EditorSession`, hit testing, solver | Przesunięcie obu obiektów, także połączonych relacją |
| Ruch wierzchołków i punktów kontrolnych | Stan gestu i dokładnie zadane współrzędne | Ruch prowadzony oraz propagacja do elementów zależnych |
| Podział krawędzi i usuwanie wierzchołka | Operacje topologiczne | Spójne ID i znikanie właściwych relacji |
| Długość odcinka z dialogu i bieżąca wartość domyślna | Dialog WinAPI + `AddFixedLength` | Edycja liczby, odrzucenie wartości niepoprawnej i krawędzi innego typu |
| Równoległość odcinków, również między wielokątami | `Parallel`, graf zależności, solver | Scena dwóch wielokątów i wspólna edycja; odrzucenie krzywych |
| Kilka relacji na jednym odcinku | Rejestr ograniczeń | Długość i równoległość działające jednocześnie |
| Wyświetlanie i usuwanie relacji | `SceneRenderer`, wybór oznaczenia, `RemoveConstraint` | Usunięcie jednej relacji bez naruszenia pozostałych |
| Konwersja odcinek/Bézier i dwa uchwyty | `EdgeGeometry`, konwersja, renderer | Przełączenie typu, usunięcie relacji odcinka i manipulacja uchwytami |
| G0/G1/C1 i domyślne G1 | `ContinuitySetting`, równania ciągłości | Poprawne styczne oraz zachowanie jawnych ustawień |
| Łuki G0/G1 i najwyżej jeden koniec G1 | Geometria łuku i walidator | Półokrąg dla G0/G0, odrzucenie drugiego G1 |
| Biblioteczne odcinki i własny Bresenham | `cad_render`, GDI, radio buttony | Przełączanie algorytmu podczas pracy |
| Przyrostowe rysowanie Béziera | Sampler bazy potęgowej | Porównanie próbek z niezależną ewaluacją |
| Scena początkowa z dwoma wielokątami, relacjami i Bézierem | `DemoSceneBuilder` | Poprawna scena dostępna natychmiast po uruchomieniu |
| Dokumentacja obsługi, modelu i algorytmów | `docs`, polecenie pomocy F1 | Pomoc dostępna z aplikacji i zgodna z implementacją |

## Priorytety zakresu

Najpierw poprawny model, sprzężone relacje, krzywe i wymagana rasteryzacja. Wypełnienia, siatka, przyciąganie, zapis plików, pełna historia undo/redo, kreator wielokątów i dodatkowa prostopadłość pozostają rozszerzeniami.

Zakładamy niewielką scenę demonstracyjną. Nie określamy wydajności dla tysięcy obiektów przed pomiarami. Parametry numeryczne i limity solvera zostaną wybrane na podstawie prototypu i opisane w dokumentacji końcowej.
