#include <editor/HitTester.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeSampling.h>

#include <algorithm>

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


}
