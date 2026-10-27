# Architektura edytora wielokątów

Dokumentacja przedstawia strukturę zaimplementowanego projektu w C++20. Podział odpowiedzialności oddziela model i obliczenia geometryczne od sesji edycji oraz interfejsu WinAPI. Pozwala wykorzystać rdzeń i logikę edycji w innym interfejsie po przygotowaniu adapterów wejścia, wyświetlania i operacji plikowych.

| Dokument | Zakres |
| --- | --- |
| [01. Warstwy](01-layers.md) | Zależności, własność danych i przepływ sterowania |
| [02. Model danych](02-data-model.md) | Encje, identyfikatory, topologia i serializacja |
| [03. Ograniczenia](03-constraints.md) | Równania, solver, transakcje i diagnostyka |
| [04. Krzywe](04-curves.md) | Bézier, łuki, styczne i ciągłość |
| [05. Sesja edytora](05-editor.md) | Zaznaczenie, gesty i operacje użytkownika |
| [06. Renderowanie](06-rendering.md) | GDI, Bresenham, Wu i bufor obrazu |
| [07. Organizacja projektu](07-implementation.md) | Katalogi, projekty Visual Studio i budowanie |
| [08. Zakres funkcjonalny](08-requirements.md) | Realizacja wymagań i decyzje projektowe |
| [09. Macierz Jacobiego](09-jacobian.md) | Różnice skończone, skalowanie i zależności |

Instrukcję obsługi, parametry numeryczne i scenariusz prezentacji zawiera [dokumentacja projektu](../documentation.md). Szczegółowy opis metod wraz z pseudokodem znajduje się w [dokumencie algorytmów](../algorithms.md), a strukturę pliku `.cadscene` określa [specyfikacja formatu sceny](../scene-format.md).
