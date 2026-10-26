#include "Bresenham.h"

#include <cstdint>
#include <cstdlib>

namespace cad::render
{
    void rasterizeLine(PixelPoint start, PixelPoint end, std::vector<PixelPoint> &pixels)
    {
        int x = start.x;
        int y = start.y;

        const std::int64_t dx = std::abs(static_cast<std::int64_t>(end.x) - start.x);
        const std::int64_t dy = -std::abs(static_cast<std::int64_t>(end.y) - start.y);

        const int sx = start.x < end.x ? 1 : -1;
        const int sy = start.y < end.y ? 1 : -1;

        std::int64_t error = dx + dy;

        while(1)
        {
            pixels.push_back({x, y});

            if (x == end.x && y == end.y)
                break;

            const std::int64_t e2 = 2 * error;

            if (e2 >= dy)
            {
                error += dy;
                x += sx;
            }

            if (e2 <= dx)
            {
                error += dx;
                y += sy;
            }
        }
    }
} // namespace cad::render
