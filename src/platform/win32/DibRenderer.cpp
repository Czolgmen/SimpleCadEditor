#include "DibRenderer.h"
#include "render/LineClipping.h"

#include <cmath>
#include <cstddef>
#include <algorithm>

namespace cad::win32
{
    void drawBresenhamLine(
        BackBuffer& buffer,
        Vec2 screenStart,
        Vec2 screenEnd,
        std::uint32_t color,
        std::vector<cad::render::PixelPoint>& scratch,
        int strokeWidth,
        const RECT* clipRect)
    {
        auto* pixels = buffer.pixels();
        const int width = buffer.width();
        const int height = buffer.height();
        if (!pixels || width <= 0 || height <= 0 || strokeWidth <= 0)
            return;

        const int xmin = clipRect ? (std::max)(0, static_cast<int>(clipRect->left)) : 0;
        const int ymin = clipRect ? (std::max)(0, static_cast<int>(clipRect->top)) : 0;
        const int xmax = clipRect ? (std::min)(width, static_cast<int>(clipRect->right)) : width;
        const int ymax = clipRect ? (std::min)(height, static_cast<int>(clipRect->bottom)) : height;
        if (xmin >= xmax || ymin >= ymax)
            return;

        const Vec2 origin{static_cast<double>(xmin), static_cast<double>(ymin)};
        const auto clipped = cad::render::clipLineToViewport(
            screenStart - origin, screenEnd - origin, xmax - xmin, ymax - ymin);

        if (!clipped)
            return;

        const cad::render::PixelPoint start{
            static_cast<int>(std::lround(clipped->start.x + xmin)),
            static_cast<int>(std::lround(clipped->start.y + ymin))};
        const cad::render::PixelPoint end{
            static_cast<int>(std::lround(clipped->end.x + xmin)),
            static_cast<int>(std::lround(clipped->end.y + ymin))};

        scratch.clear();
        cad::render::rasterizeLine(start, end, scratch);

        for (const auto pixel : scratch)
        {
            if (pixel.x < xmin || pixel.x >= xmax || pixel.y < ymin || pixel.y >= ymax)
                continue;

            // A square brush preserves the normal/selected stroke widths.
            // Wide arithmetic also makes oversized brushes safe at the borders.
            const auto left = static_cast<std::int64_t>(pixel.x) - strokeWidth / 2;
            const auto top = static_cast<std::int64_t>(pixel.y) - strokeWidth / 2;
            const int x0 = static_cast<int>((std::max)(static_cast<std::int64_t>(xmin), left));
            const int y0 = static_cast<int>((std::max)(static_cast<std::int64_t>(ymin), top));
            const int x1 = static_cast<int>((std::min)(static_cast<std::int64_t>(xmax), left + strokeWidth));
            const int y1 = static_cast<int>((std::min)(static_cast<std::int64_t>(ymax), top + strokeWidth));
            for (int y = y0; y < y1; ++y)
            {
                const auto row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
                std::fill(pixels + row + x0, pixels + row + x1, color);
            }
        }
    }
} // namespace cad::win32


