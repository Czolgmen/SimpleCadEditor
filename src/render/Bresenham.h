#pragma once

#include <vector>

namespace cad::render
{
    struct PixelPoint
    {
        int x;
        int y;
    };

    void rasterizeLine(
        PixelPoint start,
        PixelPoint end,
        std::vector<PixelPoint>& pixels
    );
    
} // namespace cad::render
