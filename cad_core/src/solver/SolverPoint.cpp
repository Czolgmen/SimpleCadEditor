#include <cad_core/solver/SolverPoint.h>

#include <cad_core/model/Document.h>

#include <cmath>

namespace cad::solver::detail
{
    std::optional<Vec2> getPointPosition(const Document &document, const SolverPoint &point)
    {
        Vec2 position;
        if (const auto* vertexId = std::get_if<VertexId>(&point))
        {
            const auto* vertex = document.findVertex(*vertexId);
            if (!vertex)
                return std::nullopt;

            position = vertex->position;
        }
        else if (const auto* controlPoint = std::get_if<ControlPointRef>(&point))
        {
            const auto* edge = document.findEdge(controlPoint->edge);
            if (!edge)
                return std::nullopt;

            const auto* bezier = std::get_if<CubicBezier>(&edge->geometry);
            if (!bezier)
                return std::nullopt;

            switch (controlPoint->point)
            {
            case BezierControlPoint::First:
                position = bezier->control1;
                break;
            case BezierControlPoint::Second:
                position = bezier->control2;
                break;
            default:
                return std::nullopt;
            }
        }
        else
        {
            return std::nullopt;
        }

        if (!std::isfinite(position.x) || !std::isfinite(position.y))
            return std::nullopt;

        return position;
    }

    bool setPointPosition(Document &document, const SolverPoint &point, const Vec2 &position)
    {
        if (const auto* vertexId = std::get_if<VertexId>(&point))
            return document.moveVertex(*vertexId, position);

        if (const auto* control = std::get_if<ControlPointRef>(&point))
            return document.moveControlPoint(*control, position);

        return false;
    }

} // namespace cad::solver::detail
