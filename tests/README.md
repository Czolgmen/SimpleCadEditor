# Weryfikacja poprawności

Projekt zawiera dwa niezależne programy testowe: `cad_core_tests` dla kodu niezależnego od Windows oraz `win32_tests` dla integracji z WinAPI. Pierwszy obejmuje **760 przypadków w 40 zestawach**, drugi **49 przypadków integracyjnych**. Testy sprawdzają wyniki, niezmienniki modelu i zachowanie po odrzuceniu operacji; kontrole pozostają aktywne także w Release.

## Uruchomienie

Wymagane są narzędzia C++ Visual Studio, MSVC v145 i Windows SDK 10. Skrypt odnajduje MSBuild, kompiluje oba projekty i uruchamia programy kolejno:

```powershell
.\tests\run-tests.cmd -Configuration Debug -Platform x64
.\tests\run-tests.cmd -Configuration Release -Platform x64
```

Każdy przypadek zgłasza `PASS` albo `FAIL`. Program podaje liczbę poprawnych wyników i zwraca kod różny od zera po niepowodzeniu. Zestaw natywny wymaga Windows. Uruchomienia korzystające z przechwycenia myszy nie powinny odbywać się równolegle.

## Testy niezależne od interfejsu

| Obszar | Lokalizacja | Zakres weryfikacji |
| --- | --- | --- |
| Geometria | `geometry/` | Operacje wektorowe, próbki Béziera, podział de Casteljau, parametry i styczne łuków |
| Model i topologia | `core/` | Identyfikatory, cykle wielokątów, walidacja, konwersje, podział i usuwanie wierzchołków |
| Relacje | `constraints/` | Reszty długości, równoległości, G1 i C1, poprawne zwroty stycznych i degeneracje |
| Solver | `solver/` | Algebra liniowa, Jacobian, układy mieszane, skalowanie, przypięcie punktów i brak częściowego zatwierdzenia |
| Edytor | `editor/` | Wybór elementów, wnętrze, uchwyty, gesty, anulowanie i transakcyjne polecenia |
| Rasteryzacja | `render/` | Oktanty Bresenhama, przycinanie, pokrycia Wu, szerokość obrysu i przypadki graniczne |
| Serializacja | `core/SceneIOTests.cpp` | Odtworzenie stanu, wersje formatu, ścisła walidacja i odrzucanie uszkodzonych danych |
| Styl krawędzi | `core/EdgeStyleTests.cpp` | Zakres szerokości i jej zachowanie po konwersji, podziale i zapisie |

Rejestr zestawów znajduje się w [TestsMain.cpp](TestsMain.cpp). Wspólny mechanizm asercji definiuje [TestSupport.h](TestSupport.h).

## Metody sprawdzania wyników

Próbki geometryczne są porównywane ze znanymi wartościami oraz niezależnymi wzorami. Testy obejmują translacje, obroty, odbicia, zmianę skali i odwrócenie orientacji. Dla łuków sprawdzane są także promień, zwrot i zgodność stycznych z różnicami numerycznymi. Przypadki graniczne obejmują wartości nieskończone, NaN, zerowe kierunki, zbieżne końce i przepełnienie.

Testy solvera oceniają surowe reszty poszczególnych relacji, dokładne pozycje punktów przypiętych i poprawność dokumentu. Kontrola nie wymaga konkretnego kształtu tam, gdzie układ dopuszcza wiele rozwiązań. Nieudane operacje są porównywane z migawką wejściową, łącznie z metadanymi i licznikami ID.

Testy zapisu i odczytu obejmują wersje 1 i 2, grubości krawędzi, luki w identyfikatorach oraz niedopuszczalne odwołania, sekcje i wartości. Odczyt ma odtwarzać zapisane współrzędne bez dopasowania solverem.

## Integracja z Windows

[PointerPreviewTests.cpp](platform/PointerPreviewTests.cpp) korzysta z rzeczywistej procedury okna i zasobów aplikacji. Okna pozostają ukryte, a wybrane wywołania systemowe są przechwytywane, aby sterować warunkami testu i rejestrować komunikaty bez otwierania modalnych okien.

Weryfikowane są koalescencja ruchu myszy, końcowa próbka gestu, żądanie odmalowania, anulowanie przez Esc lub utratę przechwycenia oraz awaria timera. Takty są dostarczane jawnie, co eliminuje zależność testu od terminowości zegara systemowego.

Kontrole DIB porównują piksele, kolory, granice obszaru sceny, zmianę rozmiaru i synchronizację z GDI. Testy obejmują wszystkie metody rysowania, wyróżnienie i niezależną grubość krawędzi, przycinanie oraz powrót do GDI przy braku bufora.

Testy pomocy i dokumentacji sprawdzają zasoby, polskie znaki, pięć tematów F1, dziesięć rozdziałów pełnego opisu, formatowanie i niezależny czas życia okien. Sprawdzana jest również dostępność poleceń menu i skrótów. Testy plików korzystają z izolowanego katalogu tymczasowego, obejmując anulowanie dialogu, zapis, odczyt i zachowanie wcześniejszego pliku po błędzie zapisu.

## Zakres oceny

Testy automatyczne obejmują przypadki typowe, graniczne i regresyjne. Nie zastępują oceny wizualnej ani nie dowodzą zbieżności dla dowolnej geometrii. Weryfikację interaktywną określa [scenariusz prezentacji](../docs/documentation.md#10-scenariusz-prezentacji); wydajność należy oceniać w konfiguracji Release i na określonym sprzęcie.
