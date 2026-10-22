# Ograniczenia i solver

[Spis treści](README.md)

## Jeden układ zależności

Solver obejmuje długości i równoległość **wyłącznie odcinków**, a osobno równania ciągłości Bézierów oraz warunki dotyczące łuków. Krzywe i półokręgi nie otrzymują ograniczeń długości ani równoległości. Dodanie takiej relacji do krzywej jest odrzucane przez walidację.

Wspólne rozwiązywanie nadal ma znaczenie: krzywa i sąsiedni odcinek współdzielą wierzchołek. Ciągłość może więc pośrednio wpływać na odcinek mający ograniczenie długości lub równoległości. Rozdzielenie tych operacji na niezależne procedury mogłoby prowadzić do sytuacji, w której poprawa styczności psuje długość odcinka, a poprawa długości ponownie psuje styczność.

Zmienne to współrzędne wierzchołków, współrzędne punktów kontrolnych Béziera i parametr `bulge` łuków. Parametry takie jak zadana długość są stałymi. Końce krzywych nie mają osobnych współrzędnych poza wierzchołkami.

Graf zależności łączy zmienne uczestniczące we wspólnej relacji. Podczas edycji rozwiązujemy tylko dotkniętą składową, z uwzględnieniem relacji między wielokątami. Nie zakładamy, że cała składowa mieści się w edytowanym wielokącie.

## Równania

Dla odcinka przyjmujemy `d = end - start`. Pierwsze dwa równania dotyczą wyłącznie krawędzi typu `LineSegment`; pozostałe opisują ciągłość i geometrię krzywych:

- Zadana długość: `length(d) - L = 0`, z wymaganiem `L > epsilon`.
- Równoległość: `cross(unit(d1), unit(d2)) = 0`. Kierunki przeciwne są dozwolone.
- G1: zgodność kierunków stycznych wzdłuż obiegu wielokąta; zerowy iloczyn wektorowy oraz dodatni iloczyn skalarny. Sama współliniowość dopuszczałaby zawrócenie krzywej.
- C1: równość dwóch składowych wektorów pochodnych po obu stronach wierzchołka.
- Łuk G0/G0: `bulge = +1` albo `bulge = -1`, zgodnie z zapisaną stroną łuku.

G0 wynika ze współdzielenia wierzchołka. Regułę najwyżej jednego końca G1 łuku sprawdza walidator przed uruchomieniem solvera. Dodatni zwrot stycznych i brak degeneracji są warunkami dopuszczalności kandydata, a nie wyłącznie równaniami do minimalizacji.

Reszty odległościowe skalujemy charakterystycznym rozmiarem rozwiązywanej geometrii. Reszty kierunkowe są bezwymiarowe. Wynik weryfikujemy również w jednostkach świata, oddzielnie dla błędu pozycji, długości i kąta. Tolerancje modelu nie zależą od powiększenia widoku.

## Proponowany algorytm startowy

Dla rozmiaru sceny z zadania proponujemy iteracyjny solver numeryczny z tłumieniem, oparty na linearyzacji równań. To prototyp wymagający sprawdzenia na scenach testowych, a nie deklaracja niezawodności dowolnego układu CAD.

1. Zbuduj zmienne i równania dotkniętej składowej.
2. Wstaw dokładne położenie elementu prowadzonego przez użytkownika i usuń jego współrzędne ze zbioru niewiadomych.
3. Rozpocznij od ostatniego poprawnego podglądu. Przy przeciąganiu przenieś też powiązane uchwyty o przyrost ruchu wierzchołka, aby uzyskać rozsądny punkt startowy.
4. Oblicz wektor reszt `F` i jakobian `J`. W pierwszej wersji wystarczą różnice skończone z krokiem dopasowanym do skali zmiennej.
5. Wyznacz tłumioną korektę minimalnej normy, np. przez rozwiązanie `(J*J^T + lambda*I)y = -F`, a następnie `delta = J^T*y`. Zmienne i równania muszą być wcześniej przeskalowane.
6. Dobierz krok i tłumienie tak, aby zmniejszyć błąd i zachować dopuszczalną geometrię. W razie odrzuconego kroku zmniejsz go lub zwiększ tłumienie.
7. Zakończ po spełnieniu tolerancji albo osiągnięciu limitu iteracji. Sprawdź ponownie wszystkie relacje, skończoność współrzędnych i warunki geometryczne.

