#include <platform/win32/HelpDialog.h>
#include "../../../Resource.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    struct HelpTopic
    {
        const wchar_t* title;
        const wchar_t* text;
    };

    constexpr std::array topics{
        HelpTopic{L"Obsługa i skróty",
            L"ZAZNACZANIE I PRZESUWANIE\r\n"
            L"\r\n"
            L"LPM na wierzchołku lub kwadratowym uchwycie Béziera zaznacza element. Przeciągnięcie z wciśniętym LPM zmienia jego położenie. Kliknięcie wewnątrz figury umożliwia przesuwanie całego wielokąta wraz z uchwytami. Kliknięcie blisko krawędzi zaznacza krawędź, a kliknięcie pustego tła usuwa zaznaczenie. PPM otwiera menu operacji dotyczących wskazanego elementu.\r\n"
            L"\r\n"
            L"SKRÓTY KLAWISZOWE\r\n"
            L"\r\n"
            L"Esc — anulowanie przeciągania lub wyboru drugiej krawędzi; poza operacją usuwa zaznaczenie.\r\n"
            L"Delete — usunięcie zaznaczonego wierzchołka, jeżeli wielokąt ma więcej niż trzy wierzchołki.\r\n"
            L"Shift+F10 — menu kontekstowe zaznaczonego elementu.\r\n"
            L"F1 — instrukcja obsługi.\r\n"
            L"Ctrl+O — Załaduj scenę.\r\n"
            L"Ctrl+Shift+S — Zapisz jako.\r\n"
            L"\r\n"
            L"PREZENTACJA I DOKUMENTACJA\r\n"
            L"\r\n"
            L"Przyciski GDI, Bresenham i Wu (AA) wybierają metodę renderowania bez zmiany geometrii i ograniczeń. Tryb Wu (AA) wygładza brzegi przez mieszanie koloru obrysu z tłem. Przycisk Dokumentacja otwiera opis modelu, algorytmów i solvera.\r\n"
            L"\r\n"
            L"Pomoc działa w osobnym oknie. Jej otwarcie anuluje trwający gest oraz wybór pary krawędzi. Esc lub Zamknij zamyka okno; Ctrl+C kopiuje zaznaczony tekst. Treść pomocy i dokumentacji jest dołączona do pliku wykonywalnego."},
        HelpTopic{L"Krawędzie i ograniczenia",
            L"STAŁA DŁUGOŚĆ\r\n"
            L"\r\n"
            L"Polecenie PPM na odcinku → Ustaw stałą długość... otwiera dialog z bieżącą długością jako wartością początkową. Wymagana jest dodatnia, skończona liczba. Dopuszczalne są kropka lub przecinek dziesiętny oraz notacja wykładnicza. Istniejące ograniczenie można zmienić albo usunąć z menu. Etykieta L=... przedstawia zadaną długość.\r\n"
            L"\r\n"
            L"RÓWNOLEGŁOŚĆ\r\n"
            L"\r\n"
            L"Polecenie PPM na pierwszym odcinku → Ustaw równoległość z... rozpoczyna wybór pary. Drugi odcinek wskazuje się LPM; może należeć do innego wielokąta. Pierwsza krawędź jest wyróżniona kolorem pomarańczowym. Esc anuluje wybór. Etykiety P1, P2 itd. identyfikują pary; menu Usuń równoległość umożliwia usunięcie pojedynczej relacji.\r\n"
            L"\r\n"
            L"Odcinek może jednocześnie mieć stałą długość i kilka relacji równoległości. Oba rodzaje ograniczeń dotyczą wyłącznie odcinków. Krzywe Béziera i łuki uczestniczą w warunkach ciągłości.\r\n"
            L"\r\n"
            L"GRUBOŚĆ KRAWĘDZI\r\n"
            L"\r\n"
            L"Polecenie Grubość krawędzi... ustawia całkowitą szerokość od 1 do 32 px dla jednej krawędzi dowolnego typu. Wartość domyślna wynosi 2 px; zaznaczenie zwiększa szerokość wyświetlania o 2 px. Ustawienie obowiązuje we wszystkich trybach renderowania i jest zapisywane ze sceną.\r\n"
            L"\r\n"
            L"Zmiana wymagająca rozwiązania ograniczeń jest zatwierdzana wyłącznie po uzyskaniu poprawnego stanu. Odrzucenie operacji zachowuje dotychczasowy dokument."},
        HelpTopic{L"Bézier i ciągłość",
            L"KRZYWA BÉZIERA\r\n"
            L"\r\n"
            L"Polecenie Zamień na Béziera zastępuje wskazaną krawędź krzywą trzeciego stopnia. Dwa kwadratowe uchwyty wyznaczają punkty kontrolne; linie przerywane przedstawiają wielobok kontrolny. Uchwyty są przesuwane przez przeciągnięcie LPM. Polecenie Zamień na odcinek przywraca prostą krawędź.\r\n"
            L"\r\n"
            L"Konwersja na krzywą usuwa ograniczenia długości i równoległości tej krawędzi. Konwersja powrotna nie odtwarza usuniętych relacji.\r\n"
            L"\r\n"
            L"CIĄGŁOŚĆ W WIERZCHOŁKU\r\n"
            L"\r\n"
            L"Menu Ciągłość wierzchołka określa sposób połączenia krawędzi:\r\n"
            L"G0 — wspólny punkt końcowy; dopuszczalny narożnik.\r\n"
            L"G1 — zgodne kierunki i zwroty niezerowych stycznych.\r\n"
            L"C1 — równe wektory pochodnych, z uwzględnieniem ich długości i lokalnej parametryzacji segmentów.\r\n"
            L"\r\n"
            L"Konwersja na Béziera nadaje domyślnie G1 na końcach, których ciągłość nie została wcześniej ustawiona jawnie. Jawny wybór użytkownika pozostaje zachowany. Bieżące ustawienie jest oznaczone w menu.\r\n"
            L"\r\n"
            L"Solver rozwiązuje ciągłość łącznie z pozostałymi ograniczeniami. Przeciąganie uchwytu rozpoczyna się od próby zachowania końców krzywej; w razie potrzeby dopuszczany jest ich ruch. Przyjęty wynik zachowuje dokładne położenie prowadzonego punktu wynikające z kursora i miejsca uchwycenia."},
        HelpTopic{L"Łuki i zmiany topologii",
            L"ŁUK OKRĘGU\r\n"
            L"\r\n"
            L"Polecenie Zamień na łuk okręgu tworzy krawędź kołową. Dostępne klasy ciągłości to G0 i G1; C1 jest niedostępne, a najwyżej jeden koniec łuku może mieć G1. Przy G0 na obu końcach łuk jest półokręgiem o promieniu równym połowie długości cięciwy. Pojedyncze G1 wyznacza kształt łuku na podstawie stycznej sąsiedniej krawędzi. Końce łuku muszą mieć różne położenia.\r\n"
            L"\r\n"
            L"PODZIAŁ KRAWĘDZI\r\n"
            L"\r\n"
            L"Polecenie Podziel krawędź dodaje wierzchołek niezależnie od miejsca kliknięcia:\r\n"
            L"Odcinek — podział w środku długości.\r\n"
            L"Bézier — podział algorytmem de Casteljau dla t=0,5; przed ponownym rozwiązaniem ograniczeń zachowany jest kształt. Nowy wierzchołek otrzymuje domyślne G1.\r\n"
            L"Łuk — zastąpienie dwoma odcinkami przez punkt łuku dla t=0,5. Nowy wierzchołek otrzymuje G0; operacja zmienia kształt obrysu.\r\n"
            L"\r\n"
            L"USUWANIE WIERZCHOŁKA\r\n"
            L"\r\n"
            L"Polecenie Usuń wierzchołek lub Delete zastępuje dwie sąsiednie krawędzie jednym odcinkiem. Wielokąt musi zachować co najmniej trzy wierzchołki.\r\n"
            L"\r\n"
            L"Zmiana topologii usuwa relacje zastępowanych krawędzi i ponownie rozwiązuje pozostałe zależności. Nieudana operacja zachowuje dokument oraz identyfikatory. Zamiana łuku na odcinek jest dostępna z menu kontekstowego."},
        HelpTopic{L"Scena i komunikaty",
            L"SCENA DEMONSTRACYJNA\r\n"
            L"\r\n"
            L"Scena początkowa zawiera dwa wielokąty. Lewy obejmuje krzywą Béziera z połączeniem C1 przy pionowym boku oraz G1 przy ukośnym. Zadane długości wynoszą 180 i 120. Prawy wielokąt zawiera łuk G1, półokrąg G0/G0 i odcinek o zadanej długości 160. Dwie relacje równoległości łączą odpowiednie dolne i pionowe odcinki obu figur.\r\n"
            L"\r\n"
            L"Scena umożliwia prezentację ruchu uchwytów, edycji łuków, translacji figur, zmian ograniczeń i porównania trzech metod renderowania.\r\n"
            L"\r\n"
            L"OBSŁUGA NIEUDANEJ PRÓBY\r\n"
            L"\r\n"
            L"Komunikat podczas przeciągania oznacza, że dla ostatniego położenia kursora nie uzyskano poprawnego rozwiązania. Widoczny pozostaje ostatni zaakceptowany podgląd. Dalszy ruch umożliwia ponowienie próby. Zwolnienie LPM zatwierdza ostatni poprawny stan, natomiast Esc odtwarza dokument sprzed całego gestu.\r\n"
            L"\r\n"
            L"Brak zbieżności może wynikać z konfliktu relacji, degeneracji geometrii albo ograniczeń metody numerycznej; nie stanowi dowodu matematycznej sprzeczności układu.\r\n"
            L"\r\n"
            L"ZAPIS I ODCZYT\r\n"
            L"\r\n"
            L"Zapisz jako... zapisuje geometrię, grubości i relacje w pliku .cadscene. Załaduj... zastępuje bieżącą scenę poprawnie odczytanym dokumentem. Plik uszkodzony lub niespójny jest odrzucany bez zastępowania sceny. Aplikacja nie prowadzi automatycznego zapisu; uruchomienie programu odtwarza scenę demonstracyjną. Wczytanie zastępuje bieżące dane bez pytania o zapis zmian."}
    };

    struct DialogTopic
    {
        std::wstring title;
        std::wstring text;
    };

    struct DialogState
    {
        HWND* slot;
        std::vector<DialogTopic> topics;
        std::size_t initialTopic = 0;
        bool ownedByDialog = false;
    };

    std::wstring inlineText(std::wstring_view line)
    {
        std::wstring result;
        for (std::size_t i = 0; i < line.size();)
        {
            if (line.substr(i, 2) == L"**") { i += 2; continue; }
            if (line[i] == L'`') { ++i; continue; }
            if (line[i] == L'[')
            {
                const auto labelEnd = line.find(L"](", i + 1);
                const auto linkEnd = labelEnd == std::wstring_view::npos
                    ? std::wstring_view::npos : line.find(L')', labelEnd + 2);
                if (linkEnd != std::wstring_view::npos)
                {
                    result.append(line.substr(i + 1, labelEnd - i - 1));
                    i = linkEnd + 1;
                    continue;
                }
            }
            result += line[i++];
        }
        return result;
    }

    std::wstring plainText(std::wstring_view markdown)
    {
        // Format the small Markdown subset used by our manual. Code blocks
        // remain literal; links are labels because this is a text viewer.
        std::wstring text;
        bool code = false;
        while (!markdown.empty())
        {
            const auto newline = markdown.find(L'\n');
            auto line = markdown.substr(0, newline);
            if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);
            markdown = newline == std::wstring_view::npos ? std::wstring_view{} : markdown.substr(newline + 1);
            auto trimmed = line;
            const auto first = trimmed.find_first_not_of(L" \t");
            if (first != std::wstring_view::npos) trimmed.remove_prefix(first);
            if (trimmed.starts_with(L"```")) { code = !code; continue; }
            if (code)
                text.append(line);
            else
            {
                if (trimmed.starts_with(L'|'))
                {
                    if (trimmed.find_first_not_of(L"|-: \t") == std::wstring_view::npos) continue;
                    trimmed.remove_prefix(1);
                    if (!trimmed.empty() && trimmed.back() == L'|') trimmed.remove_suffix(1);
                    line = trimmed;
                }
                if (trimmed.starts_with(L'#'))
                {
                    const auto heading = trimmed.find_first_not_of(L"# ");
                    line = heading == std::wstring_view::npos ? std::wstring_view{} : trimmed.substr(heading);
                }
                text += inlineText(line);
            }
            text += L"\r\n";
        }
        return text;
    }

    std::vector<DialogTopic> documentationTopics(HINSTANCE instance)
    {
        const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_DOCUMENTATION), RT_RCDATA);
        if (!resource) return {};
        const DWORD size = SizeofResource(instance, resource);
        const auto* data = static_cast<const char*>(LockResource(LoadResource(instance, resource)));
        if (!data || size == 0 || size > static_cast<DWORD>((std::numeric_limits<int>::max)())) return {};
        const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(size), nullptr, 0);
        if (characters <= 0) return {};
        std::wstring markdown(static_cast<std::size_t>(characters), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(size),
            markdown.data(), characters) != characters) return {};

        std::vector<DialogTopic> result;
        std::wstring title, body;
        const auto appendTopic = [&]
        {
            if (!title.empty() && title != L"Spis treści") result.push_back({title, plainText(body)});
        };
        std::wstring_view remaining = markdown;
        while (!remaining.empty())
        {
            const auto newline = remaining.find(L'\n');
            auto line = remaining.substr(0, newline);
            if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);
            remaining = newline == std::wstring_view::npos ? std::wstring_view{} : remaining.substr(newline + 1);
            if (line.starts_with(L"## "))
            {
                appendTopic();
                title = line.substr(3);
                body.clear();
            }
            else
            {
                body.append(line);
                body += L'\n';
            }
        }
        appendTopic();
        return result;
    }

    void selectTopic(HWND dialog)
    {
        const auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
        const auto index = SendDlgItemMessageW(dialog, IDC_HELP_TOPIC, CB_GETCURSEL, 0, 0);
        if (!state || index < 0 || static_cast<std::size_t>(index) >= state->topics.size())
            return;
        SetDlgItemTextW(dialog, IDC_HELP_TEXT, state->topics[static_cast<std::size_t>(index)].text.c_str());
        SendDlgItemMessageW(dialog, IDC_HELP_TEXT, EM_SETSEL, 0, 0);
        SendDlgItemMessageW(dialog, IDC_HELP_TEXT, EM_SCROLLCARET, 0, 0);
    }

    void layout(HWND dialog)
    {
        RECT client{}, units{0, 0, 8, 14};
        if (!GetClientRect(dialog, &client) || !MapDialogRect(dialog, &units))
            return;
        const int margin = units.right;
        const int controlHeight = units.bottom;
        const int width = (std::max)(1L, client.right - 2 * margin);
        const int textTop = margin + controlHeight + margin / 2;
        const int buttonTop = (std::max)(textTop + margin, static_cast<int>(client.bottom) - margin - controlHeight);
        MoveWindow(GetDlgItem(dialog, IDC_HELP_TOPIC), margin, margin, width, controlHeight * 8, TRUE);
        MoveWindow(GetDlgItem(dialog, IDC_HELP_TEXT), margin, textTop, width,
            (std::max)(1, buttonTop - margin - textTop), TRUE);
        MoveWindow(GetDlgItem(dialog, IDCANCEL), (std::max)(margin, static_cast<int>(client.right) - 7 * margin),
            buttonTop, 6 * margin, controlHeight, TRUE);
    }

    INT_PTR CALLBACK helpDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* state = reinterpret_cast<DialogState*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            *state->slot = dialog;
            for (const auto& topic : state->topics)
                SendDlgItemMessageW(dialog, IDC_HELP_TOPIC, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(topic.title.c_str()));
            SendDlgItemMessageW(dialog, IDC_HELP_TEXT, EM_SETLIMITTEXT, 1024 * 1024, 0);
            SendDlgItemMessageW(dialog, IDC_HELP_TOPIC, CB_SETCURSEL, state->initialTopic, 0);
            selectTopic(dialog);
            layout(dialog);
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_HELP_TOPIC && HIWORD(wParam) == CBN_SELCHANGE)
            {
                selectTopic(dialog);
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDOK)
            {
                DestroyWindow(dialog);
                return TRUE;
            }
            break;
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) layout(dialog);
            return TRUE;
        case WM_GETMINMAXINFO:
        {
            RECT minimum{0, 0, 260, 180};
            MapDialogRect(dialog, &minimum);
            AdjustWindowRectEx(&minimum, static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_STYLE)),
                FALSE, static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_EXSTYLE)));
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize = {minimum.right - minimum.left, minimum.bottom - minimum.top};
            return TRUE;
        }
        case WM_CLOSE:
            DestroyWindow(dialog);
            return TRUE;
        case WM_NCDESTROY:
        {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
            SetWindowLongPtrW(dialog, DWLP_USER, 0);
            if (state)
            {
                if (*state->slot == dialog) *state->slot = nullptr;
                if (state->ownedByDialog) delete state;
            }
            break;
        }
        }
        return FALSE;
    }

    HWND createTopicDialog(HINSTANCE instance, HWND owner, HWND& windowSlot,
        const wchar_t* caption, std::vector<DialogTopic> content, std::size_t initialTopic = 0)
    {
        windowSlot = nullptr;
        auto state = std::make_unique<DialogState>(DialogState{&windowSlot, std::move(content), initialTopic});
        const HWND dialog = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_HELP), owner,
            helpDialogProc, reinterpret_cast<LPARAM>(state.get()));
        if (!dialog || !IsWindow(dialog))
        {
            windowSlot = nullptr;
            return nullptr;
        }
        // Transfer only after successful creation. If initialization destroys
        // the window, the caller still owns and releases the state.
        state->ownedByDialog = true;
        state.release();
        SetWindowTextW(dialog, caption);

        RECT parent{}, bounds{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetWindowRect(owner, &parent) && GetWindowRect(dialog, &bounds) &&
            GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const int width = (std::min)(bounds.right - bounds.left, monitor.rcWork.right - monitor.rcWork.left);
            const int height = (std::min)(bounds.bottom - bounds.top, monitor.rcWork.bottom - monitor.rcWork.top);
            const int x = (std::clamp)(parent.left + (parent.right - parent.left - width) / 2,
                monitor.rcWork.left, monitor.rcWork.right - width);
            const int y = (std::clamp)(parent.top + (parent.bottom - parent.top - height) / 2,
                monitor.rcWork.top, monitor.rcWork.bottom - height);
            SetWindowPos(dialog, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return dialog;
    }
}

