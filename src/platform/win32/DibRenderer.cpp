#include "DibRenderer.h"
#include "render/LineClipping.h"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <limits>

namespace cad::win32
{
    void drawWuPolyline(BackBuffer& buffer, std::span<const Vec2> screenPoints,
        std::uint32_t color, WuScratch& scratch, int strokeWidth, const RECT* clipRect)
    {
        auto* pixels = buffer.pixels();
        const int width = buffer.width(), height = buffer.height();
        if (!pixels || width <= 0 || height <= 0 || strokeWidth <= 0 || screenPoints.empty()) return;
        // Bound pathological brush requests before asking the neutral rasterizer
        // to generate a footprint. Persistent widths are 1..32 plus selection emphasis.
        if (static_cast<std::int64_t>(strokeWidth) > 2LL * (std::max)(width, height) + 2) return;
        const int xmin = clipRect ? (std::max)(0L, clipRect->left) : 0;
        const int ymin = clipRect ? (std::max)(0L, clipRect->top) : 0;
        const int xmax = clipRect ? (std::min)(static_cast<LONG>(width), clipRect->right) : width;
        const int ymax = clipRect ? (std::min)(static_cast<LONG>(height), clipRect->bottom) : height;
        if (xmin >= xmax || ymin >= ymax) return;
        // Reject the entire polyline, rather than painting only its valid prefix.
        for (const auto& point : screenPoints)
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) return;

        const auto pixelCount = static_cast<std::size_t>(width) * height;
        if (scratch.coverage.size() != pixelCount) scratch.coverage.assign(pixelCount, 0.0);
        else for (const auto index : scratch.touched) scratch.coverage[index] = 0.0;
        scratch.touched.clear();
        const int margin = strokeWidth / 2 + 2;
        // Include neighbouring centres whose AA footprint can reach into the clip.
        const Vec2 origin{static_cast<double>(xmin) - margin, static_cast<double>(ymin) - margin};
        const auto clipWidth = static_cast<std::int64_t>(xmax) - xmin + 2LL * margin;
        const auto clipHeight = static_cast<std::int64_t>(ymax) - ymin + 2LL * margin;
        if (clipWidth > (std::numeric_limits<int>::max)() || clipHeight > (std::numeric_limits<int>::max)()) return;
        const bool dotStroke = std::all_of(screenPoints.begin(), screenPoints.end(),
            [&](const Vec2& point) { return point == screenPoints.front(); });
        const auto segmentCount = dotStroke ? 1 : screenPoints.size() - 1;
        for (std::size_t segment = 0; segment < segmentCount; ++segment)
        {
            const Vec2 start = screenPoints[segment];
            const Vec2 end = dotStroke ? start : screenPoints[segment + 1];
            const auto clipped = render::clipLineToViewport(start - origin, end - origin,
                static_cast<int>(clipWidth), static_cast<int>(clipHeight));
            if (!clipped) continue;
            // Duplicate vertices of a sampled curve do not create extra square dots.
            if (start == end && !dotStroke) continue;
            scratch.samples.clear();
            if (!render::rasterizeWuLine(clipped->start + origin, clipped->end + origin, scratch.samples, strokeWidth)) continue;
            for (const auto& sample : scratch.samples)
            {
                if (sample.x < xmin || sample.x >= xmax || sample.y < ymin || sample.y >= ymax) continue;
                const auto index = static_cast<std::size_t>(sample.y) * width + sample.x;
                if (scratch.coverage[index] == 0.0) scratch.touched.push_back(index);
                // Thin Wu fragments partition endpoint intervals. Wide strokes
                // form a union of round capsules: max coverage prevents overlaps
                // at joins from making the stroke thicker or darker.
                scratch.coverage[index] = strokeWidth > 1
                    ? (std::max)(scratch.coverage[index], sample.coverage)
                    : (std::min)(1.0, scratch.coverage[index] + sample.coverage);
            }
        }
        for (const auto index : scratch.touched)
        {
            const double alpha = scratch.coverage[index];
            const auto background = pixels[index];
            std::uint32_t blended = 0;
            for (const unsigned shift : {0u, 8u, 16u})
            {
                const double foreground = (color >> shift) & 255u;
                const double previous = (background >> shift) & 255u;
                const auto channel = static_cast<std::uint32_t>(std::lround(previous + alpha * (foreground - previous)));
                blended |= channel << shift;
            }
            pixels[index] = blended;
        }
    }

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


