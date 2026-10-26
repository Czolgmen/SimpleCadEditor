# Edytor wielokątów — dokumentacja programu

Program służy do edycji wielokątów z prostymi krawędziami, krzywymi Béziera i łukami okręgów. Można przesuwać ich punkty oraz ustawiać warunki, które geometria ma zachować. Moduł obliczający położenia zgodne z tymi warunkami nazywamy dalej solverem.

Projekt realizuje wariant **A–K**: zadaną długość i równoległość odcinków oraz domyślne G1 przy włączeniu Béziera. Jest napisany w C++20. Okna i obsługę myszy zapewnia WinAPI, a rysowanie korzysta z GDI lub własnego Bresenhama. Model dokumentu, geometria i solver są niezależne od Windows.

Rozdziały 1–5 opisują obsługę i przyjęte zasady. Rozdziały 6–8 wyjaśniają kod i algorytmy; można do nich wrócić po zapoznaniu się z programem.

Skrócona pomoc jest wbudowana w aplikację: **F1** lub **Pomoc → Obsługa programu**. Otwiera osobne, skalowalne okno z wyborem tematu, przewijanym tekstem i możliwością kopiowania. Dokumentacja w tym pliku jest osobnym materiałem; do działania pomocy nie jest potrzebny dostęp do niej ani do internetu.

Przycisk **Dokumentacja** na górnym pasku lub **Pomoc → Dokumentacja algorytmów...** otwiera osobne okno pełnej dokumentacji. Lista rozdziałów obejmuje m.in. model danych, algorytmy rysowania i solver; domyślnie wyświetlany jest rozdział o algorytmach geometrycznych. Treść pochodzi z tego pliku, dołączanego do zasobów EXE podczas kompilacji. Pomoc F1 i dokumentację można otworzyć równocześnie. Tekst dokumentacji jest prezentowany bez znaczników Markdown, z zachowaniem wzorów i bloków kodu.

## Spis treści

