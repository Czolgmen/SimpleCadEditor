#pragma once

#include <cad_core/model/EdgeGeometry.h>
#include <optional>

namespace cad
{
    // Full endpoint derivatives. Non-finite results and invalid arcs return nullopt.
    // Zero derivatives remain valid for segments and Beziers (C1 permits them).
    std::optional<Vec2> edgeStartTangent(
        const EdgeGeometry& geometry,
        const Vec2& start,
        const Vec2& end);

    std::optional<Vec2> edgeEndTangent(
        const EdgeGeometry& geometry,
        const Vec2& start,
        const Vec2& end);
}
