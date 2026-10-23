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

    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect)
    {
        for (const auto& [id, polygon] : document.getPolygons())
        {
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

            for (const auto vertexId : polygon.vertices)
            {
                const auto* vertex = document.findVertex(vertexId);
                if (!vertex)
                    continue;

                const POINT p = worldToScreen(vertex->position, clientRect);
                Ellipse(dc, p.x - 3, p.y - 3, p.x + 3, p.y + 3);

            }
            
        }

        // TODO: Convert world coordinates to screen coordinates.
        // TODO: Draw document edges and vertex markers with GDI.
    }
}
