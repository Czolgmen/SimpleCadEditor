#pragma once
#include <optional>

namespace cad
{
    class Vec2
    {
    private:
        
    public:
        double x;
        double y;
        Vec2(/* args */);
        Vec2(double x, double y);

        Vec2 operator+ (Vec2 other) const;
        Vec2 operator- (Vec2 other) const;
        Vec2 operator* (double k) const;
        std::optional<Vec2> operator/ (double k) const;

        bool operator== (const Vec2& other) const = default;

        double Length() const;
        std::optional<Vec2> Normalized() const;
    };

    double dot(const Vec2& a, const Vec2& b);
    double cross(const Vec2& a, const Vec2& b);


}
