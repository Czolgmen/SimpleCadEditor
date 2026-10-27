# 07. Organizacja projektu i budowanie

[Spis dokumentacji architektury](README.md)

## Układ katalogów

```text
cad_core/
  include/cad_core/    publiczny interfejs rdzenia
  src/
    geometry/         wektory, Bézier i łuki
    model/            dokument, topologia, styczne i próbkowanie
    constraints/      definicje i reszty ograniczeń
    solver/           Jacobian, algebra liniowa i iteracje
    io/               serializacja sceny
    demo/             scena demonstracyjna
src/
  editor/             sesja, zaznaczenie, gesty i hit testing
  render/             przycinanie, Bresenham i Wu
  platform/win32/     DIB, GDI, dialogi i obsługa plików
tests/                testy rdzenia, edycji, rasteryzacji i Windows
docs/                 dokumentacja użytkowa i techniczna
scripts/              budowanie wersji dystrybucyjnej
dist/windows-x64/     samodzielny EXE i suma SHA-256
```

[`Projecr1_GK.cpp`](../../Projecr1_GK.cpp) zawiera punkt wejścia aplikacji i procedurę okna. Plik `.rc` definiuje menu, akceleratory, dialogi oraz zasoby dokumentacji. Nagłówki rdzenia nie zależą od `framework.h` ani nagłówków Windows.

## Projekty Visual Studio

[`Projecr1_GK.slnx`](../../Projecr1_GK.slnx) obejmuje aplikację, bibliotekę statyczną `cad_core`, przenośny zestaw `cad_core_tests` i zestaw integracyjny `win32_tests`. Kod edytora i rasteryzacji jest kompilowany w projektach aplikacji oraz testów; nie tworzy osobnych bibliotek `cad_editor` lub `cad_render`.

Wymagane są C++20, narzędzia MSVC v145, Windows SDK 10 i MSBuild obsługujący `.slnx`. Dostępne są konfiguracje Debug/Release oraz x64/Win32. Release korzysta ze statycznej biblioteki wykonawczej C++ (`/MT`).

```powershell
msbuild .\Projecr1_GK.slnx /m /p:Configuration=Release /p:Platform=x64
.\tests\run-tests.cmd -Configuration Release
.\scripts\build-release.cmd
```

Pierwsze polecenie wymaga Developer PowerShell dla Visual Studio. Skrypty samodzielnie odnajdują instalację narzędzi. Skrypt dystrybucji buduje aplikację, kopiuje EXE i aktualizuje sumę kontrolną; nie wykonuje operacji Git.

## Dokumentacja i dystrybucja

Pomoc F1 znajduje się w [`HelpDialog.cpp`](../../src/platform/win32/HelpDialog.cpp). Pełna [dokumentacja projektu](../documentation.md) jest dołączana jako zasób UTF-8 i prezentowana w dziesięciu rozdziałach. Zmiana tekstu wymaga ponownej kompilacji EXE. Opis algorytmów z pseudokodem i dokumenty architektury uzupełniają wersję dostępną w aplikacji.

Zestaw testów i sposób jego uruchomienia opisuje [dokumentacja weryfikacji](../../tests/README.md). Gotowy [pakiet Windows x64](../../dist/windows-x64/README.md) obejmuje program oraz informacje o uruchomieniu i kontroli integralności.
