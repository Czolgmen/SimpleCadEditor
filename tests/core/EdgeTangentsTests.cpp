#include <cad_core/model/EdgeTangents.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    using namespace cad;
    using cad::tests::require;

    void requireNear(Vec2 actual, Vec2 expected, double tolerance = 1e-12)
    {
        double scale = (std::max)(std::abs(expected.x), std::abs(expected.y));
        if (scale == 0) scale = 1;
        require(std::isfinite(actual.x) && std::isfinite(actual.y) &&
            std::isfinite(expected.x) && std::isfinite(expected.y), "Non-finite tangent result");
        require(std::abs(actual.x - expected.x) <= tolerance * scale &&
            std::abs(actual.y - expected.y) <= tolerance * scale, "Unexpected tangent direction or magnitude");
    }

    void segmentTangentsEqualItsDisplacement()
    {
        const std::array cases{
            std::array{Vec2{-2.5, 7.25}, Vec2{11.75, -4}, Vec2{14.25, -11.25}},
            std::array{Vec2{2, 3}, Vec2{-7, 3}, Vec2{-9, 0}},
            std::array{Vec2{-4, -5}, Vec2{-4, 12}, Vec2{0, 17}}
        };
        for (const auto& value : cases)
        {
            requireNear(edgeStartTangent(LineSegment{}, value[0], value[1]), value[2]);
            requireNear(edgeEndTangent(LineSegment{}, value[0], value[1]), value[2]);
        }
    }

    void zeroLengthSegmentReturnsZeroTangents()
    {
        for (const Vec2 position : {Vec2{}, Vec2{-2.5, 7.25}})
        {
            requireNear(edgeStartTangent(LineSegment{}, position, position), {});
            requireNear(edgeEndTangent(LineSegment{}, position, position), {});
        }
    }

    void bezierTangentsMatchKnownEndpointDerivatives()
    {
        const EdgeGeometry curve = CubicBezier{{3, 5}, {10, -4}};
        requireNear(edgeStartTangent(curve, {1, 2}, {14, 8}), {6, 9});
        requireNear(edgeEndTangent(curve, {1, 2}, {14, 8}), {12, 36});
    }

    void endpointTangentUsesOnlyAdjacentHandleAndEndpoint()
    {
        const EdgeGeometry startCurve = CubicBezier{{3, 5}, {-100, 200}};
        requireNear(edgeStartTangent(startCurve, {1, 2}, {-400, 500}), {6, 9});
        const EdgeGeometry endCurve = CubicBezier{{-100, 200}, {10, -4}};
        requireNear(edgeEndTangent(endCurve, {-400, 500}, {14, 8}), {12, 36});
    }

    void thirdsHandlesMatchSegmentDerivatives()
    {
        const Vec2 start{2, -1}, end{11, 11};
        const EdgeGeometry curve = CubicBezier{{5, 3}, {8, 7}};
        requireNear(edgeStartTangent(curve, start, end), {9, 12});
        requireNear(edgeEndTangent(curve, start, end), {9, 12});
        requireNear(edgeStartTangent(curve, start, end), edgeStartTangent(LineSegment{}, start, end));
        requireNear(edgeEndTangent(curve, start, end), edgeEndTangent(LineSegment{}, start, end));
    }

    void differentHandleLengthsRetainDerivativeMagnitudes()
    {
        const EdgeGeometry curve = CubicBezier{{2, 0}, {8, 0}};
        const Vec2 start = edgeStartTangent(curve, {0, 0}, {12, 0});
        const Vec2 end = edgeEndTangent(curve, {0, 0}, {12, 0});
        requireNear(start, {6, 0});
        requireNear(end, {12, 0});
        require(cross(start, end) == 0 && dot(start, end) > 0, "Known tangents should have the same direction");
        require(start.x != end.x, "Tangent lengths were discarded or normalized");
    }

    void coincidentHandlesGiveIndependentZeroDerivatives()
    {
        const Vec2 start{1, 2}, end{14, 8};
        for (int mask = 1; mask <= 3; ++mask)
        {
            const EdgeGeometry curve = CubicBezier{(mask & 1) ? start : Vec2{3, 5}, (mask & 2) ? end : Vec2{10, -4}};
            requireNear(edgeStartTangent(curve, start, end), (mask & 1) ? Vec2{} : Vec2{6, 9});
            requireNear(edgeEndTangent(curve, start, end), (mask & 2) ? Vec2{} : Vec2{12, 36});
        }
    }

    void reversalSwapsAndNegatesTangents()
    {
        const Vec2 start{1, 2}, end{14, 8};
        const EdgeGeometry curve = CubicBezier{{3, 5}, {10, -4}};
        const EdgeGeometry reversed = CubicBezier{{10, -4}, {3, 5}};
        requireNear(edgeStartTangent(reversed, end, start), edgeEndTangent(curve, start, end) * -1);
        requireNear(edgeEndTangent(reversed, end, start), edgeStartTangent(curve, start, end) * -1);
        requireNear(edgeStartTangent(LineSegment{}, end, start), edgeEndTangent(LineSegment{}, start, end) * -1);
        requireNear(edgeEndTangent(LineSegment{}, end, start), edgeStartTangent(LineSegment{}, start, end) * -1);
    }

    void tangentsRespectTranslationAndScaling()
    {
        const Vec2 start{1, 2}, end{14, 8}, first{3, 5}, second{10, -4};
        for (const double scale : {-3.5, 1e-90, 1.0, 1e90})
        {
            const EdgeGeometry curve = CubicBezier{first * scale, second * scale};
            requireNear(edgeStartTangent(curve, start * scale, end * scale), Vec2{6, 9} * scale);
            requireNear(edgeEndTangent(curve, start * scale, end * scale), Vec2{12, 36} * scale);
            requireNear(edgeStartTangent(LineSegment{}, start * scale, end * scale), Vec2{13, 6} * scale);
            requireNear(edgeEndTangent(LineSegment{}, start * scale, end * scale), Vec2{13, 6} * scale);
        }
        const Vec2 offset{-100.25, 200.5};
        const EdgeGeometry shifted = CubicBezier{first + offset, second + offset};
        requireNear(edgeStartTangent(shifted, start + offset, end + offset), {6, 9});
        requireNear(edgeEndTangent(shifted, start + offset, end + offset), {12, 36});
        requireNear(edgeStartTangent(LineSegment{}, start + offset, end + offset), {13, 6});
        requireNear(edgeEndTangent(LineSegment{}, start + offset, end + offset), {13, 6});
    }

    Vec2 bernstein(const std::array<Vec2, 4>& points, double t)
    {
        const double u = 1 - t;
        const std::array weights{u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t};
        Vec2 result{};
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            result.x += points[i].x * weights[i];
            result.y += points[i].y * weights[i];
        }
        return result;
    }

    void tangentsMatchIndependentNumericalDerivatives()
    {
        const std::array curves{
            std::array{Vec2{1, 2}, Vec2{3, 5}, Vec2{10, -4}, Vec2{14, 8}},
            std::array{Vec2{0, 0}, Vec2{0, 4}, Vec2{4, 4}, Vec2{4, 0}},
            std::array{Vec2{-3, 7}, Vec2{-3, 7}, Vec2{10, -9}, Vec2{10, -9}}
        };
        constexpr double h = 1e-4;
        for (const auto& points : curves)
        {
            const EdgeGeometry curve = CubicBezier{points[1], points[2]};
            // Second-order one-sided differences, evaluated independently in Bernstein form.
            const Vec2 startDerivative = (bernstein(points, 0) * -3 + bernstein(points, h) * 4 - bernstein(points, 2 * h)) * (1 / (2 * h));
            const Vec2 endDerivative = (bernstein(points, 1) * 3 - bernstein(points, 1 - h) * 4 + bernstein(points, 1 - 2 * h)) * (1 / (2 * h));
            requireNear(startDerivative, edgeStartTangent(curve, points[0], points[3]), 1e-5);
            requireNear(endDerivative, edgeEndTangent(curve, points[0], points[3]), 1e-5);
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> edgeTangentsTests()
    {
        static const std::array tests{
            TestCase{"segment tangents equal displacement", segmentTangentsEqualItsDisplacement},
            TestCase{"zero-length segment returns zero tangents", zeroLengthSegmentReturnsZeroTangents},
            TestCase{"known Bezier endpoint derivatives", bezierTangentsMatchKnownEndpointDerivatives},
            TestCase{"endpoint tangent uses adjacent control and endpoint", endpointTangentUsesOnlyAdjacentHandleAndEndpoint},
            TestCase{"thirds handles match segment derivatives", thirdsHandlesMatchSegmentDerivatives},
            TestCase{"different handle lengths retain derivative magnitudes", differentHandleLengthsRetainDerivativeMagnitudes},
            TestCase{"coincident handles give independent zero derivatives", coincidentHandlesGiveIndependentZeroDerivatives},
            TestCase{"reversal swaps and negates tangents", reversalSwapsAndNegatesTangents},
            TestCase{"translation and scaling of tangents", tangentsRespectTranslationAndScaling},
            TestCase{"independent numerical endpoint derivatives", tangentsMatchIndependentNumericalDerivatives}
        };
        return tests;
    }
}
