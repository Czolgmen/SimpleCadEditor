# Edytor wielokątów

Projekt w C++20 z frontendem WinAPI/GDI i niezależnym od Windows rdzeniem geometrii. Wariant A–K: równoległość odcinków i domyślne G1 przy konwersji na Béziera.

**Uruchomienie bez kompilacji:** pobierz [Projecr1_GK.exe](dist/windows-x64/Projecr1_GK.exe) i uruchom na 64-bitowym Windows 10/11. Wystarczy sam EXE — scena, pomoc i dokumentacja są wbudowane. Biblioteki C++ są dołączone statycznie; nie trzeba instalować Visual Studio ani pakietu Visual C++ Redistributable. [Opis paczki i suma kontrolna](dist/windows-x64/README.md).

- [Dokumentacja programu](docs/documentation.md): obsługa, założenia, model danych, algorytmy i scenariusz demonstracji.
- **F1** lub **Pomoc → Obsługa programu**: skrócona, wbudowana pomoc dostępna również bez plików dokumentacji.
- Przycisk **Dokumentacja** lub **Pomoc → Dokumentacja algorytmów...**: osobne okno z rozdziałami pełnej dokumentacji, w tym modelem danych i solverem. Treść z docs/documentation.md jest dołączana do EXE podczas kompilacji.
- [Architektura](docs/architecture/README.md): opis warstw i decyzji projektowych, wraz z wcześniejszymi planami rozbudowy.
- [Testy](tests/README.md): zakres i uruchamianie zestawów przenośnych oraz WinAPI.

Jeżeli chcesz zbudować program ze źródeł, w Developer PowerShell dla Visual Studio, z katalogu repozytorium:

```powershell
msbuild .\Projecr1_GK.slnx /m /p:Configuration=Release /p:Platform=x64
.\x64\Release\Projecr1_GK.exe
```

Do kompilacji potrzebne są narzędzia C++ v145 i Windows SDK 10. Po zmianach kodu lub dokumentacji uruchom `.\scripts\build-release.cmd`, aby odświeżyć EXE w `dist/windows-x64/` oraz jego sumę SHA256. Pełna instrukcja budowania i przekazania programu znajduje się w [dokumentacji](docs/documentation.md#9-budowanie-testy-i-przekazanie-programu).
