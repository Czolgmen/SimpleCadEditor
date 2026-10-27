#include "SceneFiles.h"
#include <cad_core/io/SceneIO.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
    std::wstring wide(const std::string& text)
    {
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    std::wstring systemError(const wchar_t* operation, DWORD code)
    {
        return std::wstring(operation) + L" (kod błędu Windows: " + std::to_wstring(code) + L").";
    }

    struct PendingFile
    {
        std::filesystem::path path;
        HANDLE handle = INVALID_HANDLE_VALUE;
        bool committed = false;
        ~PendingFile()
        {
            if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            if (!committed && !path.empty()) DeleteFileW(path.c_str());
        }
    };
}

namespace cad::win32
{
    std::optional<std::filesystem::path> askScenePath(HWND owner, bool save)
    {
        std::vector<wchar_t> name(32768, L'\0');
        if (save)
        {
            const std::wstring initial = L"scena.cadscene";
            std::copy(initial.begin(), initial.end(), name.begin());
        }
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof dialog;
        dialog.hwndOwner = owner;
        dialog.lpstrFilter = L"Scena edytora (*.cadscene)\0*.cadscene\0Wszystkie pliki (*.*)\0*.*\0\0";
        dialog.nFilterIndex = 1;
        dialog.lpstrFile = name.data();
        dialog.nMaxFile = static_cast<DWORD>(name.size());
        dialog.lpstrDefExt = L"cadscene";
        dialog.lpstrTitle = save ? L"Zapisz scenę jako" : L"Załaduj scenę";
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
            (save ? OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN : OFN_FILEMUSTEXIST);
        const BOOL selected = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
        if (selected) return std::filesystem::path(name.data());
        const DWORD code = CommDlgExtendedError();
        if (code)
        {
            const auto message = systemError(L"Nie można otworzyć okna wyboru pliku", code);
            MessageBoxW(owner, message.c_str(), L"Plik sceny", MB_OK | MB_ICONERROR);
        }
        return std::nullopt;
    }

    SceneFileResult loadSceneFile(const std::filesystem::path& path)
    {
        try
        {
            std::ifstream input(path, std::ios::binary);
            if (!input) return {std::nullopt, L"Nie można otworzyć wybranego pliku do odczytu."};
            auto result = io::readScene(input);
            return {std::move(result.document), wide(result.error)};
        }
        catch (const std::exception& exception) { return {std::nullopt, wide(exception.what())}; }
    }

    bool saveSceneFile(const std::filesystem::path& path, const Document& document, std::wstring& error)
    {
        error.clear();
        try
        {
            std::ostringstream output;
            std::string codecError;
            if (!io::writeScene(output, document, codecError))
            {
                error = wide(codecError);
                return false;
            }
            const auto text = output.str();
            static std::atomic<unsigned long> serial{0};
            PendingFile temporary;
            for (unsigned attempt = 0; attempt < 16; ++attempt)
            {
                auto candidate = path;
                candidate += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
                temporary.handle = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (temporary.handle != INVALID_HANDLE_VALUE)
                {
                    temporary.path = std::move(candidate);
                    break;
                }
                const DWORD code = GetLastError();
                if (code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS)
                {
                    error = systemError(L"Nie można utworzyć pliku do zapisu", code);
                    return false;
                }
            }
            if (temporary.handle == INVALID_HANDLE_VALUE)
            {
                error = L"Nie można utworzyć unikalnego pliku tymczasowego.";
                return false;
            }
            DWORD written = 0;
            if (!WriteFile(temporary.handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) ||
                written != text.size() || !FlushFileBuffers(temporary.handle))
            {
                error = systemError(L"Nie udało się zapisać całej sceny. Poprzedni plik zachowano", GetLastError());
                return false;
            }
            const BOOL closed = CloseHandle(temporary.handle);
            temporary.handle = INVALID_HANDLE_VALUE;
            if (!closed || !MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                error = systemError(L"Nie można zastąpić docelowego pliku. Poprzedni plik zachowano", GetLastError());
                return false;
            }
            temporary.committed = true;
            return true;
        }
        catch (const std::exception& exception) { error = wide(exception.what()); return false; }
    }
}
