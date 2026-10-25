#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <optional>
#include <cad_core/model/Id.h>



namespace cad
{
    class Document;
}

namespace cad::win32
{
    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect,
        std::optional<cad::VertexId> selectedVertex, std::optional<cad::PolygonId> selectedPolygon,
        std::optional<cad::EdgeId> selectedEdge, std::optional<cad::EdgeId> pendingParallelEdge = {});
}
