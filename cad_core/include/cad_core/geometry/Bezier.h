#pragma once

#include <cad_core/geometry/Vec2.h>
#include <array>
#include <cstddef>
#include <vector>

namespace cad
{
    Vec2 bezierPoint(
        const Vec2& p0, const Vec2& p1,
        const Vec2& p2, const Vec2& p3,
        double t
    );

    struct BezierSplit
    {
        std::array<Vec2,4> left;
        std::array<Vec2,4> right;
    };

    BezierSplit splitBezierHalf(
        const Vec2& p0, const Vec2& p1,
        const Vec2& p2, const Vec2& p3
    );

    std::vector<Vec2> sampleBezier (
        const Vec2& p0, const Vec2& p1,
        const Vec2& p2, const Vec2& p3,
        std::size_t steps
    );
} // namespace cad
