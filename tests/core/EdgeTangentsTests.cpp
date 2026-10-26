#include <cad_core/model/EdgeTangents.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>
#include <utility>

namespace
{
    using namespace cad;
    using cad::tests::require;

    static_assert(std::is_same_v<decltype(edgeStartTangent(std::declval<const EdgeGeometry&>(),
        std::declval<const Vec2&>(), std::declval<const Vec2&>())), std::optional<Vec2>>);
    static_assert(std::is_same_v<decltype(edgeEndTangent(std::declval<const EdgeGeometry&>(),
        std::declval<const Vec2&>(), std::declval<const Vec2&>())), std::optional<Vec2>>);

    Vec2 startTangent(const EdgeGeometry& geometry, const Vec2& start, const Vec2& end)
    {
        const auto result = edgeStartTangent(geometry, start, end);
        require(result.has_value(), "Expected a valid start derivative");
        return *result;
    }

    Vec2 endTangent(const EdgeGeometry& geometry, const Vec2& start, const Vec2& end)
    {
        const auto result = edgeEndTangent(geometry, start, end);
        require(result.has_value(), "Expected a valid end derivative");
        return *result;
    }

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
            requireNear(startTangent(LineSegment{}, value[0], value[1]), value[2]);
            requireNear(endTangent(LineSegment{}, value[0], value[1]), value[2]);
        }
    }

    void zeroLengthSegmentReturnsZeroTangents()
    {
        for (const Vec2 position : {Vec2{}, Vec2{-2.5, 7.25}})
        {
            requireNear(startTangent(LineSegment{}, position, position), {});
            requireNear(endTangent(LineSegment{}, position, position), {});
        }
    }

    void bezierTangentsMatchKnownEndpointDerivatives()
    {
        const EdgeGeometry curve = CubicBezier{{3, 5}, {10, -4}};
        requireNear(startTangent(curve, {1, 2}, {14, 8}), {6, 9});
        requireNear(endTangent(curve, {1, 2}, {14, 8}), {12, 36});
    }

    void endpointTangentUsesOnlyAdjacentHandleAndEndpoint()
    {
        const EdgeGeometry startCurve = CubicBezier{{3, 5}, {-100, 200}};
        requireNear(startTangent(startCurve, {1, 2}, {-400, 500}), {6, 9});
        const EdgeGeometry endCurve = CubicBezier{{-100, 200}, {10, -4}};
        requireNear(endTangent(endCurve, {-400, 500}, {14, 8}), {12, 36});
    }

    void thirdsHandlesMatchSegmentDerivatives()
    {
        const Vec2 start{2, -1}, end{11, 11};
        const EdgeGeometry curve = CubicBezier{{5, 3}, {8, 7}};
        requireNear(startTangent(curve, start, end), {9, 12});
        requireNear(endTangent(curve, start, end), {9, 12});
        requireNear(startTangent(curve, start, end), startTangent(LineSegment{}, start, end));
        requireNear(endTangent(curve, start, end), endTangent(LineSegment{}, start, end));
    }

    void differentHandleLengthsRetainDerivativeMagnitudes()
    {
        const EdgeGeometry curve = CubicBezier{{2, 0}, {8, 0}};
        const Vec2 start = startTangent(curve, {0, 0}, {12, 0});
        const Vec2 end = endTangent(curve, {0, 0}, {12, 0});
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
            requireNear(startTangent(curve, start, end), (mask & 1) ? Vec2{} : Vec2{6, 9});
            requireNear(endTangent(curve, start, end), (mask & 2) ? Vec2{} : Vec2{12, 36});
        }
    }

    void reversalSwapsAndNegatesTangents()
    {
        const Vec2 start{1, 2}, end{14, 8};
        const EdgeGeometry curve = CubicBezier{{3, 5}, {10, -4}};
        const EdgeGeometry reversed = CubicBezier{{10, -4}, {3, 5}};
        requireNear(startTangent(reversed, end, start), endTangent(curve, start, end) * -1);
        requireNear(endTangent(reversed, end, start), startTangent(curve, start, end) * -1);
        requireNear(startTangent(LineSegment{}, end, start), endTangent(LineSegment{}, start, end) * -1);
        requireNear(endTangent(LineSegment{}, end, start), startTangent(LineSegment{}, start, end) * -1);
    }

    void tangentsRespectTranslationAndScaling()
    {
        const Vec2 start{1, 2}, end{14, 8}, first{3, 5}, second{10, -4};
        for (const double scale : {-3.5, 1e-90, 1.0, 1e90})
        {
            const EdgeGeometry curve = CubicBezier{first * scale, second * scale};
            requireNear(startTangent(curve, start * scale, end * scale), Vec2{6, 9} * scale);
            requireNear(endTangent(curve, start * scale, end * scale), Vec2{12, 36} * scale);
            requireNear(startTangent(LineSegment{}, start * scale, end * scale), Vec2{13, 6} * scale);
            requireNear(endTangent(LineSegment{}, start * scale, end * scale), Vec2{13, 6} * scale);
        }
        const Vec2 offset{-100.25, 200.5};
        const EdgeGeometry shifted = CubicBezier{first + offset, second + offset};
        requireNear(startTangent(shifted, start + offset, end + offset), {6, 9});
        requireNear(endTangent(shifted, start + offset, end + offset), {12, 36});
        requireNear(startTangent(LineSegment{}, start + offset, end + offset), {13, 6});
        requireNear(endTangent(LineSegment{}, start + offset, end + offset), {13, 6});
    }

    void arcTangentsDispatchToFullCircularDerivatives()
    {
        constexpr double pi = std::numbers::pi;
        for (const double sign : {-1.0, 1.0})
        {
            const EdgeGeometry arc = CircularArc{sign};
            requireNear(startTangent(arc, {0, 0}, {10, 0}), {0, -sign * 5 * pi});
            requireNear(endTangent(arc, {0, 0}, {10, 0}), {0, sign * 5 * pi});
        }
        const EdgeGeometry quarter = CircularArc{std::tan(pi / 8)};
        requireNear(startTangent(quarter, {1, 0}, {0, 1}), {0, pi / 2});
        requireNear(endTangent(quarter, {1, 0}, {0, 1}), {-pi / 2, 0});
        const EdgeGeometry major = CircularArc{-std::tan(3 * pi / 8)};
        requireNear(startTangent(major, {1, 0}, {0, 1}), {0, -3 * pi / 2});
        requireNear(endTangent(major, {1, 0}, {0, 1}), {3 * pi / 2, 0});
    }

    void invalidArcsReturnAbsentTangents()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        for (const double bulge : {0.0, nan, inf, -inf})
        {
            require(!edgeStartTangent(CircularArc{bulge}, {0, 0}, {10, 0}), "Invalid arc start accepted");
            require(!edgeEndTangent(CircularArc{bulge}, {0, 0}, {10, 0}), "Invalid arc end accepted");
        }
        for (const Vec2 end : {Vec2{}, Vec2{nan, 0}, Vec2{0, inf}})
        {
            require(!edgeStartTangent(CircularArc{}, {}, end), "Invalid arc endpoints accepted");
            require(!edgeEndTangent(CircularArc{}, {}, end), "Invalid arc endpoints accepted");
        }
        const double halfMaximum = std::numeric_limits<double>::max() / 2;
        require(!edgeStartTangent(CircularArc{}, {-halfMaximum, 0}, {halfMaximum, 0}),
            "Overflowed arc start derivative accepted");
        require(!edgeEndTangent(CircularArc{}, {-halfMaximum, 0}, {halfMaximum, 0}),
            "Overflowed arc end derivative accepted");
    }

    void nonFiniteRelevantInputsReturnAbsentTangents()
    {
        for (const double bad : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (const Vec2 position : {Vec2{bad, 0}, Vec2{0, bad}})
        {
            require(!edgeStartTangent(LineSegment{}, position, {}), "Invalid segment start accepted");
            require(!edgeEndTangent(LineSegment{}, {}, position), "Invalid segment end accepted");
            const EdgeGeometry badStart = CubicBezier{position, {8, 0}};
            const EdgeGeometry badEnd = CubicBezier{{2, 0}, position};
            require(!edgeStartTangent(badStart, {}, {10, 0}), "Invalid first handle accepted");
            require(!edgeEndTangent(badEnd, {}, {10, 0}), "Invalid second handle accepted");
            require(!edgeStartTangent(badEnd, position, {10, 0}), "Invalid start endpoint accepted");
            require(!edgeEndTangent(badStart, {}, position), "Invalid end endpoint accepted");
            // Each endpoint derivative depends only on its own endpoint and adjacent handle.
            requireNear(startTangent(badEnd, {}, position), {6, 0});
            requireNear(endTangent(badStart, position, {10, 0}), {6, 0});
        }
    }

    void finiteInputsWithOverflowReturnAbsentTangents()
    {
        const double maximum = std::numeric_limits<double>::max();
        require(!edgeStartTangent(LineSegment{}, {-maximum, 0}, {maximum, 0}), "Segment subtraction overflow accepted");
        require(!edgeEndTangent(LineSegment{}, {-maximum, 0}, {maximum, 0}), "Segment subtraction overflow accepted");
        const EdgeGeometry startOverflow = CubicBezier{{maximum / 2, 0}, {}};
        require(!edgeStartTangent(startOverflow, {}, {}), "Bezier multiplication overflow accepted");
        requireNear(endTangent(startOverflow, {}, {}), {});
        const EdgeGeometry endOverflow = CubicBezier{{}, {-maximum / 2, 0}};
        require(!edgeEndTangent(endOverflow, {}, {}), "Bezier multiplication overflow accepted");
        requireNear(startTangent(endOverflow, {}, {}), {});
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
            requireNear(startDerivative, startTangent(curve, points[0], points[3]), 1e-5);
            requireNear(endDerivative, endTangent(curve, points[0], points[3]), 1e-5);
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
            TestCase{"independent numerical endpoint derivatives", tangentsMatchIndependentNumericalDerivatives},
            TestCase{"arc dispatch preserves full circular derivatives", arcTangentsDispatchToFullCircularDerivatives},
            TestCase{"invalid arcs return absent tangents", invalidArcsReturnAbsentTangents},
            TestCase{"non-finite relevant inputs return absent tangents", nonFiniteRelevantInputsReturnAbsentTangents},
            TestCase{"finite inputs with overflow return absent tangents", finiteInputsWithOverflowReturnAbsentTangents}
        };
        return tests;
    }
}
