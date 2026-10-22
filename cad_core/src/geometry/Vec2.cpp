#include <cad_core/geometry/Vec2.h>
#include <math.h>


namespace cad
{
    Vec2::Vec2(/* args */)
    {
        x = 0;
        y = 0;
    }

    Vec2::Vec2(double _x, double _y)
    {
        x = _x;
        y = _y;
    }

    Vec2 Vec2::operator+(Vec2 other) const
    {
        return Vec2(x + other.x, y + other.y);
    }

    Vec2 Vec2::operator-(Vec2 other) const
    {
        return Vec2(x - other.x, y - other.y);
    }

    Vec2 Vec2::operator*(double k) const
    {
        return Vec2(x * k, y * k);
    }

    std::optional<Vec2> Vec2::operator/(double k) const
    {
        if(k == 0)
        {
            return std::nullopt;
        }
        return Vec2(x / k, y / k);
    }

    double Vec2::Length() const
    {
        return sqrt(x*x + y*y);
    }
    std::optional<Vec2> Vec2::Normalized() const
    {
        double length = Length();

        if (length <= 1e-10)
        {
            return std::nullopt;
        }

        return Vec2(x / length, y / length);

    }

    double dot(const Vec2 &a, const Vec2 &b)
    {
        return a.x * b.x + a.y * b.y;
    }

    double cross(const Vec2 &a, const Vec2 &b)
    {
        return a.x * b.y - a.y * b.x;
    }
}