1. [Uruchomienie i scena demonstracyjna](#1-uruchomienie-i-scena-demonstracyjna)
2. [Mysz, klawisze i zaznaczanie](#2-mysz-klawisze-i-zaznaczanie)
3. [Operacje i ograniczenia](#3-operacje-i-ograniczenia)
4. [Krzywe i ciągłość](#4-krzywe-i-ciągłość)
5. [Założenia i sytuacje wyjątkowe](#5-założenia-i-sytuacje-wyjątkowe)
6. [Model danych i architektura](#6-model-danych-i-architektura)
7. [Algorytmy geometryczne i rysowanie](#7-algorytmy-geometryczne-i-rysowanie)
8. [Mechanizm ograniczeń i solver](#8-mechanizm-ograniczeń-i-solver)
9. [Budowanie, testy i przekazanie programu](#9-budowanie-testy-i-przekazanie-programu)
10. [Scenariusz prezentacji](#10-scenariusz-prezentacji)

## 1. Uruchomienie i scena demonstracyjna

Program automatycznie tworzy dwie figury, każdą o pięciu wierzchołkach. Nie trzeba przygotowywać sceny ręcznie.

| Figura | Elementy demonstracyjne |
| --- | --- |
| Lewa | Górny Bézier z dwoma uchwytami; C1 przy pionowym boku i G1 przy ukośnym; długości dolnego i pionowego odcinka odpowiednio 180 i 120 |
| Prawa | Łuk G1 przy pionowym boku i półokrąg G0/G0; długość dolnego odcinka 160 |
| Obie | Dwie pary równoległości: dolne odcinki oraz pionowe odcinki obu figur |

Wszystkie ustawione warunki są spełnione od razu po uruchomieniu. Wierzchołki, krzywe i uchwyty mieszczą się w obszarze rysowania okna 640×420 pikseli, z miejscem na górny pasek. Przy bardzo małym oknie część sceny może być przycięta; można powiększyć okno.

Scena powstaje w [DemoScene.cpp](../cad_core/src/demo/DemoScene.cpp). Dodanie figury wymaga tablicy jej współrzędnych i wywołania `Document::addPolygon`. Zwrócone ID pozwala ustawić geometrię i relacje jej krawędzi. Przed pokazaniem sceny program sprawdza strukturę dokumentu i dopasowuje geometrię do ustawionych warunków.

Zmiany nie są zapisywane między uruchomieniami. Zamknięcie i ponowne uruchomienie programu przywraca scenę demonstracyjną.

## 2. Mysz, klawisze i zaznaczanie

| Wejście | Działanie |
| --- | --- |
| LPM na wierzchołku | Zaznaczenie; przytrzymanie i ruch myszy przeciąga wierzchołek |
| LPM na kwadratowym uchwycie | Zaznaczenie i przeciąganie punktu kontrolnego Béziera |
| LPM blisko krawędzi | Zaznaczenie krawędzi; jej operacje są w menu PPM |
| LPM wewnątrz figury | Zaznaczenie i przeciąganie całego wielokąta |
| LPM na pustym tle | Usunięcie zaznaczenia |
| PPM na elemencie | Zaznaczenie elementu i menu dostępnych operacji |
| Shift+F10 | Menu dla aktualnego zaznaczenia, bez wskazywania nowego elementu |
| Esc podczas gestu | Odtworzenie całego dokumentu sprzed przeciągania |
| Esc podczas wyboru pary | Anulowanie wyboru drugiego odcinka |
| Esc bez aktywnej operacji | Usunięcie zaznaczenia |
| Delete | Usunięcie zaznaczonego wierzchołka, jeśli figura ma więcej niż trzy wierzchołki |
| F1 | Otwarcie lub przywołanie pomocy; anuluje aktywny gest i wybór pary |
| Przycisk Dokumentacja | Otwarcie lub przywołanie pełnej dokumentacji; również anuluje aktywny gest i wybór pary |

Jeśli pod kursorem znajduje się kilka elementów, program sprawdza je w kolejności: **uchwyt → wierzchołek → krawędź → wnętrze**. W punkt lub krawędź można trafić z odległości do 6 pikseli. Wierzchołki są oznaczone kółkami o promieniu 5 pikseli, a uchwyty kwadratami o boku 10 pikseli. Gdy wnętrza figur nakładają się, wybierana jest ostatnia z nich w kolejności dokumentu.

Miejsce złapania wierzchołka lub uchwytu jest zachowywane: kliknięcie blisko środka nie powoduje nagłego przeskoku. Po zaakceptowanym przeliczeniu punkt ma dokładnie pozycję wynikającą z kursora i tego przesunięcia.

Przeciąganie przechwytuje mysz także poza oknem. Utrata przechwycenia, utrata fokusu sceny na rzecz innego okna lub minimalizacja anuluje gest i przywraca dokument sprzed jego rozpoczęcia. Przejście fokusu do przycisków algorytmu nie anuluje operacji samo w sobie.

Pomoc można zostawić otwartą i wrócić do sceny. W jej oknie lista wybiera temat, Tab przechodzi między kontrolkami, tekst można zaznaczać i kopiować przez Ctrl+C, a Esc, Zamknij i krzyżyk zamykają pomoc. Ponowne F1 w scenie przywołuje istniejące okno bez zmiany tematu.

## 3. Operacje i ograniczenia

### Długość odcinka

1. Kliknij PPM odcinek i wybierz **Ustaw stałą długość...**.
2. Okno podpowiada aktualną długość krawędzi. Wpisz liczbę większą od zera, np. 120.
3. Zatwierdź OK albo anuluj bez zmian.

Akceptowane są kropka lub przecinek dziesiętny, np. `12.5` i `12,5`, a także zapis wykładniczy, np. `1e2`. Wartości niedodatnie, NaN, nieskończoności i częściowo poprawny tekst są odrzucane.

Podczas zmiany długości pierwszy koniec krawędzi pozostaje w miejscu, a pozostałe punkty mogą się dostosować. Przy istniejącej relacji menu udostępnia **Zmień stałą długość...** i **Usuń ograniczenie długości**. Napis `L=...` na scenie pokazuje zadaną wartość. Usunięcie relacji zachowuje aktualną geometrię.

### Równoległość

1. PPM na pierwszym odcinku → **Ustaw równoległość z...**.
2. Pierwszy odcinek staje się pomarańczowy, a scena pokazuje komunikat wyboru.
3. Kliknij LPM drugi odcinek. Może należeć do innego wielokąta.
4. Esc anuluje wybór. Powtórna para, ta sama krawędź lub niepoprawna relacja nie jest dodawana.

Pary są oznaczane `P1`, `P2` itd. Menu **Usuń równoległość** przy odcinku zawiera jego pary; wybranie jednej usuwa tylko ją. Numery są etykietami aktualnej kolejności relacji i mogą się zmienić po usunięciu. Odcinki mogą być równoległe także wtedy, gdy w kolejności obiegu figur są skierowane w przeciwne strony.

Jeden odcinek może mieć zadaną długość i wiele różnych par równoległości. Oba rodzaje relacji dotyczą wyłącznie prostych odcinków. Krzywe mają warunki ciągłości.

### Dodawanie i usuwanie wierzchołków

PPM na krawędzi → **Podziel krawędź** dodaje punkt w ustalonym miejscu, opisanym w tabeli. Miejsce kliknięcia służy jedynie do wskazania krawędzi. Oznaczenie Default przy nowym punkcie znaczy, że klasę ciągłości nadał program; Explicit oznacza wybór użytkownika.

| Typ krawędzi | Wynik podziału |
| --- | --- |
| Odcinek | Dwa odcinki przez środek; nowy wierzchołek ma G0/Default |
| Bézier | Dwie krzywe dla podziału parametru `t=0.5`; nowy wierzchołek ma G1/Default |
| Łuk | Dwa proste odcinki przez punkt łuku dla `t=0.5`; nowy wierzchołek ma G0/Default; kształt się zmienia |

Sam podział Béziera zachowuje jego kształt. Każda nowa krzywa otrzymuje jednak własny parametr od 0 do 1, więc zmieniają się długości jej pochodnych. Jeśli na zewnętrznych końcach obowiązuje C1, solver może przez to skorygować położenia punktów.

PPM na wierzchołku → **Usuń wierzchołek** lub Delete usuwa punkt i zastępuje dwie sąsiednie krawędzie jednym odcinkiem. Nie można zredukować figury poniżej trzech wierzchołków.

Relacje zastępowanych krawędzi są usuwane, także gdy wskazują krawędź innej figury. Ustawienia ciągłości ocalałych wierzchołków pozostają; pozostałe zależności są ponownie rozwiązywane. Operacje są zatwierdzane tylko przy poprawnym wyniku.

## 4. Krzywe i ciągłość

### Bézier

PPM na krawędzi → **Zamień na Béziera** tworzy krzywą trzeciego stopnia. Ma ona dwa końce współdzielone z wielokątem i dwa niezależne uchwyty. Przerywany wielobok kontrolny pomaga odczytać kierunki stycznych. Uchwyty przesuwa się LPM.

Po konwersji ustawiane jest domyślne G1 na końcach, jeśli użytkownik nie wybrał tam wcześniej klasy jawnie. Jawne ustawienie, także G0, jest zachowywane. Początkowe uchwyty leżą w 1/3 i 2/3 cięciwy, po czym solver może je dostosować do ciągłości.

PPM na wierzchołku przy Bézierze → **Ciągłość wierzchołka**:

| Klasa | Znaczenie |
| --- | --- |
| G0 | Wspólny punkt końcowy, bez zgodności stycznych |
| G1 | Zgodne kierunki i zwroty stycznych wzdłuż obiegu wielokąta |
| C1 | Równe pełne wektory pochodnych, łącznie z długością |

Pochodne odnoszą się do parametru każdego segmentu w `[0,1]`. Dlatego C1 pomiędzy odcinkiem a Bézierem wymaga długości sąsiedniego uchwytu równej 1/3 długości odcinka. Zerowa styczna jest niedopuszczalna dla G1; C1 może porównywać dwa zerowe wektory.

Przy ruchu uchwytu program najpierw próbuje zachować końce jego Béziera. Jeżeli nie uzyska rozwiązania, może dopuścić ich ruch. Inne punkty kontrolne i wierzchołki mogą się dostosować do wspólnych relacji.

### Łuk okręgu

PPM na krawędzi → **Zamień na łuk okręgu** zachowuje dotychczasowe ustawienia ciągłości. Dopuszcza G0 i G1, z G1 najwyżej na jednym końcu. C1 lub G1/G1 powoduje odrzucenie konwersji; należy wcześniej zmienić ustawienia końców w menu wierzchołka.

Przy G0 na obu końcach łuk jest półokręgiem. Jego promień to połowa odległości między końcami, czyli połowa długości cięciwy. Przy jednym G1 łuk jest analitycznie dopasowywany do stycznej sąsiada. Jeśli sąsiadujące łuki mają wspólne G1, łuk o niższym ID pozostaje referencją, a drugi jest dopasowywany. Gdy wymagana styczna nie wyznacza dopuszczalnego skończonego łuku, operacja jest odrzucana.

Nowy łuk ma dodatni parametr wygięcia. Zmiana strony nie jest osobnym poleceniem UI. Końce łuku muszą być różne.

### Konwersje

Menu krawędzi umożliwia przełączanie między odcinkiem, Bézierem i łukiem. Bieżący typ jest nieaktywny. Konwersja odcinka na krzywą usuwa jego zadaną długość i wszystkie pary równoległości. Powrót do odcinka nie przywraca dawnych relacji. Nieudana operacja zachowuje poprzedni typ, geometrię i relacje.

## 5. Założenia i sytuacje wyjątkowe

- Wielokąty mogą się samoprzecinać. Ich wnętrze określa reguła parzystości.
- Figury są przygotowane w kodzie sceny. Interfejs edytuje ich wierzchołki i krawędzie; nie tworzy ani nie usuwa całych wielokątów.
- Przy przesuwaniu całej figury wszystkie jej wierzchołki i uchwyty przesuwają się o ten sam wektor. Kształt, długości i kierunki krawędzi pozostają takie same. Dzięki temu zachowane są też ciągłość i równoległości, a inne figury pozostają na miejscu.
- Środek Béziera w operacji podziału oznacza `t=0.5`, a nie połowę długości krzywej. Podział łuku świadomie zastępuje go odcinkami, aby nowe łuki G0/G0 nie naruszały reguły półokręgu.
- Przy odrzuconym ruchu na scenie pozostaje **ostatni poprawny podgląd** i pojawia się ostrzeżenie. Przeciąganie może być kontynuowane. Puszczenie LPM zachowuje ostatni zaakceptowany podgląd, a Esc przywraca stan sprzed całego gestu.
- Brak zbieżności nie dowodzi matematycznej sprzeczności: przyczyną może być także degeneracja, ograniczony budżet iteracji lub trudności numeryczne. Program nie usuwa relacji, aby wymusić sukces.
- Domyślna skala wynosi 1 piksel na jednostkę świata; początek jest odsunięty o 20 pikseli od lewej i dolnej krawędzi. Oś świata Y jest skierowana w górę. Zmiana rozmiaru okna nie zmienia współrzędnych dokumentu.
- Przycinanie ogranicza rysowanie do widoku, ale skrajne wartości `double` mogą przepełnić obliczenia. Takie wejście lub kandydat jest odrzucany; program jest przeznaczony do niewielkiej sceny demonstracyjnej.
- Program nie implementuje zapisu plików, pełnej historii undo/redo, zoomu, przyciągania ani dodatkowej prostopadłości. Anulowanie pojedynczego gestu działa niezależnie od historii zmian.

## 6. Model danych i architektura

| Warstwa | Pliki i odpowiedzialność |
| --- | --- |
| Rdzeń | `cad_core`: geometria, dokument, walidacja, relacje i solver; bez nagłówków Windows |
| Edytor | `src/editor`: `EditorSession`, zaznaczenie, wykrywanie wskazanego elementu, przeciąganie i zatwierdzanie zmian; również bez WinAPI |
| Rasteryzacja | `src/render`: neutralne piksele Bresenhama, przycinanie i wybór algorytmu |
| Frontend | `src/platform/win32` i `Projecr1_GK.cpp`: komunikaty, okna, menu, dialogi, bufor DIB i rysowanie |

`Document` jest właścicielem trzech map: wierzchołków, krawędzi i wielokątów. Używa silnych typów `VertexId`, `EdgeId`, `PolygonId`, zawierających 64-bitową wartość. Zero oznacza niepoprawne ID. Encje są powiązane przez ID, a nie wskaźniki do elementów kontenera.

`Vertex` zawiera ID, pozycję `Vec2` z dwiema współrzędnymi `double` oraz ustawienie ciągłości. Ustawienie przechowuje klasę G0/G1/C1 i pochodzenie Default/Explicit.

`Edge` zawiera ID, ID obu końców oraz wariant geometrii:

| Wariant | Dodatkowe dane |
| --- | --- |
| `LineSegment` | Brak; końce wynikają z wierzchołków |
| `CubicBezier` | Dwie pozycje punktów kontrolnych |
| `CircularArc` | Niezerowy parametr `bulge` |

`Polygon` przechowuje uporządkowane listy ID wierzchołków i krawędzi. Krawędź `i` łączy wierzchołek `i` z `(i+1) mod n`. Sąsiednie segmenty używają tego samego wierzchołka, dlatego G0 jest spełnione konstrukcyjnie.

Długości i pary równoległości są przechowywane w wektorach dokumentu. Łączą krawędzie przez ID. Ciągłość jest metadanymi wierzchołka; aktywne połączenia są zbierane na podstawie topologii i typów sąsiednich krawędzi.

`EditorSession` udostępnia dokument do odczytu i kontroluje edycję. Gest jest wariantem przeciągania wierzchołka, uchwytu lub figury. Na czas przeciągania zapamiętuje kopię dokumentu sprzed ruchu, aby można było anulować gest. Zapisuje też miejsce złapania punktu lub początkową pozycję myszy przy przesuwaniu figury. Polecenia wymagające rozwiązania relacji pracują na kopii i zatwierdzają ją wyłącznie po sukcesie.

Walidator sprawdza spójność ID, połączeń i cykli, skończoność współrzędnych, geometrię i dozwolone klasy łuków. Samo `validate()` nie oznacza spełnienia numerycznych równań wszystkich relacji; za to odpowiada solver.

Inny frontend może używać `EditorSession` i `Document`, zastępując adapter wejścia oraz renderowanie. Typy `HWND`, `HDC`, pióra i pamięć DIB pozostają w warstwie WinAPI.

## 7. Algorytmy geometryczne i rysowanie

### Bézier: baza potęgowa i różnice progresywne

Dla punktów `P0, P1, P2, P3` używamy:

```text
B(t) = a*t³ + b*t² + c*t + d
a = P3 - P0 + 3*(P1 - P2)
b = 3*(P0 - 2*P1 + P2)
c = 3*(P1 - P0)
d = P0
```

Funkcja `bezierPoint` wyznacza pojedynczy punkt schematem Hornera i jest używana w testach. Podczas rysowania i sprawdzania, co wskazuje mysz, aplikacja korzysta z `sampleBezier`. Ta funkcja oblicza całą serię punktów przyrostowo: przygotowuje kilka różnic, a potem aktualizuje je przez dodawanie. Dla `h=1/N` wartości początkowe wynoszą:

```text
point = P0
Δ1 = a*h³ + b*h² + c*h
Δ2 = 6*a*h³ + 2*b*h²
Δ3 = 6*a*h³
```

W każdym kroku wykonywane są dodawania: `point += Δ1`, `Δ1 += Δ2`, `Δ2 += Δ3`. Wynik ma `N+1` punktów; końce są ustawione dokładnie na `P0` i `P3`. Wspólne próbkowanie krawędzi używa domyślnie 100 odcinków na krzywą.

Podział Béziera dla `t=0.5` używa de Casteljau: kolejnych środków odcinków wieloboku kontrolnego. Wynikiem są dwa zestawy czterech punktów, mające wspólny punkt podziału.

### Łuk

Łuk wyznaczają końce i `bulge = tan(sweep/4)`. Znak wygięcia określa zwrot przebiegu w układzie świata. Z tych danych obliczane są środek, promień, kąt początkowy i kąt przebiegu. Punkty wynikają z parametryzacji okręgu; końce próbkowania są zachowane dokładnie.

`updateArcBulges` ustawia `±1` dla G0/G0 albo analitycznie dopasowuje wygięcie do stycznej przy G1. Wszystkie wygięcia są zatwierdzane razem po sprawdzeniu dopuszczalności. `bulge` nie jest osobną zmienną numerycznego solvera.

### Trafianie i wnętrze

Odległość punktu od odcinka jest obliczana przez rzut:

```text
t = clamp(dot(P-A, B-A) / |B-A|², 0, 1)
Q = A + t*(B-A)
distanceSquared = |P-Q|²
```

Odcinek o zbieżnych końcach jest traktowany jak punkt. Dla krzywej mierzone są odległości do kolejnych odcinków jej próbkowanej łamanej. Wybierana jest najbliższa krawędź w tolerancji.

Aby sprawdzić wnętrze figury, prowadzimy od wskazanego punktu poziomy promień i liczymy jego przecięcia z konturem. Nieparzysta liczba oznacza wnętrze, parzysta — zewnętrze. Reguła działa także dla samoprzecięć. Rysowanie i wykrywanie trafień używają wspólnego `sampleEdge`, więc sprawdzany kontur odpowiada łamanej widocznej na ekranie.

### GDI, Bresenham i bufor

Natywne radio buttony **GDI / Bresenham** wybierają algorytm dla wszystkich odcinków geometrii, także fragmentów próbkowanych krzywych. GDI używa `MoveToEx` i `LineTo`. Własny Bresenham obsługuje wszystkie oktanty i odcinek zerowy; dopisuje oba końce do roboczego wektora pikseli.

Bresenham najpierw wyznacza ślad o grubości jednego piksela. Zaczyna w pierwszym końcu odcinka i utrzymuje całkowitoliczbowy błąd, który pomaga wybrać kolejny piksel. W każdym kroku na podstawie podwojonego błędu decyduje, czy przesunąć X, Y, czy obie współrzędne. Znaki przyrostów określają kierunek ruchu, dlatego jeden kod obsługuje linie we wszystkich ośmiu oktantach. Pętla kończy się po zapisaniu drugiego końca. Grubość linii jest dodawana później przez pędzel opisany poniżej.

Przed konwersją na `int` współrzędne ekranowe `double` są przycinane algorytmem Cohena–Sutherlanda. Kody bitowe klasyfikują końce względem granic, wspólny bit pozwala odrzucić odcinek, a pozostałe przypadki są przycinane przez przecięcia z granicami.

W trybie własnym piksele są zapisywane bezpośrednio do odgórnego 32-bitowego DIB, pod indeksem `y*width+x`, w formacie `0x00RRGGBB`. Program nie wywołuje `SetPixel` dla każdego punktu. Roboczy wektor zachowuje pojemność między odcinkami w klatce. Kwadratowy pędzel ma szerokość 2 pikseli dla zwykłej krawędzi i 4 dla wyróżnienia; jego zapis jest ograniczony do obszaru sceny. Na przekątnych ten pędzel daje inny ślad niż pióro GDI.

Klatka powstaje w `BackBuffer`: tło, wszystkie krawędzie, następnie dekoracje i pasek. Jedno `GdiFlush` przed partią zapisów CPU kończy wcześniejsze rysowanie GDI. Uchwyty, wierzchołki, etykiety i linie pomocnicze są później rysowane przez GDI. `BitBlt` prezentuje bufor. Przy niedostępnym buforze pozostaje bezpośrednie rysowanie GDI.

`WM_MOUSEMOVE` zapamiętuje najnowszą próbkę zamiast rozwiązywać wszystkie komunikaty osobno. Timer o zadanym interwale 1 ms przelicza podgląd; rzeczywisty termin zależy od kolejki i systemu Windows. Puszczenie LPM natychmiast przelicza końcową pozycję. Awaria utworzenia timera przywraca bezpośrednią obsługę ruchu.

## 8. Mechanizm ograniczeń i solver

### Równania

Solver oblicza, o ile każdy warunek jest niespełniony. Tę liczbę nazywamy błędem lub resztą. Jeśli odcinek ma długość 10, a powinien mieć 12, jego błąd wynosi `10 - 12 = -2`. Dla poprawnego odcinka jest równy zeru.

| Relacja | Jak obliczamy błąd |
| --- | --- |
| Długość | `length(B-A) - L` |
| Równoległość | `cross(unit(d1), unit(d2))`; dopuszcza przeciwne zwroty |
| G1 | `atan2(cross(unit(Tin), unit(Tout)), dot(unit(Tin), unit(Tout)))` |
| C1 | Dwie składowe `Tout - Tin` |

W tabeli `length` oznacza długość wektora, `unit` jego kierunek jednostkowy, a `cross` i `dot` odpowiednio iloczyn wektorowy 2D i skalarny. `Tin` i `Tout` to pochodne segmentu dochodzącego do wierzchołka i segmentu z niego wychodzącego.

Pochodna odcinka to `B-A`. Dla Béziera na początku wynosi `3*(P1-P0)`, a na końcu `3*(P3-P2)`. Dla łuku pochodzi z parametryzacji okręgu i uwzględnia kąt przebiegu. G1 porównuje również zwrot; sama współliniowość nie wystarcza.

### Przebieg rozwiązania

Program zmienia położenia punktów małymi krokami, tak aby zmniejszać łączny błąd. Współrzędne przeciąganego punktu są przy tym zablokowane w pozycji wyznaczonej przez kursor. Tłumienie ogranicza rozmiar poprawek, a sprawdzanie kolejnych prób pozwala odrzucić krok, który pogarsza wynik.

W aplikacji tę pracę wykonuje `solveConstraints`, korzystając z `solvePointConstraintSystem`. Starsza ścieżka z `solveWithJacobian` pozostała na potrzeby testów. Poniższy opis dotyczy solvera używanego podczas edycji.

1. Utworzenie kandydata dokumentu, dopasowanie wygięć łuków, walidacja wejścia i zebranie aktywnych połączeń.
2. Przypięcie prowadzonego wierzchołka lub uchwytu dokładnie w żądanej pozycji. Jego współrzędne są wyłączone z niewiadomych.
3. Zebranie pozostałych wierzchołków i uchwytów uczestniczących w relacjach. Każdy punkt daje dwie kolumny: X i Y. Relacje mogą obejmować różne figury.
4. Obliczenie wektora reszt i numerycznego Jacobiego. Długości i C1 są normalizowane skalą geometrii; kąty i równoległość pozostają bezwymiarowe.
5. Wyznaczenie tłumionej korekty przez gęsty układ liniowy z wyborem elementu głównego:

   ```text
   (J*Jᵀ + λ*I) * y = -Fscaled
   deltaNormalized = Jᵀ * y
   deltaWorld = geometryScale * deltaNormalized
   ```

   `J` to macierz Jacobiego, `Fscaled` to wektor błędów po przeskalowaniu, `λ` określa tłumienie, a `I` jest macierzą jednostkową. Wektor `y` jest pomocniczym wynikiem rozwiązania układu. Ostateczna poprawka `deltaWorld` podaje, o ile przesunąć poszczególne współrzędne.

6. Próby kroku z ponownym dopasowaniem łuków, zmniejszaniem kroku i zmianą tłumienia. Akceptowany jest spadek normy błędu lub spełnienie wszystkich tolerancji. Przy nieudanym kroku w sytuacji bliskiej prostopadłości możliwy jest próbny ruch pojedynczej wolnej współrzędnej, również wymagający spadku błędu.
7. Ponowne sprawdzenie relacji, skończoności, geometrii i dokładnego przypięcia punktu. Dopiero sukces zatwierdza dokument. Niepowodzenie zachowuje stan wejściowy solvera.

Nie ma gwarancji znalezienia rozwiązania globalnego ani jedynego, najbliższego układu. Tłumienie pozwala obsługiwać układy niedookreślone i nadmiarowe, ale nie zastępuje końcowej kontroli reszt.

### Macierz Jacobiego i skala

Macierz Jacobiego opisuje, jak niewielkie przesunięcie każdej współrzędnej wpływa na błędy warunków. Kolumna odpowiada współrzędnej X lub Y konkretnego punktu, a wiersz pojedynczemu błędowi. Warunek C1 daje dwa wiersze, bo porównuje obie składowe pochodnej.

Do pomiaru pochodnej program przesuwa na chwilę jedną współrzędną, ponownie liczy błąd i dzieli jego zmianę przez wielkość przesunięcia. Jest to metoda różnic skończonych. Po pomiarze przywraca poprzedni stan. Krok zależy od precyzji `double` i skali współrzędnych; używana jest faktycznie reprezentowana zmiana, a w razie potrzeby `nextafter`. Różnice kątów uwzględniają przejście przez `±π`.

Grupy reszt zapamiętują zależności od punktów. Dla kolumny przeliczane są tylko zależne reszty, z kontrolą aktualności wektora bazowego. Każda próbka odtwarza zmienione pozycje i wygięcia. Łuki są dopasowywane przed oceną próbki. Macierz korekty jest gęsta; nie jest to solver dla scen z tysiącami obiektów.

Skala obejmuje m.in. długości cięciw, uchwyty, zadane długości i strzałkę łuku. Użycie strzałki zamiast samego promienia zapobiega sztucznemu zawyżaniu skali przy prawie prostym łuku.

### Parametry i wynik

Aktualne wartości domyślne z `SolveOptions`:

| Parametr | Wartość i znaczenie |
| --- | --- |
| `maxIterations` | Maksymalnie 10 000 zaakceptowanych poprawek położeń punktów |
| `maxFixedEndpointIterations` | Maksymalnie 50 poprawek z nieruchomymi końcami Béziera; wliczają się do łącznego limitu 10 000 |
| `lengthTolerance` | `1e-6` w jednostkach świata |
| `parallelTolerance` | `1e-6` dla bezwzględnego znormalizowanego iloczynu wektorowego |
| `g1Tolerance` | `1e-6` radiana |
| `c1Tolerance` | `1e-6` dla każdej składowej różnicy pochodnych |

Jeśli próba z nieruchomymi końcami Béziera nie znajdzie rozwiązania, program wraca do tego samego stanu wejściowego i próbuje ponownie, tym razem pozwalając końcom się przesuwać. Korzysta z pozostałej części wspólnego limitu. W obu próbach uchwyt pozostaje przypięty do pozycji wyznaczonej przez mysz.

Wygięcie łuku jest obliczane bez iteracyjnego przesuwania punktów, więc nie zużywa tego limitu. Limit iteracji określa liczbę poprawek, a nie czas obliczeń.

`SolveResult` zawiera status i liczbę zaakceptowanych korekt. Statusy: `Converged`, `InvalidInput`, `NoConvergence`, `DegenerateGeometry`. Walidator dokumentu osobno zwraca listę problemów strukturalnych. Aplikacja pokazuje użytkownikowi komunikat o nieuzyskaniu spójnego stanu i zachowuje poprawny dokument.

## 9. Budowanie, testy i przekazanie programu

Gotowy program znajduje się w [dist/windows-x64/Projecr1_GK.exe](../dist/windows-x64/Projecr1_GK.exe). Na 64-bitowym Windows 10/11 wystarczy pobrać ten plik i uruchomić go dwukrotnym kliknięciem. Nie trzeba kompilować projektu ani instalować narzędzi programistycznych.

Do budowania ze źródeł potrzebne są Visual Studio z obsługą aplikacji klasycznych C++, toolset **v145**, Windows SDK 10 i MSBuild obsługujący `.slnx`. Projekt używa C++20. Konfiguracje obejmują Debug/Release oraz x64/Win32.

W **Developer PowerShell dla Visual Studio**, w katalogu projektu:

```powershell
msbuild .\Projecr1_GK.slnx /m /p:Configuration=Release /p:Platform=x64
.\x64\Release\Projecr1_GK.exe
```

Debug buduje się analogicznie z `/p:Configuration=Debug`. Do prezentacji wydajności należy użyć Release. Zależności projektu automatycznie budują `cad_core`.

Testy można zbudować i uruchomić razem:

```powershell
.\tests\run-tests.cmd -Configuration Debug
.\tests\run-tests.cmd -Configuration Release
```

Zestaw przenośny sprawdza model, geometrię, relacje, Jacobian, solver, sesję, hit testy i rasteryzację. Zestaw WinAPI sprawdza rzeczywistą procedurę okna, dialog pomocy, zasoby, przyciski, DIB i prezentację. Okna testów są ukryte, a modalne ostrzeżenia przechwytywane. Szczegółowy zakres i bieżąca liczba przypadków: [tests/README.md](../tests/README.md).

Pomoc, pełna dokumentacja, ikony i scena początkowa są wkompilowane do EXE. W konfiguracji Release biblioteki uruchomieniowe C++ są dołączone statycznie (`/MT`), więc do uruchomienia wystarczy sam EXE i biblioteki systemowe Windows. Nie jest potrzebny osobny pakiet Microsoft Visual C++ Redistributable. Zmiany sceny nie są zapisywane między uruchomieniami.

Po zmianach źródeł lub dokumentacji odśwież wersję do pobrania:

```powershell
.\scripts\build-release.cmd
```

Skrypt odnajduje MSBuild, buduje aplikację Release x64 i kopiuje ją do `dist/windows-x64/`. Aktualizuje też `SHA256SUMS.txt`, którym można sprawdzić zgodność pobranego pliku. Skrypt wymaga narzędzi do kompilacji i nie wykonuje commitów. Aktualny EXE oraz sumę kontrolną dołączaj do commita razem ze zmianami źródeł.

Przekazując cały projekt, dołącz również źródła, projekty Visual Studio, ikony, zasób `.rc`, ten dokument i opis testów.

Wcześniejsze plany rozbudowy znajdują się w [dokumentach architektury](architecture/README.md). Opis bieżącego zachowania i granic implementacji stanowi niniejszy dokument.

## 10. Scenariusz prezentacji

1. Uruchom Release: pokaż dwie figury, `L=180`, `L=120`, `L=160` oraz pary `P1` i `P2`.
2. Przeciągnij oba uchwyty lewego Béziera. W menu jego końców pokaż C1 i G1. Anuluj gest przez Esc, aby wrócić do punktu wyjścia.
3. Przeciągnij wierzchołek prawego łuku i pokaż zachowanie styczności. Sprawdź, że menu przy łuku wyłącza C1.
4. Przesuń całą figurę za wnętrze; długości, wygięcia i równoległości pozostają zachowane.
5. Zmień długość odcinka, usuń jedną równoległość i dodaj ją ponownie przez wybór dwóch odcinków.
6. Przełącz GDI/Bresenham. Uchwyty i geometria pozostają edytowalne w obu trybach.
7. Podziel Béziera i łuk; porównaj zachowanie kształtu. Usuń dodany wierzchołek i pokaż aktualizację relacji.
8. Wywołaj F1, zmień temat, powiększ pomoc, wróć do sceny i zamknij pomoc przez Esc.

Operacje demonstracyjne mogą zmieniać późniejszą geometrię i relacje. Gdy potrzebna jest identyczna scena początkowa, uruchom program ponownie.
