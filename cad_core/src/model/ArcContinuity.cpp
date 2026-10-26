#include <cad_core/model/ArcContinuity.h>
#include <cad_core/geometry/Arc.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeTangents.h>

#include <cmath>
#include <set>
#include <utility>
#include <vector>

namespace
{
    bool finitePosition(const cad::Vec2& position)
    {
        return std::isfinite(position.x) && std::isfinite(position.y);
    }

    bool allowedContinuity(cad::Continuity continuity)
    {
        return continuity == cad::Continuity::G0 || continuity == cad::Continuity::G1;
    }
}

namespace cad
{
    bool updateArcBulges(Document &document)
    {
        struct Update
        {
            EdgeId edge;
            double bulge;
        };
        std::vector<Update> updates;
        std::set<EdgeId> seenEdges;

        for (const auto& [polygonId, polygon] : document.getPolygons())
        {
            const std::size_t count = polygon.vertices.size();
            if (count < 3 || polygon.edges.size() != count)
                return false;

            for (std::size_t i = 0; i < count; ++i)
            {
                const EdgeId edgeId = polygon.edges[i];
                const auto* edge = document.findEdge(edgeId);
                if (!edge || !seenEdges.insert(edgeId).second ||
                    edge->start != polygon.vertices[i] ||
                    edge->end != polygon.vertices[(i + 1) % count])
                    return false;

                const auto* start = document.findVertex(edge->start);
                const auto* end = document.findVertex(edge->end);
                if (!start || !end || !finitePosition(start->position) || !finitePosition(end->position))
                    return false;

                const auto* arc = std::get_if<CircularArc>(&edge->geometry);
                if (!arc)
                    continue;

                const Continuity startContinuity = start->continuity.value;
                const Continuity endContinuity = end->continuity.value;
                if (!allowedContinuity(startContinuity) || !allowedContinuity(endContinuity) ||
                    (startContinuity == Continuity::G1 && endContinuity == Continuity::G1) ||
                    !std::isfinite(arc->bulge) || arc->bulge == 0.0 || start->position == end->position)
                    return false;

                double bulge = arc->bulge;
                if (startContinuity == Continuity::G0 && endContinuity == Continuity::G0)
                {
                    bulge = std::copysign(1.0, arc->bulge);
                }
                else
                {
                    const bool atStart = startContinuity == Continuity::G1;
                    const std::size_t neighborIndex = atStart ? (i + count - 1) % count : (i + 1) % count;
                    const EdgeId neighborId = polygon.edges[neighborIndex];
                    const auto* neighbor = document.findEdge(neighborId);
                    if (!neighbor)
                        return false;

                    // A G1 pair of arcs has one reference shape. Both arcs can
                    // have no other G1 end, so this dependency cannot form a chain.
                    const bool keepReference = std::holds_alternative<CircularArc>(neighbor->geometry) &&
                        edgeId < neighborId;
                    if (!keepReference)
                    {
                        const auto* neighborStart = document.findVertex(neighbor->start);
                        const auto* neighborEnd = document.findVertex(neighbor->end);
                        if (!neighborStart || !neighborEnd ||
                            !finitePosition(neighborStart->position) || !finitePosition(neighborEnd->position))
                            return false;

                        const auto tangent = atStart
                            ? edgeEndTangent(neighbor->geometry, neighborStart->position, neighborEnd->position)
                            : edgeStartTangent(neighbor->geometry, neighborStart->position, neighborEnd->position);
                        if (!tangent)
                            return false;

                        const auto fitted = atStart
                            ? arcBulgeFromStartTangent(start->position, end->position, *tangent)
                            : arcBulgeFromEndTangent(start->position, end->position, *tangent);
                        if (!fitted)
                            return false;
                        bulge = *fitted;
                    }

                    const auto fittedTangent = atStart
                        ? arcStartTangent(start->position, end->position, bulge)
                        : arcEndTangent(start->position, end->position, bulge);
                    if (!fittedTangent)
                        return false;
                }

                if (!computeArcParameters(start->position, end->position, bulge))
                    return false;
                if (bulge != arc->bulge)
                    updates.push_back({edgeId, bulge});
            }
        }

        if (updates.empty())
            return true;

        // Every fit reads the original geometry. Commit all changes together,
        // preserving the input even if a later setter rejects its candidate.
        Document candidate = document;
        for (const auto& update : updates)
            if (!candidate.setArcBulge(update.edge, update.bulge))
                return false;
        document = std::move(candidate);
        return true;
    }

} // namespace cad
