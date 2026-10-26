#pragma once

#include <cad_core/geometry/Vec2.h>
#include <optional>

namespace cad::render
{
    struct ClippedSegment
    {
        Vec2 start;
        Vec2 end;
    };

    std::optional<ClippedSegment> clipLineToViewport(
        Vec2 start,
        Vec2 end,
        int width,
        int height);
}