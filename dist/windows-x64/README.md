# Gotowy program dla Windows x64

Uruchom **Projecr1_GK.exe** dwukrotnym kliknięciem. Program jest przeznaczony dla 64-bitowego Windows 10/11.

Można skopiować sam plik EXE na drugi komputer. Nie wymaga Visual Studio, kompilatora ani osobnej instalacji bibliotek Microsoft Visual C++: wersja Release ma je dołączone statycznie. Korzysta z bibliotek systemowych Windows. Scena początkowa, ikony, pomoc F1 i pełna dokumentacja są wbudowane w EXE.

Zmiany sceny nie są zapisywane między uruchomieniami. Instrukcję obsługi otwiera F1, a opis algorytmów przycisk Dokumentacja.

Plik `SHA256SUMS.txt` zawiera sumę kontrolną bieżącego EXE. W PowerShell można ją sprawdzić poleceniem:

```powershell
Get-FileHash .\Projecr1_GK.exe -Algorithm SHA256
```

## Odświeżanie wersji w repozytorium

Po zmianach kodu lub dokumentacji uruchom z katalogu repozytorium:

```powershell
.\scripts\build-release.cmd
```

Do tej operacji potrzebne są narzędzia Visual Studio opisane w [dokumentacji budowania](../../docs/documentation.md#9-budowanie-testy-i-przekazanie-programu). Skrypt buduje Release x64, kopiuje aktualny EXE tutaj i aktualizuje sumę kontrolną. Aktualizację paczki należy dołączyć do commita razem ze zmianami źródeł. Skrypt nie wykonuje commitów.
