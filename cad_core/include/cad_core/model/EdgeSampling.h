#pragma once

#include <cad_core/model/EdgeGeometry.h>

#include <cstddef>
#include <vector>

namespace cad
{   
    std::vector<Vec2> sampleEdge(
        const EdgeGeometry& geometry,
        const Vec2& start,
        const Vec2& end,
        std::size_t steps = 100
    );
} // namespace cad
