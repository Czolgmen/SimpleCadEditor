#include <cad_core/geometry/Bezier.h>
#include <stdexcept>
#include <array>
#include <numeric>

namespace
{
    cad::Vec2 mid(const cad::Vec2& p0, const cad::Vec2& p1)
    {
        return {std::midpoint(p0.x, p1.x), std::midpoint(p0.y, p1.y)};
    }
}

namespace cad
{
    Vec2 bezierPoint(
        const Vec2& p0, const Vec2& p1,
        const Vec2& p2, const Vec2& p3,
        double t)
    {
        // Zachowujemy dokładne końce mimo błędów zaokrągleń.
        if (t == 0.0) return p0;
        if (t == 1.0) return p3;

        const Vec2 a = p3 - p0 + (p1 - p2) * 3.0;
        const Vec2 b = (p0 - p1 * 2.0 + p2) * 3.0;
        const Vec2 c = (p1 - p0) * 3.0;
        const std::array coefficients{a, b, c, p0}; // d = p0

        Vec2 result = coefficients[0];

        for (std::size_t i = 1; i < coefficients.size(); ++i)
        {
            result = result * t + coefficients[i];
        }

        return result;
    }

    BezierSplit splitBezierHalf(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3)
    {
        const Vec2 a = mid(p0,p1);
        const Vec2 b = mid(p1,p2);
        const Vec2 c = mid(p2,p3);
        
        const Vec2 d = mid(a,b);
        const Vec2 e = mid(b,c);
        
        const Vec2 m = mid(d,e);

        return {{p0,a,d,m}, {m,e,c,p3}};
    }

    std::vector<Vec2> sampleBezier(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3, std::size_t steps)
    {
        if (steps == 0)
            throw std::invalid_argument("steps must be greater than zero");

        const Vec2 a = p3 - p0 + (p1 - p2) * 3.0;
        const Vec2 b = (p0 - p1 * 2.0 + p2) * 3.0;
        const Vec2 c = (p1 - p0)  * 3.0;
        
        const double h = 1.0 / static_cast<double>(steps);
        const double h2 = h * h;
        const double h3 = h2 * h;

        Vec2 point = p0;
        Vec2 delta1 = a * h3 + b * h2 + c * h;
        Vec2 delta2 = a * (6.0 * h3) + b * (2.0 * h2);
        const Vec2 delta3 = a * (6.0 * h3);

        std::vector<Vec2> points;
        points.reserve(steps + 1);
        points.push_back(point);

        for (std::size_t i = 0; i < steps; i++)
        {
            point = point + delta1;
            delta1 = delta1 + delta2;
            delta2 = delta2 + delta3;

            points.push_back(point);
        }

        points.back() = p3;
        return points;
    }        
}
