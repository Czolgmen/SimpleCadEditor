#pragma once
#include <editor/EditorSession.h>
#include <platform/win32/BackBuffer.h>


namespace cad::win32
{
    struct AppState
    {
        cad::editor::EditorSession editor;
        BackBuffer backBuffer{};
    };
    

}
