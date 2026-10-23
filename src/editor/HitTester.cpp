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
}