Na początku wystarczy niewielki moduł macierzy gęstych i rozwiązania układu liniowego z wyborem elementu głównego. Trzymamy go oddzielnie od geometrii i weryfikujemy na znanych układach. Gdy prototyp pokaże problemy kondycjonowania lub skali, można podmienić ten moduł na implementację QR/SVD bez zmiany dokumentu i API edytora.

Tłumienie pomaga przy nadmiarowych równaniach, ale nie zastępuje weryfikacji wyniku. Minimalna korekta w pojedynczej iteracji ogranicza niepotrzebne ruchy; nie gwarantuje globalnie najbliższego rozwiązania. Nie dodajemy miękkiej kary za ruch do kryterium akceptacji kosztem niespełnionych relacji.

Wynik: `Converged`, `InvalidInput`, `NoConvergence` lub `DegenerateGeometry`, wraz z resztami i identyfikatorami niespełnionych relacji. Brak zbieżności nie jest dowodem matematycznej sprzeczności układu.

## Priorytet ruchu użytkownika

- Przesuwany wierzchołek lub uchwyt ma pozycję dokładnie wyznaczoną przez kursor w układzie świata. Solver nie może przyciągać go do innego miejsca.
- Przy ruchu uchwytu najpierw blokujemy oba wierzchołki jego segmentu. Jeżeli relacje uniemożliwiają rozwiązanie, można ponowić próbę z odblokowaniem tych końców w dotkniętej składowej. To wyjątek wynikający z relacji, a nie standardowy efekt przesuwania uchwytu.
- Przesuwanie całego wielokąta narzuca tę samą translację wszystkim jego wierzchołkom i uchwytom oraz zachowuje parametry `bulge` jego łuków. Powiązane inne wielokąty mogą się dostosować. Gdy to niemożliwe, ruch nie zostaje zatwierdzony.
- Relacji nie usuwamy automatycznie w celu ułatwienia przeciągania. Osobną regułą jest konwersja odcinka na krzywą: usuwa jego ograniczenia długości i równoległości w ramach transakcji.

Nie dla każdego kursora istnieje rozwiązanie: np. trójkąt o trzech ustalonych długościach nie pozwala umieścić jednego wierzchołka w dowolnym miejscu przy dowolnych dodatkowych blokadach. W takim przypadku pokazujemy niepoprawny podgląd z prowadzonym punktem pod kursorem i oznaczeniem konfliktu; dokument zachowuje ostatni poprawny stan. Zwolnienie przycisku przywraca ten stan. To jawna polityka dla sytuacji, w której nie da się jednocześnie zachować wszystkich warunków zadania.

## Transakcje

Dodanie relacji, zmiana ciągłości, konwersja krawędzi i zmiana topologii działają na kandydacie dokumentu. Zatwierdzamy go dopiero po walidacji i rozwiązaniu zależności. Nieudana operacja nie zostawia połowy zmian ani osieroconych ID.

Konwersja odcinka na Béziera lub łuk najpierw usuwa z kandydata wszystkie relacje długości i równoległości wskazujące tę krawędź. Dotyczy to całej relacji równoległości, także z odcinkiem innego wielokąta. Pozostałe relacje innych odcinków nadal obowiązują. Dopiero potem ustalamy ciągłość i rozwiązujemy zmieniony układ. Nieudana konwersja przywraca geometrię oraz usunięte relacje; powrót krzywej do odcinka nie odtwarza ich automatycznie.

Gest przeciągania przechowuje stan początkowy, ostatni poprawny podgląd oraz bieżący kandydat. `Esc` przywraca stan początkowy. Gest ma stały punkt odniesienia dla translacji, dzięki czemu nie sumujemy błędów kolejnych przyrostów myszy.
