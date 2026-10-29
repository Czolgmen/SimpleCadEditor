#include <platform/win32/GdiRenderer.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeSampling.h>
#include <platform/win32/DibRenderer.h>
#include <render/LineClipping.h>

#include <variant>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <locale>
#include <numeric>
#include <sstream>
#include <string>
#include <limits>
#include <map>
#include <algorithm>

namespace cad::win32
{
    namespace
    {
        Vec2 worldToScreenDouble(const Vec2& position, const RECT& rect)
        {
            return {static_cast<double>(rect.left) + 20.0 + position.x,
                static_cast<double>(rect.bottom) - 20.0 - position.y};
        }

        std::optional<POINT> worldToScreen(const Vec2& position, const RECT& rect)
        {
            const Vec2 point = worldToScreenDouble(position, rect);
            // Offscreen decorations need no integer conversion. Leave room for
            // marker radii and label offsets in subsequent GDI calls.
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                point.x < static_cast<double>(rect.left) - 32 ||
                point.x > static_cast<double>(rect.right) + 32 ||
                point.y < static_cast<double>(rect.top) - 32 ||
                point.y > static_cast<double>(rect.bottom) + 32 ||
                point.x < static_cast<double>((std::numeric_limits<LONG>::min)()) + 32 ||
                point.x > static_cast<double>((std::numeric_limits<LONG>::max)()) - 32 ||
                point.y < static_cast<double>((std::numeric_limits<LONG>::min)()) + 32 ||
                point.y > static_cast<double>((std::numeric_limits<LONG>::max)()) - 32)
                return std::nullopt;
            return POINT{std::lround(point.x), std::lround(point.y)};
        }

        std::optional<render::ClippedSegment> clipScreenSegment(Vec2 start, Vec2 end, const RECT& rect)
        {
            const Vec2 origin{static_cast<double>(rect.left), static_cast<double>(rect.top)};
            auto clipped = render::clipLineToViewport(start - origin, end - origin,
                rect.right - rect.left, rect.bottom - rect.top);
            if (clipped)
            {
                clipped->start = clipped->start + origin;
                clipped->end = clipped->end + origin;
            }
            return clipped;
        }

