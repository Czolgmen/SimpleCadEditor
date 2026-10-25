#pragma once

#include <cad_core/model/EdgeGeometry.h>

namespace cad
{
    Vec2 edgeStartTangent(
        const EdgeGeometry& geometry,
        const Vec2& start,
        const Vec2& end);

    Vec2 edgeEndTangent(
        const EdgeGeometry& geometry,
        const Vec2& start,
        const Vec2& end);
}