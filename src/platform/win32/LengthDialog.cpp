#include <platform/win32/LengthDialog.h>
#include <editor/LengthInput.h>

#include "../../../Resource.h"

#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    struct LengthDialogState
    {
        double initial;
        std::optional<double> result;
    };

    INT_PTR CALLBACK lengthDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_INITDIALOG)
        {
            auto* state = reinterpret_cast<LengthDialogState*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            std::wostringstream value;
            value.imbue(std::locale::classic());
            value << std::setprecision(std::numeric_limits<double>::max_digits10) << state->initial;
            SetDlgItemTextW(dialog, IDC_LENGTH_EDIT, value.str().c_str());
            SendDlgItemMessageW(dialog, IDC_LENGTH_EDIT, EM_SETLIMITTEXT, 128, 0);
            SetFocus(GetDlgItem(dialog, IDC_LENGTH_EDIT));
            SendDlgItemMessageW(dialog, IDC_LENGTH_EDIT, EM_SETSEL, 0, -1);
            return FALSE;
        }
        if (message == WM_COMMAND)
        {
            if (LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK)
            {
                wchar_t text[129]{};
                GetDlgItemTextW(dialog, IDC_LENGTH_EDIT, text, static_cast<int>(std::size(text)));
                const auto length = cad::editor::parsePositiveLength(text);
                if (!length)
                {
                    MessageBoxW(dialog, L"Podaj dodatnią, skończoną długość, np. 100 lub 12,5.",
                        L"Niepoprawna długość", MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(dialog, IDC_LENGTH_EDIT));
                    SendDlgItemMessageW(dialog, IDC_LENGTH_EDIT, EM_SETSEL, 0, -1);
                    return TRUE;
                }
                auto* state = reinterpret_cast<LengthDialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
                state->result = *length;
                EndDialog(dialog, IDOK);
                return TRUE;
            }
        }
        if (message == WM_CLOSE)
        {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
}

namespace cad::win32
{
    std::optional<double> askFixedLength(HINSTANCE instance, HWND owner, double currentLength)
    {
        LengthDialogState state{currentLength, std::nullopt};
        const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_FIXED_LENGTH), owner,
            lengthDialogProc, reinterpret_cast<LPARAM>(&state));
        if (result == IDOK)
            return state.result;
        if (result == -1)
            MessageBoxW(owner, L"Nie udało się otworzyć okna długości.", L"Stała długość", MB_OK | MB_ICONERROR);
        return std::nullopt;
    }
}
