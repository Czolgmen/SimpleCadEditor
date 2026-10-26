#include <cad_core/geometry/Arc.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numeric>

namespace
{
    // Positive scaling preserves direction without squaring extreme coordinates.
    std::optional<cad::Vec2> scaledDirection(const cad::Vec2& vector)
    {
        if (!std::isfinite(vector.x) || !std::isfinite(vector.y))
            return std::nullopt;

        const double scale = (std::max)(std::abs(vector.x), std::abs(vector.y));
        if (scale == 0.0)
            return std::nullopt;
        return cad::Vec2{vector.x / scale, vector.y / scale};
    }

    std::optional<double> bulgeFromTangent(
        const cad::Vec2& start, const cad::Vec2& end,
        const cad::Vec2& tangent, bool atStart)
    {
        if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
            !std::isfinite(end.x) || !std::isfinite(end.y) || start == end)
            return std::nullopt;

        cad::Vec2 chord = end - start;
        if (!std::isfinite(chord.x) || !std::isfinite(chord.y))
            chord = end * 0.5 - start * 0.5;

        const auto chordDirection = scaledDirection(chord);
        const auto tangentDirection = scaledDirection(tangent);
        if (!chordDirection || !tangentDirection)
            return std::nullopt;

        const double sine = atStart
            ? cad::cross(*tangentDirection, *chordDirection)
            : cad::cross(*chordDirection, *tangentDirection);
        const double cosine = cad::dot(*tangentDirection, *chordDirection);

        // Parallel directions require a straight segment or a full turn, neither
        // of which is a finite circle arc through two distinct endpoints.
        if (sine == 0.0)
            return std::nullopt;

        const double alpha = std::atan2(sine, cosine);
        // tan(alpha/2) = (hypot(sine, cosine) - cosine) / sine.
        // Use this identity for major arcs: atan2 can round to +/-pi near a full turn.
        const double bulge = cosine < 0.0
            ? (std::hypot(sine, cosine) - cosine) / sine
            : std::tan(alpha / 2.0);

        if (!std::isfinite(bulge) || bulge == 0.0 ||
            !cad::computeArcParameters(start, end, bulge))
            return std::nullopt;

        return bulge;
    }
}

namespace cad
{
    std::vector<Vec2> sampleArc(const Vec2 &start, const Vec2 &end, double bulge, std::size_t steps)
    {
        if (steps == 0)
            throw std::invalid_argument("Steps must be greater than 0");

        const auto parameters = computeArcParameters(start, end, bulge);
        if (!parameters)
            throw std::invalid_argument("Invalid arc geometry");

        std::vector<Vec2> points;
        if (steps >= points.max_size())
            throw std::length_error("Too many arc samples");
        points.reserve(steps + 1);

        for (std::size_t i = 0; i <= steps; i++)
        {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            const double angle = parameters->startAngle + parameters->sweep * t;

            const Vec2 point{
                parameters->center.x + parameters->radius * std::cos(angle),
                parameters->center.y + parameters->radius * std::sin(angle)
            };

            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                throw std::invalid_argument("Non-finite arc point");

            points.push_back(point);
        }

        points.front() = start;
        points.back() = end;

        return points;
    }

    std::optional<ArcParameters> computeArcParameters(const Vec2 &start, const Vec2 &end, double bulge)
    {
        if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
            !std::isfinite(end.x) || !std::isfinite(end.y) || start == end)
        {
            return std::nullopt;
        }

        if (!std::isfinite(bulge) || bulge == 0.0)
            return std::nullopt;

        const Vec2 middle{
            std::midpoint(start.x, end.x),
            std::midpoint(start.y, end.y)
        };

        // Form the half-chord without subtracting two extreme endpoints directly.
        const Vec2 halfChord = middle - start;
        const double factor = 0.5 / bulge - 0.5 * bulge;
        const Vec2 center = middle + Vec2{-halfChord.y, halfChord.x} * factor;
        if (!std::isfinite(center.x) || !std::isfinite(center.y))
            return std::nullopt;

        const Vec2 radial = start - center;
        const double radius = std::hypot(radial.x, radial.y);
        const double startAngle = std::atan2(radial.y, radial.x);

        if (!std::isfinite(radius) || radius <= 0.0)
            return std::nullopt;

        const double sweep = 4.0 * std::atan(bulge);

        return ArcParameters{center, radius, startAngle, sweep};
    }

    std::optional<Vec2> arcStartTangent(const Vec2 &start, const Vec2 &end, double bulge)
    {
        const auto parameters = computeArcParameters(start, end, bulge);
        if (!parameters)
            return std::nullopt;

        const Vec2 radial = start - parameters->center;

        const Vec2 tangent = Vec2{-radial.y, radial.x} * parameters->sweep;

        if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || (tangent.x == 0.0 && tangent.y == 0))
            return std::nullopt;

        return tangent;
    }

    std::optional<Vec2> arcEndTangent(const Vec2 &start, const Vec2 &end, double bulge)
    {
        const auto parameters = computeArcParameters(start, end, bulge);
        if (!parameters)
            return std::nullopt;

        const Vec2 radial = end - parameters->center;

        const Vec2 tangent = Vec2{-radial.y, radial.x} * parameters->sweep;

        if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || (tangent.x == 0.0 && tangent.y == 0))
            return std::nullopt;

        return tangent;
    }

    std::optional<double> arcBulgeFromStartTangent(const Vec2 &start, const Vec2 &end, const Vec2 &tangent)
    {
        return bulgeFromTangent(start, end, tangent, true);
    }

    std::optional<double> arcBulgeFromEndTangent(const Vec2 &start, const Vec2 &end, const Vec2 &tangent)
    {
        return bulgeFromTangent(start, end, tangent, false);
    }

} // namespace cad
