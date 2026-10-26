#include <render/Bresenham.h>
#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>

namespace
{
    using cad::render::PixelPoint;
    using cad::render::rasterizeLine;
    using cad::tests::require;

    bool same(PixelPoint a, PixelPoint b) { return a.x == b.x && a.y == b.y; }

    void requirePixels(std::span<const PixelPoint> actual, std::span<const PixelPoint> expected)
    {
        require(actual.size() == expected.size(), "Wrong number of rasterized pixels");
        for (std::size_t i = 0; i < expected.size(); ++i)
            require(same(actual[i], expected[i]), "Unexpected rasterized pixel or order");
    }

    std::vector<PixelPoint> raster(PixelPoint start, PixelPoint end)
    {
        std::vector<PixelPoint> pixels;
        rasterizeLine(start, end, pixels);
        return pixels;
    }

    void requirePath(PixelPoint start, PixelPoint end, std::span<const PixelPoint> pixels)
    {
        const std::int64_t dx = static_cast<std::int64_t>(end.x) - start.x;
        const std::int64_t dy = static_cast<std::int64_t>(end.y) - start.y;
        const auto major = (std::max)(std::abs(dx), std::abs(dy));
        require(pixels.size() == static_cast<std::size_t>(major) + 1, "Raster must have max(abs(dx),abs(dy))+1 pixels");
        require(same(pixels.front(), start) && same(pixels.back(), end), "Raster lost an endpoint");
        for (std::size_t i = 0; i < pixels.size(); ++i)
        {
            const auto point = pixels[i];
            require(point.x >= (std::min)(start.x, end.x) && point.x <= (std::max)(start.x, end.x) &&
                point.y >= (std::min)(start.y, end.y) && point.y <= (std::max)(start.y, end.y), "Pixel left the endpoint bounding box");
            // Independent line equation: at each dominant-axis coordinate, the
            // chosen minor coordinate must be within half a pixel of the ideal line.
            const std::int64_t px = static_cast<std::int64_t>(point.x) - start.x;
            const std::int64_t py = static_cast<std::int64_t>(point.y) - start.y;
            require(2 * std::abs(dx * py - dy * px) <= major, "Pixel is not nearest to the ideal line");
            if (i == 0) continue;
            const std::int64_t stepX = static_cast<std::int64_t>(point.x) - pixels[i - 1].x;
            const std::int64_t stepY = static_cast<std::int64_t>(point.y) - pixels[i - 1].y;
            require((std::max)(std::abs(stepX), std::abs(stepY)) == 1, "Raster skipped or duplicated a pixel");
            require((dx >= 0 ? stepX >= 0 : stepX <= 0) && (dy >= 0 ? stepY >= 0 : stepY <= 0), "Raster stepped backwards");
            if (std::abs(dx) >= std::abs(dy)) require(std::abs(stepX) == 1, "Raster skipped a dominant X coordinate");
            else require(std::abs(stepY) == 1, "Raster skipped a dominant Y coordinate");
        }
    }

    void horizontalLinesIncludeBothEndsInBothDirections()
    {
        const std::array expected{PixelPoint{-2, 7}, PixelPoint{-1, 7}, PixelPoint{0, 7}, PixelPoint{1, 7}, PixelPoint{2, 7}};
        requirePixels(raster(expected.front(), expected.back()), expected);
        auto reversed = expected;
        std::reverse(reversed.begin(), reversed.end());
        requirePixels(raster(reversed.front(), reversed.back()), reversed);
    }

    void verticalLinesIncludeBothEndsInBothDirections()
    {
        const std::array expected{PixelPoint{-5, -2}, PixelPoint{-5, -1}, PixelPoint{-5, 0}, PixelPoint{-5, 1}, PixelPoint{-5, 2}};
        requirePixels(raster(expected.front(), expected.back()), expected);
        auto reversed = expected;
        std::reverse(reversed.begin(), reversed.end());
        requirePixels(raster(reversed.front(), reversed.back()), reversed);
    }

