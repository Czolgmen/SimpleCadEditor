# cad_core

Szkielet biblioteki statycznej C++20, niezależnej od WinAPI i GDI. Dotychczasowa deklaracja `Vec2` została przeniesiona do nagłówka, a definicje konstruktora i destruktora do pliku implementacji, w przestrzeni nazw `cad`. Pozostałe nagłówki zawierają miejsca na implementację; typy modelu i algorytmy nie są jeszcze napisane.

## Struktura

```text
cad_core/
  include/cad_core/
    geometry/
      Vec2.h
    model/
      Id.h
      Vertex.h
      Edge.h
      Polygon.h
      Document.h
  src/
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

Metody wyszukiwania i tworzenie wielokątów mają [testy dokumentu](../tests/README.md), uruchamiane z katalogu repozytorium poleceniem `.\tests\run-tests.cmd`.

1. `Vec2.h` i `Vec2.cpp`: współrzędne oraz operacje na wektorach.
2. `Id.h`: osobne typy identyfikatorów wierzchołków, krawędzi i wielokątów.
3. `Vertex.h`, `Edge.h`, `Polygon.h`: dane modelu, na początek wyłącznie odcinki.
4. `Document.h` i `Document.cpp`: własność danych i API dokumentu.

Podział odpowiedzialności opisuje [plan architektury](../docs/architecture/README.md).
