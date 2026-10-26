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
            L"ZAZNACZANIE I PRZESUWANIE\r\n\r\n"
            L"LPM na wierzchołku lub kwadratowym uchwycie Béziera: zaznaczenie. "
            L"Przytrzymaj LPM i przesuń mysz, aby przeciągnąć element.\r\n"
            L"LPM wewnątrz figury: zaznaczenie i przeciąganie całego wielokąta. "
            L"Przesuwają się także jego uchwyty.\r\n"
            L"LPM blisko krawędzi: zaznaczenie krawędzi. Kliknięcie pustego tła usuwa zaznaczenie.\r\n"
            L"PPM na elemencie: menu dostępnych operacji.\r\n\r\n"
            L"KLAWISZE\r\n\r\n"
            L"Esc: anulowanie bieżącego przeciągania lub wyboru drugiej krawędzi. "
            L"Gdy żadna operacja nie trwa, usuwa zaznaczenie.\r\n"
            L"Delete: usunięcie zaznaczonego wierzchołka, jeśli figura ma więcej niż trzy wierzchołki.\r\n"
            L"Shift+F10: menu dla aktualnego zaznaczenia.\r\n"
            L"F1: otwarcie lub przywołanie tego okna.\r\n\r\n"
            L"Przyciski GDI / Bresenham nad sceną zmieniają sposób rysowania krawędzi i krzywych. "
            L"Geometria i ograniczenia pozostają takie same.\r\n\r\n"
            L"Pomoc można pozostawić otwartą i wrócić do sceny. Jej otwarcie anuluje trwający gest "
            L"oraz wybór drugiej krawędzi. Esc lub przycisk Zamknij zamyka okno pomocy. "
            L"Tekst można zaznaczać i kopiować skrótem Ctrl+C."},
        HelpTopic{L"Krawędzie i ograniczenia",
            L"STAŁA DŁUGOŚĆ\r\n\r\n"
            L"PPM na odcinku -> Ustaw stałą długość...\r\n"
            L"Okno podpowiada aktualną długość. Podaj dodatnią liczbę, np. 120 lub 12,5. "
            L"Możesz używać kropki albo przecinka dziesiętnego.\r\n"
            L"Dla istniejącej relacji menu pozwala zmienić długość lub usunąć ograniczenie. "
            L"Napis L=... przy krawędzi oznacza zadaną długość.\r\n\r\n"
            L"RÓWNOLEGŁOŚĆ\r\n\r\n"
            L"PPM na pierwszym odcinku -> Ustaw równoległość z... Następnie kliknij LPM drugi odcinek. "
            L"Może należeć do innego wielokąta. Pierwszy odcinek jest podczas wyboru pomarańczowy. "
            L"Esc anuluje wybór.\r\n"
            L"Oznaczenia P1, P2 itd. identyfikują pary. "
            L"PPM na odcinku -> Usuń równoległość -> wybrana para usuwa tylko tę relację.\r\n\r\n"
            L"Jeden odcinek może mieć długość i kilka relacji równoległości. "
            L"Długość oraz równoległość dotyczą wyłącznie prostych odcinków. "
            L"Béziery i łuki obsługują ciągłość opisaną w kolejnych tematach.\r\n\r\n"
            L"Przy konflikcie lub braku znalezionego rozwiązania operacja jest odrzucana. "
            L"Dotychczasowy dokument i ograniczenia pozostają zachowane."},
        HelpTopic{L"Bézier i ciągłość",
            L"KRZYWA BÉZIERA\r\n\r\n"
            L"PPM na krawędzi -> Zamień na Béziera. Dwa kwadratowe uchwyty sterują krzywą; "
            L"przerywane linie pokazują jej wielobok kontrolny. Przeciągaj uchwyty LPM.\r\n"
            L"PPM -> Zamień na odcinek przywraca prostą krawędź. "
            L"Przejście na krzywą usuwa długość i równoległości tej krawędzi; powrót ich nie odtwarza.\r\n\r\n"
            L"CIĄGŁOŚĆ W WIERZCHOŁKU\r\n\r\n"
            L"PPM na wierzchołku przy Bézierze -> Ciągłość wierzchołka:\r\n"
            L"G0: segmenty mają wspólny koniec, ale mogą tworzyć narożnik.\r\n"
            L"G1: kierunki i zwroty stycznych są zgodne; przejście jest styczne.\r\n"
            L"C1: wektory pochodnych są równe, także co do długości.\r\n\r\n"
            L"Po konwersji na Béziera domyślna jest G1. Wcześniejszy jawny wybór użytkownika pozostaje zachowany. "
            L"Aktualna klasa jest zaznaczona w menu.\r\n\r\n"
            L"Ciągłość i ograniczenia są rozwiązywane razem. Inne punkty mogą się dostosować. "
            L"Przy ruchu uchwytu program najpierw próbuje zachować końce jego krzywej; "
            L"jeśli to konieczne, dopuszcza ich ruch. Przy zaakceptowanym ruchu prowadzony element "
            L"zachowuje pozycję wyznaczoną przez kursor i miejsce złapania."},
        HelpTopic{L"Łuki i zmiany topologii",
            L"ŁUK OKRĘGU\r\n\r\n"
            L"PPM na krawędzi -> Zamień na łuk okręgu. Powrót: Zamień na odcinek.\r\n"
            L"Przy łuku dostępne są G0 i G1; C1 jest wyłączone. Najwyżej jeden koniec łuku może mieć G1. "
            L"Przy G0 na obu końcach powstaje półokrąg, którego promień jest równy połowie długości cięciwy. "
            L"Przy jednym G1 łuk dostosowuje się do stycznej sąsiada.\r\n"
            L"Jeśli konwersja jest odrzucana, sprawdź klasy ciągłości na obu końcach "
            L"i ograniczenia sąsiednich odcinków. Końce łuku muszą być różne.\r\n\r\n"
            L"DODAWANIE WIERZCHOŁKA\r\n\r\n"
            L"PPM na krawędzi -> Podziel krawędź. Miejsce kliknięcia nie wyznacza punktu podziału.\r\n"
            L"Odcinek: podział w środku.\r\n"
            L"Bézier: podział dla parametru t=0,5, z zachowaniem kształtu przed korektą ciągłości. "
            L"Nowy wierzchołek otrzymuje domyślne G1.\r\n"
            L"Łuk: zastąpienie dwoma odcinkami przez środkowy punkt łuku, z nowym G0. "
            L"Ta operacja zmienia kształt.\r\n\r\n"
            L"USUWANIE WIERZCHOŁKA\r\n\r\n"
            L"PPM na wierzchołku -> Usuń wierzchołek albo klawisz Delete. "
            L"Dwie sąsiednie krawędzie zostają zastąpione jednym odcinkiem. Trójkąt nie może utracić wierzchołka.\r\n\r\n"
            L"Podział i usuwanie czyszczą relacje zastępowanych krawędzi. "
            L"Pozostałe zależności są ponownie rozwiązywane; nieudana operacja nie zmienia dokumentu."},
        HelpTopic{L"Scena startowa i komunikaty",
            L"SCENA DEMONSTRACYJNA\r\n\r\n"
            L"Lewa figura zawiera Béziera: C1 przy pionowym boku i G1 przy ukośnym. "
            L"Jej zadane długości wynoszą 180 i 120.\r\n"
            L"Prawa figura zawiera łuk G1 oraz półokrąg G0/G0. Długość dolnego odcinka wynosi 160.\r\n"
            L"Dwie pary równoległości łączą dolne i pionowe odcinki obu figur.\r\n\r\n"
            L"Spróbuj przesunąć uchwyt Béziera, wierzchołek łuku oraz całą figurę. "
            L"Porównaj tryby GDI i Bresenham. Z menu zmień długość, ciągłość albo typ krawędzi.\r\n\r\n"
            L"BRAK SPÓJNEGO STANU\r\n\r\n"
            L"Podczas przeciągania komunikat na dole oznacza, że dla ostatniej pozycji "
            L"nie udało się znaleźć poprawnego rozwiązania. Na scenie pozostaje ostatni poprawny podgląd. "
            L"Możesz kontynuować ruch, puścić LPM, aby zachować ten podgląd, albo nacisnąć Esc, "
            L"aby cofnąć cały gest.\r\n"
            L"Brak znalezionego rozwiązania może wynikać z konfliktu relacji, zdegenerowanej geometrii "
            L"lub ograniczeń obliczeń numerycznych.\r\n\r\n"
            L"Zmiany sceny nie są zapisywane do pliku. Ponowne uruchomienie programu odtwarza scenę początkową. "
            L"Wielokąty są przygotowane w kodzie; aplikacja umożliwia ich edycję."}
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
