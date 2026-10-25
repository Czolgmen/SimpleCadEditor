#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cstddef>
#include <vector>

namespace cad
{
    // bulge = tan(sweep/4) must be finite and nonzero; its sign selects CCW/CW.
    // +/-1 gives a semicircle; |bulge| below/above 1 gives a minor/major arc.
    // Returns steps+1 finite points with exact endpoints; invalid geometry throws
    // invalid_argument, and a sample count exceeding vector capacity throws length_error.
    std::vector<Vec2> sampleArc(
        const Vec2& start,
        const Vec2& end,
        double bulge,
        std::size_t steps);
}
