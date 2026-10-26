#include <render/LineClipping.h>
#include <render/Bresenham.h>
#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

namespace
{
    using cad::Vec2;
    using cad::render::ClippedSegment;
    using cad::render::clipLineToViewport;
    using cad::tests::require;

    void requirePoint(Vec2 actual, Vec2 expected)
    {
        require(std::isfinite(actual.x) && std::isfinite(actual.y), "Clipped point must be finite");
        require(std::abs(actual.x - expected.x) <= 1e-10 &&
            std::abs(actual.y - expected.y) <= 1e-10, "Unexpected clipped point");
    }

    void requireSegment(Vec2 start, Vec2 end, Vec2 expectedStart, Vec2 expectedEnd,
        int width = 101, int height = 81)
    {
        const auto result = clipLineToViewport(start, end, width, height);
        require(result.has_value(), "Visible segment was rejected");
        requirePoint(result->start, expectedStart);
        requirePoint(result->end, expectedEnd);
    }

    void requireBothDirections(Vec2 start, Vec2 end, Vec2 expectedStart, Vec2 expectedEnd,
        int width = 101, int height = 81)
    {
        requireSegment(start, end, expectedStart, expectedEnd, width, height);
        requireSegment(end, start, expectedEnd, expectedStart, width, height);
    }

    void requireInViewport(Vec2 point, int width, int height)
    {
        require(std::isfinite(point.x) && std::isfinite(point.y) &&
            point.x >= 0 && point.x <= width - 1 &&
            point.y >= 0 && point.y <= height - 1, "Clipped point left viewport");
    }

    // Independent reference: intersect the parameter intervals allowed by
    // each coordinate. No outcodes or iterative endpoint replacement.
    std::optional<ClippedSegment> referenceClip(Vec2 start, Vec2 end, int width, int height)
    {
        double enter = 0, leave = 1;
        const auto restrictAxis = [&](double origin, double delta, double upper)
        {
            if (delta == 0) return origin >= 0 && origin <= upper;
            double first = -origin / delta;
            double last = (upper - origin) / delta;
            if (first > last) std::swap(first, last);
            enter = (std::max)(enter, first);
            leave = (std::min)(leave, last);
            return enter <= leave;
        };
        const double dx = end.x - start.x, dy = end.y - start.y;
        if (!restrictAxis(start.x, dx, width - 1) || !restrictAxis(start.y, dy, height - 1))
            return std::nullopt;
        return ClippedSegment{{start.x + enter * dx, start.y + enter * dy},
            {start.x + leave * dx, start.y + leave * dy}};
    }

    void insideEndpointsArePreservedExactly()
    {
        for (const auto segment : {ClippedSegment{{0, 0}, {100, 80}},
            ClippedSegment{{12.125, 5.75}, {99.5, 79.875}}, ClippedSegment{{100, 0}, {0, 80}}})
        {
            const auto result = clipLineToViewport(segment.start, segment.end, 101, 81);
            require(result.has_value(), "Inside segment was rejected");
            require(result->start == segment.start && result->end == segment.end,
                "Inside coordinates must stay exactly unchanged");
        }
    }

    void eachBoundaryClipsOneEndpoint()
    {
        requireBothDirections({-20, 10}, {20, 30}, {0, 20}, {20, 30});
        requireBothDirections({120, 10}, {80, 30}, {100, 20}, {80, 30});
        requireBothDirections({10, -20}, {30, 20}, {20, 0}, {30, 20});
        requireBothDirections({10, 100}, {30, 60}, {20, 80}, {30, 60});
    }

    void crossingSegmentsClipBothEnds()
    {
        requireBothDirections({-20, 40}, {120, 40}, {0, 40}, {100, 40});
        requireBothDirections({50, -20}, {50, 100}, {50, 0}, {50, 80});
        requireBothDirections({-100, -80}, {200, 160}, {0, 0}, {100, 80});
        requireBothDirections({-100, 160}, {200, -80}, {0, 80}, {100, 0});
        requireBothDirections({-100, -10}, {200, 50}, {0, 10}, {100, 30});
    }

