#pragma once
#include <editor/EditorSession.h>
#include <platform/win32/BackBuffer.h>
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
    };
    

}
