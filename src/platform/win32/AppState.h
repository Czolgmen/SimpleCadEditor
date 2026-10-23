#pragma once
#include <cad_core/model/Document.h>
#include <platform/win32/BackBuffer.h>
#include <editor/Selection.h>
#include <editor/DragState.h>


namespace cad::win32
{
    struct AppState
    {
        cad::Document document;
        BackBuffer backBuffer{};
        cad::editor::Selection selection;
        cad::editor::DragState drag;
    };
    

}