    void commonOutsideRegionsReject()
    {
        for (const auto segment : {ClippedSegment{{-20, 10}, {-1, 70}},
            ClippedSegment{{101, 10}, {120, 70}}, ClippedSegment{{10, -20}, {90, -1}},
            ClippedSegment{{10, 81}, {90, 100}}, ClippedSegment{{-50, -50}, {-1, -1}}})
        {
            require(!clipLineToViewport(segment.start, segment.end, 101, 81), "Invisible segment accepted");
            require(!clipLineToViewport(segment.end, segment.start, 101, 81), "Reversed invisible segment accepted");
        }
    }

    void differentOutsideRegionsCanStillMiss()
    {
        for (const auto segment : {ClippedSegment{{-20, 5}, {5, -20}},
            ClippedSegment{{95, -20}, {120, 5}}, ClippedSegment{{95, 100}, {120, 75}},
            ClippedSegment{{-20, 75}, {5, 100}}})
        {
            require(!clipLineToViewport(segment.start, segment.end, 101, 81), "Corner-missing line accepted");
            require(!clipLineToViewport(segment.end, segment.start, 101, 81), "Reversed corner-missing line accepted");
        }
    }

    void boundaryLinesAreInclusive()
    {
        requireBothDirections({-10, 0}, {110, 0}, {0, 0}, {100, 0});
        requireBothDirections({-10, 80}, {110, 80}, {0, 80}, {100, 80});
        requireBothDirections({0, -10}, {0, 90}, {0, 0}, {0, 80});
        requireBothDirections({100, -10}, {100, 90}, {100, 0}, {100, 80});
    }

    void cornerTangenciesReturnSinglePoints()
    {
        requireBothDirections({-10, 10}, {10, -10}, {0, 0}, {0, 0});
        requireBothDirections({90, -10}, {110, 10}, {100, 0}, {100, 0});
        requireBothDirections({90, 90}, {110, 70}, {100, 80}, {100, 80});
        requireBothDirections({-10, 70}, {10, 90}, {0, 80}, {0, 80});
        requireBothDirections({-10, -10}, {0, 0}, {0, 0}, {0, 0});
    }

    void zeroLengthSegmentsRespectVisibility()
    {
        for (const Vec2 point : {Vec2{0, 0}, Vec2{100, 80}, Vec2{12.5, 60.25}})
            requireSegment(point, point, point, point);
        for (const Vec2 point : {Vec2{-1, 0}, Vec2{0, -1}, Vec2{101, 80}, Vec2{100, 81}})
            require(!clipLineToViewport(point, point, 101, 81), "Invisible point accepted");
    }

    void onePixelDimensionsRemainValid()
    {
        requireBothDirections({-10, -10}, {10, 10}, {0, 0}, {0, 0}, 1, 1);
        requireSegment({0, 0}, {0, 0}, {0, 0}, {0, 0}, 1, 1);
        require(!clipLineToViewport({-1, 1}, {1, 1}, 1, 1), "Line missed single-pixel viewport");
        requireBothDirections({0, -10}, {0, 100}, {0, 0}, {0, 80}, 1, 81);
        requireBothDirections({-10, 20}, {10, 40}, {0, 30}, {0, 30}, 1, 81);
        requireBothDirections({-10, 0}, {110, 0}, {0, 0}, {100, 0}, 101, 1);
        requireBothDirections({20, -10}, {40, 10}, {30, 0}, {30, 0}, 101, 1);
    }

    void fractionalIntersectionsAreNotRounded()
    {
        requireBothDirections({-2, 0.25}, {2, 1.25}, {0, 0.75}, {2, 1.25});
        requireBothDirections({0.25, -2}, {1.25, 2}, {0.75, 0}, {1.25, 2});
        requireBothDirections({98, 0.25}, {102, 1.25}, {98, 0.25}, {100, 0.75});
        requireBothDirections({0.25, 78}, {1.25, 82}, {0.25, 78}, {0.75, 80});
    }

    void invalidDimensionsRejectBeforeArithmetic()
    {
        for (const int dimension : {0, -1, std::numeric_limits<int>::min()})
        {
            require(!clipLineToViewport({0, 0}, {1, 1}, dimension, 10), "Invalid width accepted");
            require(!clipLineToViewport({0, 0}, {1, 1}, 10, dimension), "Invalid height accepted");
        }
    }

