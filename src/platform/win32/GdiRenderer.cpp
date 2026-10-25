#include <platform/win32/GdiRenderer.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeSampling.h>

#include <variant>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <locale>
#include <numeric>
#include <sstream>
#include <string>

namespace cad::win32
{
    POINT worldToScreen(const cad::Vec2& position, const RECT& clientRect)
    {
        return {
            clientRect.left + 20 + std::lround(position.x),
            clientRect.bottom - 20 - std::lround(position.y)
        };
    }

    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect,
        std::optional<cad::VertexId> selectedVertex, std::optional<cad::PolygonId> selectedPolygon,
        std::optional<cad::EdgeId> selectedEdge, std::optional<cad::EdgeId> pendingParallelEdge,
        std::optional<cad::ControlPointRef> selectedControlPoint)
    {
        HPEN normalPen = CreatePen(PS_SOLID, 2, RGB(0,0,0));
        HPEN selectedPen = CreatePen(PS_SOLID, 4, RGB(0,120,215));
        HPEN pendingPen = CreatePen(PS_SOLID, 4, RGB(220,120,0));
        HPEN controlPen = CreatePen(PS_DASH, 1, RGB(140, 140, 140));

        if (!normalPen || !selectedPen || !pendingPen || !controlPen)
        {
            if (normalPen) DeleteObject(normalPen);
            if (selectedPen) DeleteObject(selectedPen);
            if (pendingPen) DeleteObject(pendingPen);
            if (controlPen) DeleteObject(controlPen);
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
                SelectObject(dc, pendingParallelEdge == IdEdge ? pendingPen : (highlighted ? selectedPen : normalPen));

                const auto points = sampleEdge(
                    ed->geometry,
                    start->position,
                    end->position);

                MoveToEx(dc, p1.x, p1.y, nullptr);

                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    const POINT point = worldToScreen(points[i], clientRect);
                    LineTo(dc, point.x, point.y);
                }

                if (const auto* bezier = std::get_if<CubicBezier>(&ed->geometry))
                {

                    const POINT c1 = worldToScreen(bezier->control1, clientRect);
                    const POINT c2 = worldToScreen(bezier->control2, clientRect);

                    const POINT controlPolygon[]{p1, c1, c2, p2};

                    const HGDIOBJ curvePen = SelectObject(dc, controlPen);
                    const int previousBackground = SetBkMode(dc, TRANSPARENT);

                    Polyline(dc, controlPolygon, 4);

                    SetBkMode(dc, previousBackground);
                    SelectObject(dc, normalPen);

                    constexpr int radius = 5;

                    for (const auto which : {
                        BezierControlPoint::First,
                        BezierControlPoint::Second})
                    {
                        const POINT control =
                            which == BezierControlPoint::First ? c1 : c2;
                    
                        const bool selected =
                            selectedControlPoint == ControlPointRef{IdEdge, which};
                    
                        SelectObject(dc, selected ? selectedPen : normalPen);
                    
                        Rectangle(dc,
                            control.x - radius, control.y - radius,
                            control.x + radius, control.y + radius);
                    }

                    SelectObject(dc, curvePen);

                }

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
        SetTextColor(dc, RGB(130, 65, 0));
        for (const auto& [id, polygon] : document.getPolygons())
        {
            (void)id;
            for (const EdgeId edgeId : polygon.edges)
            {
                std::wstring text;
                std::size_t number = 0;
                for (const auto& pair : document.getParallelConstraints())
                {
                    ++number;
                    if (pair.first == edgeId || pair.second == edgeId)
                    {
                        if (!text.empty()) text += L", ";
                        text += L"P" + std::to_wstring(number);
                    }
                }
                if (text.empty())
                    continue;
                const auto* edge = document.findEdge(edgeId);
                if (!edge) continue;
                const auto* start = document.findVertex(edge->start);
                const auto* end = document.findVertex(edge->end);
                if (!start || !end) continue;
                const POINT middle = worldToScreen(
                    {std::midpoint(start->position.x, end->position.x), std::midpoint(start->position.y, end->position.y)}, clientRect);
                TextOutW(dc, middle.x + 8, middle.y - 20, text.c_str(), static_cast<int>(text.size()));
            }
        }
        if (pendingParallelEdge)
        {
            constexpr wchar_t hint[] = L"Równoległość: kliknij drugą krawędź. Esc anuluje wybór.";
            TextOutW(dc, clientRect.left + 10, clientRect.top + 10, hint, static_cast<int>(std::size(hint) - 1));
        }
        SelectObject(dc, previousFont);
        SetTextColor(dc, previousText);
        SetBkMode(dc, previousBackground);
        SelectObject(dc, previousPen);

        DeleteObject(selectedPen);
        DeleteObject(pendingPen);
        DeleteObject(normalPen);
        DeleteObject(controlPen);
    }
}