namespace cad::win32
{
    bool handleHelpDialogMessage(HWND dialog, MSG& message)
    {
        if (!dialog || !IsWindow(dialog) || (message.hwnd != dialog && !IsChild(dialog, message.hwnd)))
            return false;
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE)
        {
            DestroyWindow(dialog);
            return true;
        }
        return IsDialogMessageW(dialog, &message) != FALSE;
    }

    HWND createHelpDialog(HINSTANCE instance, HWND owner, HWND& windowSlot)
    {
        if (windowSlot && IsWindow(windowSlot)) return windowSlot;
        std::vector<DialogTopic> content;
        for (const auto& topic : topics) content.push_back({topic.title, topic.text});
        return createTopicDialog(instance, owner, windowSlot, L"Pomoc - Edytor wielokątów", std::move(content));
    }

    HWND createDocumentationDialog(HINSTANCE instance, HWND owner, HWND& windowSlot)
    {
        if (windowSlot && IsWindow(windowSlot)) return windowSlot;
        windowSlot = nullptr;
        auto content = documentationTopics(instance);
        if (content.empty()) return nullptr;
        std::size_t initial = 0;
        for (std::size_t i = 0; i < content.size(); ++i)
            if (content[i].title.find(L"Algorytmy geometryczne") != std::wstring::npos) initial = i;
        return createTopicDialog(instance, owner, windowSlot, L"Dokumentacja - algorytmy i model", std::move(content), initial);
    }
}
