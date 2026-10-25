#include <cad_core/model/ContinuityJoins.h>

#include <cad_core/model/Document.h>

#include <variant>

namespace cad
{
    std::optional<std::vector<ContinuityJoin>> collectContinuityJoins(const Document &document)
    {
        std::vector<ContinuityJoin> joins;

        for (const auto& [polygonId, polygon] : document.getPolygons())
        {
            const std::size_t count = polygon.vertices.size();
            if (count < 3 || polygon.edges.size() != count)
                return std::nullopt;

            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t previous = (i + count - 1) % count;

                const VertexId vertexId = polygon.vertices[i];
                const EdgeId incomingId = polygon.edges[previous];
                const EdgeId outgoingId = polygon.edges[i];

                const Vertex* vertex = document.findVertex(vertexId);
                const Edge* incoming = document.findEdge(incomingId);
                const Edge* outgoing = document.findEdge(outgoingId);

                if (!vertex || !incoming || !outgoing)
                    return std::nullopt;

                if (incoming->end != vertexId || outgoing->start != vertexId)
                {   
                    return std::nullopt;
                }

                const Continuity continuity = vertex->continuity.value;
                if (continuity == Continuity::G0)
                    continue;

                const bool incomingBezier = std::holds_alternative<CubicBezier>(incoming->geometry);
                const bool outgoingBezier = std::holds_alternative<CubicBezier>(outgoing->geometry);
                
                if (!incomingBezier && !outgoingBezier)
                    continue;

                joins.push_back({
                    vertexId,
                    incomingId,
                    outgoingId,
                    continuity
                });

            }
        }

        return joins;
    }
} // namespace cad


