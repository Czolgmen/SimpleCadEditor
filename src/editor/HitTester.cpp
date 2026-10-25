#include <editor/HitTester.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeSampling.h>
#include <cad_core/model/EdgeGeometry.h>

#include <algorithm>
#include <cmath>

namespace cad::editor
{
    std::optional<VertexId> hitTestVertex(
        const Document& document,
        const Vec2& worldPosition,
        double tolerance)
    {
        if (tolerance < 0)
            return std::nullopt;


        std::optional<VertexId> nearest;
        double bestDistanceSquared = tolerance * tolerance;

        for (const auto& [polygonId, polygon] : document.getPolygons())
        {
            for (const auto& vertexId : polygon.vertices)
            {
                const auto* vertex = document.findVertex(vertexId);

                if (!vertex) continue;

                const double dx = vertex->position.x - worldPosition.x;
                const double dy = vertex->position.y - worldPosition.y;
                const double distanceSquared = dx * dx + dy * dy;

                if (distanceSquared <= bestDistanceSquared &&
                   (!nearest || distanceSquared < bestDistanceSquared))
                {
                    nearest = vertexId;
                    bestDistanceSquared = distanceSquared;
                }
            }
        }

        return nearest;
    }

    bool pointInPolygon(
        const Document& document,
        const Polygon& polygon,
        const Vec2& point
    )
    {
        bool inside = false;

        for (const auto& edgeId: polygon.edges)
        {
            const auto* edge = document.findEdge(edgeId);
            if (!edge)
                return false;

            const auto* start = document.findVertex(edge->start);
            const auto* end = document.findVertex(edge->end);
            if (!start || !end)
                return false;

            const auto points = sampleEdge(
                edge->geometry,
                start->position,
                end->position);

            for (std::size_t i = 1; i < points.size(); ++i)
            {
                const Vec2& a = points[i - 1];
                const Vec2& b = points[i];
            
                if ((a.y > point.y) != (b.y > point.y))
                {
                    const double crossingX =
                        a.x + (point.y - a.y) *
                        (b.x - a.x) / (b.y - a.y);
                
                    if (crossingX > point.x)
                        inside = !inside;
                }
            }

        }

        return inside;
    }

    std::optional<PolygonId> hitTestPolygon(const Document &document, const Vec2 &worldPosition)
    {
        const auto& polygons = document.getPolygons();
        for (auto it = polygons.rbegin(); it != polygons.rend(); ++it)
        {
            if (pointInPolygon(document, it->second, worldPosition))
            {
                return it->first;
            }
        }

        return std::nullopt;
    }

    double pointSegmentDistanceSquared(
        const Vec2& point,
        const Vec2& start,
        const Vec2& end)
    {
        const Vec2 direction = end - start;
        const Vec2 offset = point - start;
        const double lengthSquared = dot(direction, direction);

        if (lengthSquared == 0.0)
            return dot(offset, offset);

        const double t = std::clamp(
            dot(offset, direction) / lengthSquared, 0.0, 1.0);

        const Vec2 closest = start + direction * t;
        const Vec2 difference = point - closest;
        return dot(difference, difference);
    }

    std::optional<EdgeId> hitTestEdge(const Document &document, const Vec2 &worldPosition, double tolerance)
    {
        if (tolerance < 0)
            return std::nullopt;
    
        std::optional<EdgeId> nearest;
        double bestDistanceSquared = tolerance * tolerance;

        for (const auto& [polygonId, polygon] : document.getPolygons())
        {
            for (const auto& edgeId : polygon.edges)
            {
                const auto* edge = document.findEdge(edgeId);

                if (!edge)
                    continue;

                const auto* start = document.findVertex(edge->start);
                const auto* end = document.findVertex(edge->end);

                if (!start || !end)
                    continue;

                const auto points = sampleEdge(
                    edge->geometry,
                    start->position,
                    end->position);

                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    const double distanceSquared = pointSegmentDistanceSquared(
                        worldPosition,
                        points[i - 1],
                        points[i]);
                    
                    if (distanceSquared <= tolerance * tolerance &&
                        (!nearest || distanceSquared < bestDistanceSquared))
                    {
                        nearest = edgeId;
                        bestDistanceSquared = distanceSquared;
                    }
                }
            }
        }

        return nearest;
    }

    std::optional<ControlPointRef> hitTestControlPoint(const Document &document, const Vec2 &worldPosition, double tolerance)
    {
        if (!std::isfinite(tolerance) || tolerance < 0)
            return std::nullopt;

        std::optional<ControlPointRef> nearest = std::nullopt;
        double BestDistanceSquared = tolerance * tolerance;

        for (const auto& [polygonId, polygon] : document.getPolygons())
        {
            for(const auto& edgeId : polygon.edges)
            {
                const auto* edge = document.findEdge(edgeId);
                if (!edge)
                    continue;

                const auto* start = document.findVertex(edge->start);
                const auto* end = document.findVertex(edge->end);
                
                if (!start || !end)
                    continue;

                if (std::holds_alternative<CubicBezier>(edge->geometry))
                {
                    const CubicBezier& geometry = std::get<CubicBezier>(edge->geometry);

                    const auto& control1 = geometry.control1;
                    const auto& control2 = geometry.control2;

                    const Vec2 difference1 = control1 - worldPosition;
                    const Vec2 difference2 = control2 - worldPosition;

                    const double dist1Squared = dot(difference1, difference1);
                    const double dist2Squared = dot(difference2, difference2);
                    if (dist1Squared < BestDistanceSquared || (!nearest && dist1Squared <= BestDistanceSquared))
                    {
                        nearest = ControlPointRef{
                            edgeId,
                            BezierControlPoint::First
                        };
                        BestDistanceSquared = dist1Squared;
                    }

                    if (dist2Squared < BestDistanceSquared || (!nearest && dist2Squared <= BestDistanceSquared))
                    {
                        nearest = ControlPointRef{
                            edgeId,
                            BezierControlPoint::Second
                        };
                        BestDistanceSquared = dist2Squared;
                    }


                }

            }


        }

        return nearest;

    }
}
