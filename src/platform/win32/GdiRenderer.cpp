#include <platform/win32/GdiRenderer.h>

#include <cad_core/model/Document.h>

#include <cmath>

namespace cad::win32
{
    POINT worldToScreen(const cad::Vec2& position, const RECT& clientRect)
    {
        return {
            clientRect.left + 20 + std::lround(position.x),
            clientRect.bottom - 20 - std::lround(position.y)
        };
    }

    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect, std::optional<cad::VertexId> selectedVertex, std::optional<cad::PolygonId> selectedPolygon)
    {
        HPEN normalPen = CreatePen(PS_SOLID, 2, RGB(0,0,0));
        HPEN selectedPen = CreatePen(PS_SOLID, 4, RGB(0,120,215));

        if (!normalPen || !selectedPen)
        {
            if (normalPen) DeleteObject(normalPen);
            if (selectedPen) DeleteObject(selectedPen);
            return;
        }

        HGDIOBJ previousPen = SelectObject(dc, normalPen);

        for (const auto& [id, polygon] : document.getPolygons())
        {
            SelectObject(
                dc,
                selectedPolygon == id ? selectedPen : normalPen);
            for (const auto IdEdge : polygon.edges)
            {
                const Edge* ed = document.findEdge(IdEdge);
                if (!ed)
                    continue;

                const auto* start = document.findVertex(ed->start);
                const auto* end = document.findVertex(ed->end);

                if (!start || !end)
                    continue;

                const POINT p1 = worldToScreen(start->position, clientRect);
                const POINT p2 = worldToScreen(end->position, clientRect);

                MoveToEx(dc, p1.x, p1.y, nullptr);
                LineTo(dc, p2.x, p2.y);
            }

            SelectObject(dc, normalPen);

            for (const auto vertexId : polygon.vertices)
            {
                const auto* vertex = document.findVertex(vertexId);
                if (!vertex)
                    continue;

                const POINT p = worldToScreen(vertex->position, clientRect);
                constexpr int radius = 5;
                SelectObject(dc, selectedVertex == vertexId ? selectedPen : normalPen);
                Ellipse(dc, p.x - radius, p.y - radius,
                            p.x + radius, p.y + radius);

            }
            
        }

        SelectObject(dc, previousPen);

        DeleteObject(selectedPen);
        DeleteObject(normalPen);
    }
}
