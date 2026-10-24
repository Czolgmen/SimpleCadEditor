#include <platform/win32/GdiRenderer.h>

#include <cad_core/model/Document.h>

#include <cmath>
#include <iomanip>
#include <locale>
#include <numeric>
#include <sstream>

namespace cad::win32
{
    POINT worldToScreen(const cad::Vec2& position, const RECT& clientRect)
    {
        return {
            clientRect.left + 20 + std::lround(position.x),
            clientRect.bottom - 20 - std::lround(position.y)
        };
    }

    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect, std::optional<cad::VertexId> selectedVertex, std::optional<cad::PolygonId> selectedPolygon, std::optional<cad::EdgeId> selectedEdge)
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

                const bool highlighted =
                    selectedPolygon == id || selectedEdge == IdEdge;
                SelectObject(dc, highlighted ? selectedPen : normalPen);

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

        // The label remains visible after deselection and while dragging an endpoint.
        const int previousBackground = SetBkMode(dc, TRANSPARENT);
        const COLORREF previousText = SetTextColor(dc, RGB(0, 110, 55));
        const HGDIOBJ previousFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        for (const auto& relation : document.getFixedLengthConstraints())
        {
            const auto* edge = document.findEdge(relation.edge);
            if (!edge)
                continue;
            const auto* start = document.findVertex(edge->start);
            const auto* end = document.findVertex(edge->end);
            if (!start || !end)
                continue;
            const POINT middle = worldToScreen(
                Vec2{std::midpoint(start->position.x, end->position.x),
                    std::midpoint(start->position.y, end->position.y)}, clientRect);
            std::wostringstream label;
            label.imbue(std::locale::classic());
            label << L"L=" << std::setprecision(6) << relation.length;
            const auto text = label.str();
            TextOutW(dc, middle.x + 8, middle.y + 8, text.c_str(), static_cast<int>(text.size()));
        }
        SelectObject(dc, previousFont);
        SetTextColor(dc, previousText);
        SetBkMode(dc, previousBackground);
        SelectObject(dc, previousPen);

        DeleteObject(selectedPen);
        DeleteObject(normalPen);
    }
}
