#include <cad_core/constraints/FixedLengthConstraint.h>
#include <cad_core/model/Document.h>
#include <cmath>

namespace cad::constraint
{

    std::optional<double> fixedLengthError(const Document &document, const FixedLengthConstraint &constraint)
    {
        if (!std::isfinite(constraint.length) || constraint.length <= 0)
        {
            return std::nullopt;
        }

        const auto& edge = document.findEdge(constraint.edge);
        if (!edge || !std::holds_alternative<LineSegment>(edge->geometry))
        {
            return std::nullopt;
        }

        const auto& start = document.findVertex(edge->start);
        const auto& end = document.findVertex(edge->end);

        if (!start || !end || !std::isfinite(start->position.x ) || !std::isfinite(start->position.y )
            || !std::isfinite(end->position.x ) || !std::isfinite(end->position.y ))
        {
            return std::nullopt;
        }

        const double currentLength = std::hypot(
            end->position.x - start->position.x,
            end->position.y - start->position.y);

        const double error = currentLength - constraint.length;

        if (!std::isfinite(error))
        {
            return std::nullopt;
        }

        return error;
    }  
} // namespace cad::constraint