    void diagonalLinesStepAlongBothAxes()
    {
        for (const int sx : {-1, 1})
        for (const int sy : {-1, 1})
        {
            std::vector<PixelPoint> expected;
            for (int i = 0; i <= 5; ++i) expected.push_back({-7 + sx * i, 11 + sy * i});
            requirePixels(raster(expected.front(), expected.back()), expected);
            std::reverse(expected.begin(), expected.end());
            requirePixels(raster(expected.front(), expected.back()), expected);
        }
    }

    void knownShallowAndSteepPixelsCoverAllOctants()
    {
        const std::array reference{PixelPoint{0, 0}, PixelPoint{1, 0}, PixelPoint{2, 1},
            PixelPoint{3, 1}, PixelPoint{4, 2}, PixelPoint{5, 2}};
        for (const bool swap : {false, true})
        for (const int sx : {-1, 1})
        for (const int sy : {-1, 1})
        {
            std::vector<PixelPoint> expected;
            for (const auto point : reference)
                expected.push_back({7 + sx * (swap ? point.y : point.x), -11 + sy * (swap ? point.x : point.y)});
            requirePixels(raster(expected.front(), expected.back()), expected);
            std::reverse(expected.begin(), expected.end()); // This slope has no halfway ties.
            requirePixels(raster(expected.front(), expected.back()), expected);
        }
    }

    void zeroLengthLinesEmitOnePixelIncludingExtremeCoordinates()
    {
        const int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
        for (const PixelPoint point : {PixelPoint{}, PixelPoint{-8, 13}, PixelPoint{low, high}, PixelPoint{high, low}})
        {
            const std::array expected{point};
            requirePixels(raster(point, point), expected);
        }
    }

