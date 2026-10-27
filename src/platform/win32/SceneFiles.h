#pragma once

#include <windows.h>
#include <cad_core/model/Document.h>
#include <filesystem>
#include <optional>
#include <string>

namespace cad::win32
{
    struct SceneFileResult
    {
        std::optional<Document> document;
        std::wstring error;
    };

    // Cancellation returns nullopt. Dialog errors are reported separately to the user.
    std::optional<std::filesystem::path> askScenePath(HWND owner, bool save);
    SceneFileResult loadSceneFile(const std::filesystem::path& path);
    // Writes a sibling temporary file, flushes it, then replaces the destination.
    bool saveSceneFile(const std::filesystem::path& path, const Document& document, std::wstring& error);
}
