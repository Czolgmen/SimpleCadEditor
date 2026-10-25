#include <cad_core/geometry/Arc.h>
#include <cmath>
#include <stdexcept>
#include <numeric>


namespace cad
{
    std::vector<Vec2> sampleArc(const Vec2 &start, const Vec2 &end, double bulge, std::size_t steps)
    {
        if (steps <= 0 || !std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(end.x) || !std::isfinite(end.y) || start == end)
        {
            throw std::invalid_argument("Invalid arguments");
        }

        if (!std::isfinite(bulge) || bulge == 0.0)
            throw std::invalid_argument("Invalid bulge");

        const Vec2 middle{
            std::midpoint(start.x, end.x),
            std::midpoint(start.y, end.y)
        };

        // Form the half-chord without subtracting two extreme endpoints directly.
        const Vec2 halfChord = middle - start;
        const double factor = 0.5 / bulge - 0.5 * bulge;
        const Vec2 center = middle + Vec2{-halfChord.y, halfChord.x} * factor;
        if (!std::isfinite(center.x) || !std::isfinite(center.y))
            throw std::invalid_argument("Invalid arc center");

        const Vec2 radial = start - center;
        const double radius = std::hypot(radial.x, radial.y);
        const double startAngle = std::atan2(radial.y, radial.x);

        if (!std::isfinite(radius) || radius <= 0.0)
            throw std::invalid_argument("Invalid radius");

        const double sweep = 4.0 * std::atan(bulge);
        
        std::vector<Vec2> points;
        if (steps >= points.max_size())
            throw std::length_error("Too many arc samples");
        points.reserve(steps + 1);

        for (std::size_t i = 0; i <= steps; i++)
        {
            const double t = static_cast<double>(i) / steps;
            const double angle = startAngle + sweep * t;

            const Vec2 point{
                center.x + radius * std::cos(angle),
                center.y + radius * std::sin(angle)
            };

            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                throw std::invalid_argument("Non-finite arc point");

            points.push_back(point);
        }

        points.front() = start;
        points.back() = end;
        
        return points;
    }

} // namespace cad
