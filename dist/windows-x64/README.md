# Dystrybucja aplikacji — Windows x64

Plik **Projecr1_GK.exe** jest wersją Release przeznaczoną dla Windows 10/11 x64. Uruchomienie nie wymaga kompilacji, Visual Studio ani osobnej instalacji pakietu Visual C++ Redistributable: biblioteka wykonawcza C++ jest dołączona statycznie. Program korzysta z bibliotek systemowych Windows.

Scena demonstracyjna, ikony, instrukcja F1 i pełna dokumentacja są zawarte w pliku wykonywalnym. EXE może zostać skopiowany i uruchomiony samodzielnie na innym komputerze spełniającym wymagania systemowe.

## Obsługa i dane użytkownika

Instrukcję otwiera F1, a opis modelu i algorytmów przycisk Dokumentacja. Polecenie **Plik → Zapisz jako...** (`Ctrl+Shift+S`) zapisuje scenę w pliku `.cadscene`; **Plik → Załaduj...** (`Ctrl+O`) odtwarza geometrię, grubości i ograniczenia. Pliki scen są niezależne od EXE i mogą być przenoszone między komputerami. Program nie prowadzi automatycznego zapisu.

## Integralność pliku

`SHA256SUMS.txt` zawiera sumę kontrolną bieżącego EXE. Wartość obliczona poniższym poleceniem powinna być zgodna z zapisanym skrótem:

```powershell
Get-FileHash .\Projecr1_GK.exe -Algorithm SHA256
```

## Odtworzenie dystrybucji ze źródeł

Z katalogu repozytorium należy uruchomić:

```powershell
.\scripts\build-release.cmd
```

Skrypt buduje Release x64, kopiuje program do katalogu dystrybucji i aktualizuje sumę SHA-256. Wymagania narzędziowe opisuje [dokumentacja projektu](../../docs/documentation.md#9-budowanie-testy-i-przekazanie-programu). Aktualizacja źródeł lub dokumentacji osadzonej w zasobach wymaga ponownego zbudowania dystrybucji. Skrypt nie wykonuje operacji Git.
