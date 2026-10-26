# cad_core

Biblioteka statyczna C++20, niezależna od WinAPI i GDI. Zawiera operacje `Vec2`, model dokumentu z odcinkami, walidator oraz początkową scenę demonstracyjną, w przestrzeni nazw `cad`.

## Struktura

```text
cad_core/
  include/cad_core/
    demo/
      DemoScene.hpp
    geometry/
      Vec2.h
    model/
      Id.h
      Vertex.h
      Edge.h
      Polygon.h
      Document.h
      Validation.h
  src/
    demo/
      DemoScene.cpp
    geometry/
      Vec2.cpp
    model/
      Document.cpp
  cad_core.vcxproj
  cad_core.vcxproj.filters
```

Nagłówki publiczne są w `include/cad_core`, a implementacje w `src`. Plik `Vec2.cpp` został przeniesiony z katalogu głównego biblioteki do `src/geometry`. Filtry Visual Studio odpowiadają grupom plików na dysku.

## Budowanie i użycie

Otwórz [rozwiązanie w katalogu repozytorium](../Projecr1_GK.slnx). Zawiera projekty aplikacji oraz biblioteki. `cad_core` ma ustawione C++20, wyłączone nagłówki prekompilowane i konfiguracje Debug/Release dla Win32/x64.

Aplikacja ma referencję do projektu biblioteki i ścieżkę do jej publicznych nagłówków. Przykład dołączania:

```cpp
#include <cad_core/geometry/Vec2.h>
```

Biblioteka nie dołącza `framework.h` ani nagłówków Windows. Zależność biegnie od aplikacji do rdzenia.

## IntelliSense w VS Code

Otwórz folder repozytorium zawierający `.vscode` oraz `cad_core`. [Konfiguracja C/C++](../.vscode/c_cpp_properties.json) wskazuje C++20, katalog `cad_core/include`, kompilator MSVC i Windows SDK dostępne na bieżącej maszynie. Na innym komputerze dostosuj `compilerPath` i `windowsSdkVersion` do zainstalowanych narzędzi.

Ścieżkę w `#include` zapisuj względem `cad_core/include`, np. `<cad_core/model/Id.h>`. Nie dopisuj do niej ponownie fragmentu `cad_core/include`. Konfiguracja `.vcxproj` służy budowaniu w Visual Studio/MSBuild, a plik `.vscode/c_cpp_properties.json` ustawia analizę kodu w rozszerzeniu C/C++ dla VS Code.

## Kolejność implementacji

Metody wyszukiwania, tworzenie wielokątów, walidator i operacje `Vec2` mają [testy](../tests/README.md), uruchamiane z katalogu repozytorium poleceniem `.\tests\run-tests.cmd`.

1. `Vec2.h` i `Vec2.cpp`: współrzędne oraz operacje na wektorach.
2. `Id.h`: osobne typy identyfikatorów wierzchołków, krawędzi i wielokątów.
3. `Vertex.h`, `Edge.h`, `Polygon.h`: dane modelu, na początek wyłącznie odcinki.
4. `Document.h` i `Document.cpp`: własność danych i API dokumentu.

Podział odpowiedzialności opisuje [plan architektury](../docs/architecture/README.md).

## Scena demonstracyjna

`createDemoScene()`, zadeklarowane w `<cad_core/demo/DemoScene.hpp>`, tworzy dwie rozdzielone figury o pięciu wierzchołkach. Lewa ma górnego Béziera z jawnym C1 przy pionowym boku i domyślnym G1 przy przekątnej. Prawa zawiera łuk G1 styczny do pionowego boku oraz półokrąg G0/G0. Długości 180 i 120 są przypisane dolnemu i pionowemu odcinkowi lewej figury, a 160 dolnemu odcinkowi prawej. Dwie pary równoległości łączą dolne oraz pionowe odcinki obu figur. Wierzchołki, krzywe i uchwyty mieszczą się w obszarze klienta 640×420 z paskiem algorytmu. Dodanie kolejnego wielokąta sprowadza się do przygotowania współrzędnych i wywołania `addPolygon` w `DemoScene.cpp`. Zwrócone ID służą do dalszej konfiguracji. Przed zwróceniem dokumentu funkcja uruchamia wspólny solver i `validate()`. Nieudana konfiguracja, brak zbieżności lub błędny dokument zgłasza `std::logic_error`. Początkowa geometria spełnia długości i ciągłość Béziera od razu; solver dopasowuje wygięcie łuku G1 analitycznie, bez korekt współrzędnych.

`Document::getPolygons() const` udostępnia istniejącą mapę wielokątów jako referencję tylko do odczytu. Renderer może iterować po niej i odczytywać powiązane krawędzie oraz wierzchołki przez `findEdge` i `findVertex`. Każde wywołanie `createDemoScene()` zwraca niezależny dokument.

## Walidacja dokumentu

`Document::validate() const` zwraca listę `ValidationIssue`. Pusta lista oznacza, że zapisane wielokąty spełniają aktualne kontrole. Dokument bez wielokątów jest poprawną pustą sceną.

Walidator sprawdza minimum trzech wierzchołków, zgodność liczby krawędzi, istnienie wskazanych encji, skończone pozycje i połączenie krawędzi w zamknięty cykl. Sprawdza także istnienie obu końców krawędzi. Samoprzecięcia są dozwolone. Przy brakujących encjach lub pustych listach zgłasza błędy i bezpiecznie kontynuuje pozostałe kontrole.

Każdy problem zawiera kod oraz związane ID wielokąta, wierzchołka lub krawędzi. Niewykorzystane pola ID pozostają zerowe. Zgłoszenia brakującego wierzchołka mogą dotyczyć jego wystąpienia na liście wielokąta albo jako końca krawędzi; w drugim przypadku podane jest również ID krawędzi. Metoda zbiera wszystkie wykryte problemy bez zmiany modelu i nie wykonuje automatycznej naprawy.
