#pragma once

#include <cad_core/geometry/Vec2.h>
#include <variant>

namespace cad
{
    struct LineSegment {} ;

    struct CubicBezier {
        Vec2 control1;
        Vec2 control2;
    };

    using EdgeGeometry = std::variant<LineSegment, CubicBezier>;
} // namespace cad