    void nonFiniteCoordinatesAlwaysReject()
    {
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (int coordinate = 0; coordinate < 4; ++coordinate)
        {
            Vec2 start{20, 20}, end{60, 60};
            switch (coordinate)
            {
            case 0: start.x = invalid; break;
            case 1: start.y = invalid; break;
            case 2: end.x = invalid; break;
            case 3: end.y = invalid; break;
            }
            require(!clipLineToViewport(start, end, 101, 81), "Non-finite coordinate accepted");
        }
    }

    void largeCoordinatesAndViewportBoundsAreSupported()
    {
        requireBothDirections({-1e12, 40}, {1e12, 40}, {0, 40}, {100, 40});
        requireBothDirections({50, -1e12}, {50, 1e12}, {50, 0}, {50, 80});
        const int size = std::numeric_limits<int>::max();
        const double upper = static_cast<double>(size) - 1;
        requireBothDirections({-10, 7}, {upper + 10, 7}, {0, 7}, {upper, 7}, size, size);
        requireBothDirections({7, -10}, {7, upper + 10}, {7, 0}, {7, upper}, size, size);
    }

    void nonFiniteIntermediateIntersectionsReject()
    {
        const double largest = std::numeric_limits<double>::max();
        // Finite inputs, but these expressions overflow before intersection
        // coordinates can be represented. The current API rejects such a case.
        for (const auto segment : {ClippedSegment{{-largest, -largest}, {largest, largest}},
            ClippedSegment{{-largest, largest}, {largest, -largest}}})
        {
            require(!clipLineToViewport(segment.start, segment.end, 101, 81), "Non-finite intersection accepted");
            require(!clipLineToViewport(segment.end, segment.start, 101, 81), "Reversed non-finite intersection accepted");
        }
    }

    void exhaustiveGridMatchesIndependentParameterIntervals()
    {
        for (const int width : {1, 2, 4})
        for (const int height : {1, 3, 5})
        for (int x0 = -2; x0 <= 4; ++x0)
        for (int y0 = -2; y0 <= 4; ++y0)
        for (int x1 = -2; x1 <= 4; ++x1)
        for (int y1 = -2; y1 <= 4; ++y1)
        {
            const Vec2 start{static_cast<double>(x0), static_cast<double>(y0)};
            const Vec2 end{static_cast<double>(x1), static_cast<double>(y1)};
            const auto expected = referenceClip(start, end, width, height);
            const auto actual = clipLineToViewport(start, end, width, height);
            if (actual.has_value() != expected.has_value())
            {
                std::ostringstream message;
                message << "Visibility mismatch: (" << x0 << ',' << y0 << ")->("
                    << x1 << ',' << y1 << ") in " << width << 'x' << height;
                require(false, message.str());
            }
            if (!actual) continue;
            requirePoint(actual->start, expected->start);
            requirePoint(actual->end, expected->end);
            requireInViewport(actual->start, width, height);
            requireInViewport(actual->end, width, height);
        }
    }

    void reversalReflectionsAndRepeatedClippingPreserveResults()
    {
        for (const auto segment : {ClippedSegment{{-23.5, 11.25}, {140.25, 70.5}},
            ClippedSegment{{20, -40}, {70, 130}}, ClippedSegment{{30, 30}, {60, 60}},
            ClippedSegment{{-20, 5}, {5, -20}}})
        for (const bool swapAxes : {false, true})
        for (const bool flipX : {false, true})
        for (const bool flipY : {false, true})
        {
            const auto transform = [&](Vec2 point)
            {
                if (flipX) point.x = 100 - point.x;
                if (flipY) point.y = 80 - point.y;
                if (swapAxes) std::swap(point.x, point.y);
                return point;
            };
            const auto reference = clipLineToViewport(segment.start, segment.end, 101, 81);
            const int width = swapAxes ? 81 : 101, height = swapAxes ? 101 : 81;
            const Vec2 start = transform(segment.start), end = transform(segment.end);
            const auto result = clipLineToViewport(start, end, width, height);
            const auto reverse = clipLineToViewport(end, start, width, height);
            require(result.has_value() == reference.has_value() &&
                reverse.has_value() == result.has_value(), "Transform changed visibility");
            if (!result) continue;
            requirePoint(result->start, transform(reference->start));
            requirePoint(result->end, transform(reference->end));
            requirePoint(reverse->start, result->end);
            requirePoint(reverse->end, result->start);
            const auto repeated = clipLineToViewport(result->start, result->end, width, height);
            require(repeated && repeated->start == result->start && repeated->end == result->end,
                "Repeated clipping changed an already clipped segment");
        }
    }

