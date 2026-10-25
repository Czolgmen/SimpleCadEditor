#include <cad_core/constraints/ParallelConstraint.h>
#include <cad_core/model/Document.h>

#include <cmath>


namespace cad::constraint
{   
    std::optional<double> parallelError(const Document &document, const ParallelConstraint &constraint)
    {
        if (constraint.first == constraint.second)
            return std::nullopt;

        const auto* first = document.findEdge(constraint.first);
        const auto* second = document.findEdge(constraint.second);

        if (!first || !second || !std::holds_alternative<LineSegment>(first->geometry) || !std::holds_alternative<LineSegment>(second->geometry))
            return std::nullopt;


        const auto* firstStart = document.findVertex(first->start);
        const auto* firstEnd = document.findVertex(first->end);
        const auto* secondStart = document.findVertex(second->start);
        const auto* secondEnd = document.findVertex(second->end);

        if (!firstStart || !firstEnd || !secondStart || !secondEnd)
            return std::nullopt;

        if (!std::isfinite(firstStart->position.x) || !std::isfinite(firstStart->position.y) ||
            !std::isfinite(firstEnd->position.x) || !std::isfinite(firstEnd->position.y) ||
            !std::isfinite(secondStart->position.x) || !std::isfinite(secondStart->position.y) ||
            !std::isfinite(secondEnd->position.x) || !std::isfinite(secondEnd->position.y))
                return std::nullopt;

        const Vec2 d1 = firstEnd->position - firstStart->position;
        const Vec2 d2 = secondEnd->position - secondStart->position;

        const double length1 = std::hypot(d1.x, d1.y);
        const double length2 = std::hypot(d2.x, d2.y);
        if (!std::isfinite(length1) || !std::isfinite(length2) || length1 <= 0 || length2 <= 0)
            return std::nullopt;

        const Vec2 u1{d1.x / length1, d1.y / length1};
        const Vec2 u2{d2.x / length2, d2.y / length2};
        const double error = cross(u1, u2);
        if (!std::isfinite(error))
            return std::nullopt;

        return error;
    }
 
} // namespace cad::constraint

