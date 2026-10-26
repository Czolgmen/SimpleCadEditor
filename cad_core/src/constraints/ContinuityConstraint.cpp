#include <cad_core/constraints/ContinuityConstraint.h>

#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeTangents.h>

#include <cmath>

std::optional<std::vector<double>> cad::constraint::continuityErrors(Continuity continuity, const Vec2 &incomingTangent, const Vec2 &outgoingTangent)
{
    switch (continuity)
    {
    case Continuity::G0:
        return std::vector<double>();
    
    case Continuity::G1:
    {
        const double incomingLength = std::hypot(
            incomingTangent.x, incomingTangent.y);

        const double outgoingLength = std::hypot(
            outgoingTangent.x, outgoingTangent.y);
        
        if (!std::isfinite(incomingLength) || incomingLength == 0.0 ||
            !std::isfinite(outgoingLength) || outgoingLength == 0.0)
        {
            return std::nullopt;
        }

        const Vec2 incomingUnit{
            incomingTangent.x / incomingLength,
            incomingTangent.y / incomingLength};
        
        const Vec2 outgoingUnit{
            outgoingTangent.x / outgoingLength,
            outgoingTangent.y / outgoingLength};

        const double angle = std::atan2(
            cross(incomingUnit, outgoingUnit),
            dot(incomingUnit, outgoingUnit));

        if (!std::isfinite(angle))
            return std::nullopt;

        return std::vector<double>{angle};

    }        
    case Continuity::C1:
    {
        const Vec2 difference = outgoingTangent - incomingTangent;

        if (!std::isfinite(difference.x) ||
            !std::isfinite(difference.y))
        {
            return std::nullopt;
        }

        return std::vector<double>{difference.x, difference.y};
    }

    default:
        return std::nullopt;
    }
}

std::optional<std::vector<double>> cad::constraint::continuityErrors(const Document &document, const ContinuityJoin &join)
{
    const auto* incoming = document.findEdge(join.incoming);
    const auto* outgoing = document.findEdge(join.outgoing);

    if (!incoming || !outgoing || !(incoming->end == join.vertex && outgoing->start == join.vertex))
        return std::nullopt;

    const auto* incomingStart = document.findVertex(incoming->start);
    const auto* incomingEnd = document.findVertex(incoming->end);
    const auto* outgoingStart = document.findVertex(outgoing->start);
    const auto* outgoingEnd = document.findVertex(outgoing->end);

    if (!incomingStart || !incomingEnd || !outgoingStart || !outgoingEnd)
        return std::nullopt;

    for (const Vertex* vertex : {incomingStart, incomingEnd, outgoingStart, outgoingEnd})
    {
        if (!std::isfinite(vertex->position.x) || !std::isfinite(vertex->position.y))
            return std::nullopt;
    }

    // G0 has no tangent equation, even if a finite-position edge has an
    // unrepresentable derivative. References and positions were checked above.
    if (join.continuity == Continuity::G0)
        return std::vector<double>{};
    if (join.continuity == Continuity::C1 &&
        (std::holds_alternative<CircularArc>(incoming->geometry) ||
         std::holds_alternative<CircularArc>(outgoing->geometry)))
        return std::nullopt;

    const auto incomingTangent = edgeEndTangent(
        incoming->geometry,
        incomingStart->position,
        incomingEnd->position);

    const auto outgoingTangent = edgeStartTangent(
        outgoing->geometry,
        outgoingStart->position,
        outgoingEnd->position);

    if (!incomingTangent || !outgoingTangent)
        return std::nullopt;

    return continuityErrors(join.continuity, *incomingTangent, *outgoingTangent);
}
