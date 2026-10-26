#pragma once
#include <editor/EditorSession.h>
#include <platform/win32/BackBuffer.h>
#include <render/LineAlgorithm.h>
#include <optional>


namespace cad::win32
{
    struct AppState
    {
        cad::editor::EditorSession editor;
        BackBuffer backBuffer{};
        bool showConsistencyWarning = false;
        std::optional<cad::Vec2> pendingPointerPosition;
        bool pointerTimerActive = false;
        cad::render::LineAlgorithm lineAlgorithm = cad::render::LineAlgorithm::Gdi;
        HWND helpWindow = nullptr;
        HWND documentationWindow = nullptr;
    };
    

}