    void clippedSegmentsRasterizeOnlyInsideViewport()
    {
        constexpr int width = 31, height = 21;
        for (const auto segment : {ClippedSegment{{-1e12, 7}, {1e12, 7}},
            ClippedSegment{{5, -1e12}, {5, 1e12}}, ClippedSegment{{-10, -10}, {50, 40}},
            ClippedSegment{{-5, 5}, {5, -5}}, ClippedSegment{{-8.5, 3.25}, {40.75, 15.5}}})
        {
            const auto clipped = clipLineToViewport(segment.start, segment.end, width, height);
            require(clipped.has_value(), "Visible rasterization example rejected");
            const auto pixel = [](Vec2 point)
            {
                return cad::render::PixelPoint{static_cast<int>(std::lround(point.x)),
                    static_cast<int>(std::lround(point.y))};
            };
            std::vector<cad::render::PixelPoint> pixels;
            cad::render::rasterizeLine(pixel(clipped->start), pixel(clipped->end), pixels);
            require(!pixels.empty() && pixels.size() <= width, "Rasterization exceeded viewport-sized path");
            for (const auto point : pixels)
                require(point.x >= 0 && point.x < width && point.y >= 0 && point.y < height,
                    "Clipped Bresenham raster escaped viewport");
        }
    }

    void immediatelyOutsideBoundariesAreNotInside()
    {
        const double negative = std::nextafter(0.0, -1.0);
        const double right = std::nextafter(100.0, 101.0);
        const double bottom = std::nextafter(80.0, 81.0);
        for (const Vec2 point : {Vec2{negative, 40}, Vec2{50, negative},
            Vec2{right, 40}, Vec2{50, bottom}})
            require(!clipLineToViewport(point, point, 101, 81), "Outside boundary point accepted with implicit tolerance");
        requireBothDirections({50, bottom}, {50, 40}, {50, 80}, {50, 40});
        requireBothDirections({right, 40}, {50, 40}, {100, 40}, {50, 40});
    }
}

namespace cad::tests
{
    std::span<const TestCase> lineClippingTests()
    {
        static const std::array tests{
            TestCase{"inside endpoints stay exactly unchanged", insideEndpointsArePreservedExactly},
            TestCase{"each boundary clips one endpoint in both directions", eachBoundaryClipsOneEndpoint},
            TestCase{"crossing segments clip both endpoints", crossingSegmentsClipBothEnds},
            TestCase{"common outside regions reject", commonOutsideRegionsReject},
            TestCase{"different outside regions can still miss", differentOutsideRegionsCanStillMiss},
            TestCase{"boundary lines are inclusive", boundaryLinesAreInclusive},
            TestCase{"corner tangencies return single points", cornerTangenciesReturnSinglePoints},
            TestCase{"zero-length segments respect visibility", zeroLengthSegmentsRespectVisibility},
            TestCase{"one-pixel dimensions remain valid", onePixelDimensionsRemainValid},
            TestCase{"fractional intersections are not rounded", fractionalIntersectionsAreNotRounded},
            TestCase{"invalid dimensions reject before arithmetic", invalidDimensionsRejectBeforeArithmetic},
            TestCase{"non-finite coordinates always reject", nonFiniteCoordinatesAlwaysReject},
            TestCase{"large coordinates and viewport bounds are supported", largeCoordinatesAndViewportBoundsAreSupported},
            TestCase{"non-finite intermediate intersections reject", nonFiniteIntermediateIntersectionsReject},
            TestCase{"exhaustive grid matches independent parameter intervals", exhaustiveGridMatchesIndependentParameterIntervals},
            TestCase{"reversal reflections and repeated clipping preserve results", reversalReflectionsAndRepeatedClippingPreserveResults},
            TestCase{"clipped segments rasterize only inside viewport", clippedSegmentsRasterizeOnlyInsideViewport},
            TestCase{"immediately outside boundaries are not inside", immediatelyOutsideBoundariesAreNotInside}
        };
        return tests;
    }
}