    void allEightAdjacentPixelsNeedExactlyOneStep()
    {
        for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            if (x == 0 && y == 0) continue;
            const std::array expected{PixelPoint{4, -3}, PixelPoint{4 + x, -3 + y}};
            requirePixels(raster(expected.front(), expected.back()), expected);
        }
    }

    void appendsKeepPrefixAndSharedEndpoints()
    {
        std::vector<PixelPoint> pixels{{99, -99}, {1, 1}, {1, 1}};
        rasterizeLine({0, 0}, {5, 2}, pixels);
        rasterizeLine({5, 2}, {5, 4}, pixels);
        rasterizeLine({100, 100}, {100, 100}, pixels);
        const std::array expected{PixelPoint{99, -99}, PixelPoint{1, 1}, PixelPoint{1, 1},
            PixelPoint{0, 0}, PixelPoint{1, 0}, PixelPoint{2, 1}, PixelPoint{3, 1}, PixelPoint{4, 2}, PixelPoint{5, 2},
            PixelPoint{5, 2}, PixelPoint{5, 3}, PixelPoint{5, 4}, PixelPoint{100, 100}};
        requirePixels(pixels, expected);
    }

    void shortLinesNearIntegerBoundsStayValid()
    {
        const int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
        const std::array origins{PixelPoint{low + 8, low + 8}, PixelPoint{high - 8, high - 8},
            PixelPoint{low + 8, high - 8}, PixelPoint{high - 8, low + 8}};
        for (const auto origin : origins)
        for (const PixelPoint delta : {PixelPoint{5, 2}, PixelPoint{-5, 2}, PixelPoint{5, -2}, PixelPoint{-5, -2},
            PixelPoint{2, 5}, PixelPoint{-2, 5}, PixelPoint{2, -5}, PixelPoint{-2, -5}})
        {
            const PixelPoint end{origin.x + delta.x, origin.y + delta.y};
            requirePath(origin, end, raster(origin, end));
            requirePath(end, origin, raster(end, origin));
        }
        for (const auto endpoints : {std::array{PixelPoint{low, low}, PixelPoint{low + 3, low + 1}},
            std::array{PixelPoint{high, high}, PixelPoint{high - 1, high - 3}}})
        {
            requirePath(endpoints[0], endpoints[1], raster(endpoints[0], endpoints[1]));
            requirePath(endpoints[1], endpoints[0], raster(endpoints[1], endpoints[0]));
        }
    }

    void exhaustiveSmallGridIsConnectedMonotoneAndNearest()
    {
        for (int x0 = -4; x0 <= 4; ++x0)
        for (int y0 = -4; y0 <= 4; ++y0)
        for (int x1 = -4; x1 <= 4; ++x1)
        for (int y1 = -4; y1 <= 4; ++y1)
            requirePath({x0, y0}, {x1, y1}, raster({x0, y0}, {x1, y1}));
    }

    void halfwayTiesKeepDirectionDependentRounding()
    {
        const std::array forward{PixelPoint{0, 0}, PixelPoint{1, 1}, PixelPoint{2, 1}, PixelPoint{3, 2}, PixelPoint{4, 2}};
        const std::array backward{PixelPoint{4, 2}, PixelPoint{3, 1}, PixelPoint{2, 1}, PixelPoint{1, 0}, PixelPoint{0, 0}};
        requirePixels(raster({0, 0}, {4, 2}), forward);
        requirePixels(raster({4, 2}, {0, 0}), backward);
        requirePath({0, 0}, {4, 2}, forward);
        requirePath({4, 2}, {0, 0}, backward);
    }

    void longShallowAndSteepLinesStayNearest()
    {
        for (const PixelPoint delta : {PixelPoint{4096, 1}, PixelPoint{1, 4096}, PixelPoint{-4096, 31},
            PixelPoint{31, -4096}, PixelPoint{-257, -4096}, PixelPoint{-4096, -257}})
        {
            const PixelPoint start{-100, 200}, end{start.x + delta.x, start.y + delta.y};
            requirePath(start, end, raster(start, end));
            requirePath(end, start, raster(end, start));
        }
    }

    void translationsReflectionsAndAxisSwapPreserveRaster()
    {
        const PixelPoint start{3, -7}, end{22, 8};
        const auto reference = raster(start, end);
        for (const PixelPoint offset : {PixelPoint{-1000, 2000}, PixelPoint{1234, -4321}})
        for (const bool swap : {false, true})
        for (const int sx : {-1, 1})
        for (const int sy : {-1, 1})
        {
            const auto transform = [&](PixelPoint point)
            {
                return PixelPoint{offset.x + sx * (swap ? point.y : point.x), offset.y + sy * (swap ? point.x : point.y)};
            };
            std::vector<PixelPoint> expected;
            for (const auto point : reference) expected.push_back(transform(point));
            requirePixels(raster(transform(start), transform(end)), expected);
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> bresenhamTests()
    {
        static const std::array tests{
            TestCase{"horizontal lines include both ends and directions", horizontalLinesIncludeBothEndsInBothDirections},
            TestCase{"vertical lines include both ends and directions", verticalLinesIncludeBothEndsInBothDirections},
            TestCase{"diagonal lines step along both axes", diagonalLinesStepAlongBothAxes},
            TestCase{"known shallow and steep pixels cover all octants", knownShallowAndSteepPixelsCoverAllOctants},
            TestCase{"zero-length lines emit one pixel at extreme coordinates", zeroLengthLinesEmitOnePixelIncludingExtremeCoordinates},
            TestCase{"all eight adjacent pixels need one step", allEightAdjacentPixelsNeedExactlyOneStep},
            TestCase{"appends preserve prefix and shared endpoints", appendsKeepPrefixAndSharedEndpoints},
            TestCase{"short lines near integer bounds stay valid", shortLinesNearIntegerBoundsStayValid},
            TestCase{"exhaustive small grid is connected monotone and nearest", exhaustiveSmallGridIsConnectedMonotoneAndNearest},
            TestCase{"halfway ties have direction-dependent rounding", halfwayTiesKeepDirectionDependentRounding},
            TestCase{"long shallow and steep lines stay nearest", longShallowAndSteepLinesStayNearest},
            TestCase{"translations reflections and axis swap preserve raster", translationsReflectionsAndAxisSwapPreserveRaster}
        };
        return tests;
    }
}
