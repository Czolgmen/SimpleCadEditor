# Ograniczenia i solver

[Spis treści](README.md)

Bieżący `solveConstraints` wspólnie rozwiązuje długości i równoległość: obejmuje walidację, oddzielne tolerancje, zgodne skalowanie błędów i zmiennych, tłumienie, dobór kroku, algebrę liniową i numeryczne [obliczanie Jacobiego](09-jacobian.md). `solveFixedLengths` wywołuje tę samą pętlę z pustą listą równoległości. Poniższy opis obejmuje także docelową ciągłość i graf zależności. Prototyp przyjmuje skończone długości docelowe większe od zera, przekazuje relacje przez `span` i rozwiązuje wszystkie przekazane relacje. `Document` przechowuje pary równoległości i usuwa relacje zmienionych krawędzi przy zmianie topologii. Aplikacja dodaje parę przez wybór pierwszej krawędzi w menu PPM i kliknięcie drugiej, zatwierdzając ją po sukcesie wspólnego solvera. Wybraną parę można usunąć z podmenu krawędzi bez zmiany geometrii i pozostałych relacji. Graf dotkniętej składowej i ciągłość pozostają kolejnymi etapami.

`Document::addParallel` waliduje parę przez `parallelError` i zapisuje ją bez uruchamiania solvera. Niezerowy błąd jest poprawnym wejściem do magazynu. Para jest nieuporządkowana: `(A, B)` i `(B, A)` są duplikatami, których dodanie zwraca `false`. Jedna krawędź może wystąpić w wielu różnych parach. `removeParallel` wyszukuje obie kolejności i usuwa wyłącznie wskazaną parę; brak pary zwraca `false`. Usuwanie nie wymaga poprawnej geometrii. Odczyt przez `getParallelConstraints` zwraca niemodyfikowalny span, który należy pobrać ponownie po zmianie magazynu lub podmianie dokumentu.

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

W obecnym prototypie długości dzielimy przez wspólną skalę geometrii, a błędy równoległości pozostają bezwymiarowe. Tolerancja długości jest w jednostkach świata; `parallelTolerance` dotyczy bezwzględnej wartości znormalizowanego iloczynu wektorowego, czyli sinusa kąta. Przy nieudanej korekcie blisko prostopadłości solver może wykonać mały próbny ruch pojedynczej wolnej współrzędnej w obu kierunkach, ze zmniejszaniem kroku. Sinus ma w tym miejscu stacjonarne maksimum, więc sam Jacobian może dać zerowy krok. Próbę przyjmujemy wyłącznie przy spadku łącznej normy, zachowaniu skończonej geometrii i niezerowych krawędzi. Ruch liczy się do limitu iteracji, nie obejmuje prowadzonego punktu i nie zatwierdza dokumentu przed spełnieniem wszystkich relacji.

Zerowy kierunek odcinka uczestniczącego w równoległości zwraca `DegenerateGeometry`; nieistniejące odwołania, relacja krawędzi z samą sobą, błędne parametry i przepełnienie wejściowych kierunków zwracają `InvalidInput`. Każdy sukces jest sprawdzany ponownie przed podmianą dokumentu. Niepowodzenie zachowuje pozycje, topologię i przechowywane relacje.

Wynik: `Converged`, `InvalidInput`, `NoConvergence` lub `DegenerateGeometry`, wraz z resztami i identyfikatorami niespełnionych relacji. Brak zbieżności nie jest dowodem matematycznej sprzeczności układu.

## Priorytet ruchu użytkownika

- Przesuwany wierzchołek lub uchwyt ma pozycję dokładnie wyznaczoną przez kursor w układzie świata. Solver nie może przyciągać go do innego miejsca.
- Przy ruchu uchwytu publiczne przeciążenie `solveConstraints(ControlPointRef)` najpierw blokuje oba wierzchołki jego segmentu. Prywatna pętla usuwa te wierzchołki z niewiadomych i sprawdza przed zatwierdzeniem, że ich pozycje są dokładnie zachowane. Ta próba otrzymuje najwyżej `min(maxIterations, maxFixedEndpointIterations)` iteracji; domyślnie limit preferencji wynosi 8, a całkowity limit pozostaje równy 100. Wartość 0 pomija korekty z zablokowanymi końcami. Jeśli próba nie uzyska zbieżności i pozostaje budżet iteracji, ponawiamy obliczenia od tego samego dokumentu z odblokowanymi końcami. Obie próby dzielą `maxIterations`, a wynik sumuje zaakceptowane poprawki obu prób. Uchwyt pozostaje dokładnie przypięty w obu wariantach. Niepoprawne wejście jest odrzucane bez drugiej próby; nieudane obliczenia nie zmieniają dokumentu. Ruch końców jest więc wariantem rezerwowym. Oddzielny limit preferencji zapobiega zużyciu wszystkich iteracji na trudny problem z unieruchomionymi końcami. Jest limitem liczby poprawek, nie czasu obliczeń.
- Przesuwanie całego wielokąta narzuca tę samą translację wszystkim jego wierzchołkom i uchwytom oraz zachowuje parametry `bulge` jego łuków. Powiązane inne wielokąty mogą się dostosować. Gdy to niemożliwe, ruch nie zostaje zatwierdzony.
- Relacji nie usuwamy automatycznie w celu ułatwienia przeciągania. Osobną regułą jest konwersja odcinka na krzywą: usuwa jego ograniczenia długości i równoległości w ramach transakcji.

Nie dla każdego kursora istnieje rozwiązanie: np. trójkąt o trzech ustalonych długościach nie pozwala umieścić jednego wierzchołka w dowolnym miejscu przy dowolnych dodatkowych blokadach. W takim przypadku pokazujemy niepoprawny podgląd z prowadzonym punktem pod kursorem i oznaczeniem konfliktu; dokument zachowuje ostatni poprawny stan. Zwolnienie przycisku przywraca ten stan. To jawna polityka dla sytuacji, w której nie da się jednocześnie zachować wszystkich warunków zadania.

## Transakcje

Dodanie relacji, zmiana ciągłości, konwersja krawędzi i zmiana topologii działają na kandydacie dokumentu. Zatwierdzamy go dopiero po walidacji i rozwiązaniu zależności. Nieudana operacja nie zostawia połowy zmian ani osieroconych ID.

Konwersja odcinka na Béziera lub łuk najpierw usuwa z kandydata wszystkie relacje długości i równoległości wskazujące tę krawędź. Dotyczy to całej relacji równoległości, także z odcinkiem innego wielokąta. Pozostałe relacje innych odcinków nadal obowiązują. Dopiero potem ustalamy ciągłość i rozwiązujemy zmieniony układ. Nieudana konwersja przywraca geometrię oraz usunięte relacje; powrót krzywej do odcinka nie odtwarza ich automatycznie.

Gest przeciągania przechowuje stan początkowy, ostatni poprawny podgląd oraz bieżący kandydat. `Esc` przywraca stan początkowy. Gest ma stały punkt odniesienia dla translacji, dzięki czemu nie sumujemy błędów kolejnych przyrostów myszy.