        void drawGdiSegment(HDC dc, const render::ClippedSegment& segment)
        {
            MoveToEx(dc, std::lround(segment.start.x), std::lround(segment.start.y), nullptr);
            LineTo(dc, std::lround(segment.end.x), std::lround(segment.end.y));
        }
    }

    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect,
        std::optional<cad::VertexId> selectedVertex, std::optional<cad::PolygonId> selectedPolygon,
        std::optional<cad::EdgeId> selectedEdge, std::optional<cad::EdgeId> pendingParallelEdge,
        std::optional<cad::ControlPointRef> selectedControlPoint,
        cad::render::LineAlgorithm algorithm, BackBuffer* buffer)
    {
        if (!dc || clientRect.right <= clientRect.left || clientRect.bottom <= clientRect.top)
            return;
        const int savedDc = SaveDC(dc);
        if (!savedDc)
            return;
        IntersectClipRect(dc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);

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
            RestoreDC(dc, savedDc);
            return;
        }

        HGDIOBJ previousPen = SelectObject(dc, normalPen);

        // Flush the GDI background once before the CPU batch. All decorations
        // are issued after that batch, so no GDI writes race with CPU pixels.
        const bool useRaster = (algorithm == render::LineAlgorithm::Bresenham || algorithm == render::LineAlgorithm::Wu) &&
            buffer && buffer->dc() == dc && buffer->pixels() && GdiFlush();
        std::vector<render::PixelPoint> scratch;
        WuScratch wuScratch;
        std::vector<Vec2> screenPoints;
        std::map<std::pair<int, COLORREF>, HPEN> edgePens;

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

                const bool highlighted =
                    selectedPolygon == id || selectedEdge == IdEdge;
                const bool pending = pendingParallelEdge == IdEdge;
                const int strokeWidth = std::clamp(ed->strokeWidth, 1, 32) + (pending || highlighted ? 2 : 0);
                if (!useRaster)
                {
                    const COLORREF color = pending ? RGB(220,120,0) : highlighted ? RGB(0,120,215) : RGB(0,0,0);
                    const auto key = std::pair{strokeWidth, color};
                    auto [pen, inserted] = edgePens.try_emplace(key, nullptr);
                    if (inserted) pen->second = CreatePen(PS_SOLID, strokeWidth, color);
                    if (!pen->second) continue;
                    SelectObject(dc, pen->second);
                }

                const auto points = sampleEdge(
                    ed->geometry,
                    start->position,
                    end->position);

                if (useRaster && algorithm == render::LineAlgorithm::Wu)
                {
                    screenPoints.clear();
                    screenPoints.reserve(points.size());
                    for (const auto& point : points) screenPoints.push_back(worldToScreenDouble(point, clientRect));
                    drawWuPolyline(*buffer, screenPoints,
                        pending ? 0x00DC7800 : highlighted ? 0x000078D7 : 0x00000000,
                        wuScratch, strokeWidth, &clientRect);
                    continue;
                }

                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    const Vec2 screenStart = worldToScreenDouble(points[i - 1], clientRect);
                    const Vec2 screenEnd = worldToScreenDouble(points[i], clientRect);
                    if (useRaster)
                        drawBresenhamLine(*buffer, screenStart, screenEnd,
                            pending ? 0x00DC7800 : highlighted ? 0x000078D7 : 0x00000000,
                            scratch, strokeWidth, &clientRect);
                    else if (const auto segment = clipScreenSegment(screenStart, screenEnd, clientRect))
                        drawGdiSegment(dc, *segment);
                }
            }
        }

        // GDI overlays are drawn only after every geometry edge is complete.
        for (const auto& [id, polygon] : document.getPolygons())
        {
            (void)id;
            for (const auto IdEdge : polygon.edges)
            {
                const auto* ed = document.findEdge(IdEdge);
                if (!ed) continue;
                if (const auto* bezier = std::get_if<CubicBezier>(&ed->geometry))
                {
                    const auto* start = document.findVertex(ed->start);
                    const auto* end = document.findVertex(ed->end);
                    if (!start || !end) continue;
                    const Vec2 controlPolygon[]{start->position, bezier->control1, bezier->control2, end->position};

                    const HGDIOBJ curvePen = SelectObject(dc, controlPen);
                    const int previousBackground = SetBkMode(dc, TRANSPARENT);

                    for (std::size_t i = 1; i < std::size(controlPolygon); ++i)
                        if (const auto segment = clipScreenSegment(
                            worldToScreenDouble(controlPolygon[i - 1], clientRect),
                            worldToScreenDouble(controlPolygon[i], clientRect), clientRect))
                            drawGdiSegment(dc, *segment);

                    SetBkMode(dc, previousBackground);
                    SelectObject(dc, normalPen);

                    constexpr int radius = 5;

                    for (const auto which : {
                        BezierControlPoint::First,
                        BezierControlPoint::Second})
                    {
                        const auto control = worldToScreen(which == BezierControlPoint::First
                            ? bezier->control1 : bezier->control2, clientRect);
                        if (!control) continue;
                    
                        const bool selected =
                            selectedControlPoint == ControlPointRef{IdEdge, which};
                    
                        SelectObject(dc, selected ? selectedPen : normalPen);
                    
                        Rectangle(dc,
                            control->x - radius, control->y - radius,
                            control->x + radius, control->y + radius);
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

                const auto p = worldToScreen(vertex->position, clientRect);
                if (!p) continue;
                constexpr int radius = 5;
                SelectObject(dc, selectedVertex == vertexId ? selectedPen : normalPen);
                Ellipse(dc, p->x - radius, p->y - radius,
                            p->x + radius, p->y + radius);

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
            const auto middle = worldToScreen(
                Vec2{std::midpoint(start->position.x, end->position.x),
                    std::midpoint(start->position.y, end->position.y)}, clientRect);
            if (!middle) continue;
            std::wostringstream label;
            label.imbue(std::locale::classic());
            label << L"L=" << std::setprecision(6) << relation.length;
            const auto text = label.str();
            TextOutW(dc, middle->x + 8, middle->y + 8, text.c_str(), static_cast<int>(text.size()));
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
                const auto middle = worldToScreen(
                    {std::midpoint(start->position.x, end->position.x), std::midpoint(start->position.y, end->position.y)}, clientRect);
                if (!middle) continue;
                TextOutW(dc, middle->x + 8, middle->y - 20, text.c_str(), static_cast<int>(text.size()));
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
        RestoreDC(dc, savedDc);

        for (const auto& [key, pen] : edgePens) if (pen) DeleteObject(pen);
        DeleteObject(selectedPen);
        DeleteObject(pendingPen);
        DeleteObject(normalPen);
        DeleteObject(controlPen);
    }
}
