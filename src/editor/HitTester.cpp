#include <editor/HitTester.h>

#include <cad_core/model/Document.h>

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

            if ((start->position.y > point.y) != (end->position.y > point.y))
            {
                const double crossingX = 
                    start->position.x + (point.y - start->position.y) * (end->position.x - start->position.x) / (end->position.y - start->position.y);

                if (crossingX > point.x)
                    inside = !inside;
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
}
