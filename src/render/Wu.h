#pragma once

#include <cad_core/geometry/Vec2.h>
#include <vector>

namespace cad::render
{
    struct CoveragePixel
    {
        int x;
        int y;
        double coverage;
    };

    // Xiaolin Wu: fractional endpoints, all octants, pixel coverage in (0,1].
    // Appends to the vector. Clip before calling to bound work. Invalid/non-finite
    // input or an unrepresentable raster footprint returns false without appending.
    // Width 1 uses classic Wu weights (a zero-length segment is a square dot).
    // Wider strokes use perpendicular distance coverage and round caps/joins;
    // combine those samples with max, not addition, across a complete polyline.
    bool rasterizeWuLine(Vec2 start, Vec2 end, std::vector<CoveragePixel>& pixels, double strokeWidth = 1.0);
}
