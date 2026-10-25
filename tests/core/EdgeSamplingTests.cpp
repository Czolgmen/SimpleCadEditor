#include <cad_core/model/EdgeSampling.h>

#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <stdexcept>

namespace
{
    using namespace cad;
    using cad::tests::require;

    void requirePoint(Vec2 actual, Vec2 expected)
    {
        require(std::isfinite(actual.x) && std::isfinite(actual.y) &&
            std::abs(actual.x - expected.x) < 1e-10 && std::abs(actual.y - expected.y) < 1e-10,
            "Wrong edge sample position");
    }

    void segmentReturnsOnlyItsEndpoints()
    {
        const EdgeGeometry geometry = LineSegment{};
        const Vec2 start{-2.5, 7.25}, end{11.75, -4};
        for (const std::size_t steps : {1, 4, 1000})
        {
            const auto points = sampleEdge(geometry, start, end, steps);
            require(points.size() == 2, "Segment sampling must return two endpoints");
            requirePoint(points[0], start);
            requirePoint(points[1], end);
        }
        require(sampleEdge(geometry, start, start).size() == 2, "Coincident segment endpoints were dropped");
    }

    void bezierUsesTheSharedDefaultResolution()
    {
        const EdgeGeometry geometry = CubicBezier{{0, 4}, {4, 4}};
        const auto points = sampleEdge(geometry, {0, 0}, {4, 0});
        require(points.size() == 101, "Wrong default edge sampling resolution");
        requirePoint(points.front(), {0, 0});
        requirePoint(points[50], {2, 3});
        requirePoint(points.back(), {4, 0});
    }

    void bezierUsesItsControlsAndRequestedResolution()
    {
        const EdgeGeometry geometry = CubicBezier{{0, 4}, {4, 4}};
        const auto points = sampleEdge(geometry, {0, 0}, {4, 0}, 4);
        require(points.size() == 5, "Requested resolution was ignored");
        requirePoint(points[1], {0.625, 2.25});
        requirePoint(points[2], {2, 3});
        requirePoint(points[3], {3.375, 2.25});
        const auto single = sampleEdge(geometry, {0, 0}, {4, 0}, 1);
        require(single.size() == 2, "One-segment Bezier sampling has wrong count");
        requirePoint(single.front(), {0, 0});
        requirePoint(single.back(), {4, 0});
    }

    void coincidentEndpointsKeepBezierInterior()
    {
        const EdgeGeometry geometry = CubicBezier{{10, 15}, {-10, 15}};
        const auto points = sampleEdge(geometry, {0, 0}, {0, 0}, 4);
        require(points.size() == 5, "Closed Bezier was treated as a segment");
        requirePoint(points.front(), {0, 0});
        requirePoint(points[2], {0, 11.25});
        requirePoint(points.back(), {0, 0});
    }

    void bezierRejectsZeroSteps()
    {
        bool thrown = false;
        try { (void)sampleEdge(CubicBezier{{0, 4}, {4, 4}}, {0, 0}, {4, 0}, 0); }
        catch (const std::invalid_argument&) { thrown = true; }
        require(thrown, "Bezier sampling did not propagate invalid_argument for zero steps");
    }
}

namespace cad::tests
{
    std::span<const TestCase> edgeSamplingTests()
    {
        static const std::array tests{
            TestCase{"segment returns only its endpoints", segmentReturnsOnlyItsEndpoints},
            TestCase{"Bezier uses shared default resolution", bezierUsesTheSharedDefaultResolution},
            TestCase{"Bezier uses controls and requested resolution", bezierUsesItsControlsAndRequestedResolution},
            TestCase{"coincident endpoints keep Bezier interior", coincidentEndpointsKeepBezierInterior},
            TestCase{"Bezier rejects zero steps", bezierRejectsZeroSteps}
        };
        return tests;
    }
}
