#include <cad_core/geometry/Arc.h>
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    using cad::Vec2;
    using cad::sampleArc;
    using cad::tests::require;

    void requireNear(Vec2 actual, Vec2 expected, double scale)
    {
        require(std::isfinite(actual.x) && std::isfinite(actual.y), "Arc returned a non-finite point");
        require(std::hypot((actual.x - expected.x) / scale, (actual.y - expected.y) / scale) <= 1e-10,
            "Arc sample differs from the expected geometry");
    }

    template<class Function>
    void requireInvalidArgument(Function function)
    {
        bool rejected = false;
        try { function(); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid arc input was not rejected with invalid_argument");
    }

    const std::array directions{Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}, Vec2{-10, 10},
        Vec2{-10, 0}, Vec2{-10, -10}, Vec2{0, -10}, Vec2{10, -10}};

    const std::array bulges{-8.0, -2.0, -1.0, -0.5, -0.125, 0.125, 0.5, 1.0, 2.0, 8.0};

    struct ReferenceCircle
    {
        Vec2 center;
        double radius;
    };

    ReferenceCircle referenceCircle(Vec2 start, Vec2 chord, double bulge)
    {
        // Standard circle identities, independent of the production half-chord factor.
        const double length = std::hypot(chord.x, chord.y);
        const Vec2 normal{-chord.y / length, chord.x / length};
        const double offset = length * (1 - bulge * bulge) / (4 * bulge);
        return {start + chord * 0.5 + normal * offset,
            length * (1 + bulge * bulge) / (4 * std::abs(bulge))};
    }

    void sampleCountAndExactEndpoints()
    {
        const Vec2 start{-7.5, 2.25};
        const Vec2 end{11.25, -9.5};
        for (const double bulge : bulges)
        for (const std::size_t steps : {1, 2, 3, 10, 100, 1024})
        {
            const auto points = sampleArc(start, end, bulge, steps);
            require(points.size() == steps + 1, "Arc must return steps+1 samples");
            require(points.front() == start && points.back() == end, "Arc endpoints must be exact");
            for (const auto& point : points)
                require(std::isfinite(point.x) && std::isfinite(point.y), "Arc has a non-finite sample");
        }
    }

    void oneStepReturnsBothEndpoints()
    {
        for (const double bulge : bulges)
        {
            const auto points = sampleArc({0, 0}, {10, 0}, bulge, 1);
            require(points.size() == 2 && points[0] == Vec2{0, 0} && points[1] == Vec2{10, 0},
                "One step must retain both distinct endpoints");
        }
    }

    void horizontalQuarterSamplesMatchKnownCoordinates()
    {
        const double diagonal = 5 * std::sqrt(0.5);
        for (const double bulge : {-1.0, 1.0})
        {
            const auto points = sampleArc({0, 0}, {10, 0}, bulge, 4);
            const std::array expected{Vec2{0, 0}, Vec2{5 - diagonal, -bulge * diagonal},
                Vec2{5, -bulge * 5}, Vec2{5 + diagonal, -bulge * diagonal}, Vec2{10, 0}};
            for (std::size_t i = 0; i < expected.size(); ++i)
                requireNear(points[i], expected[i], 5);
        }
    }

    void midpointMatchesPerpendicularChordInEveryDirection()
    {
        const Vec2 start{2.5, -3.75};
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const Vec2 end = start + chord;
            const Vec2 center = start + chord * 0.5;
            const Vec2 expected = center + Vec2{chord.y * 0.5, -chord.x * 0.5} * bulge;
            const auto points = sampleArc(start, end, bulge, 2);
            requireNear(points[1], expected, std::hypot(chord.x, chord.y) * 0.5);
        }
    }

    void everySampleLiesOnCircleAndChosenSide()
    {
        const Vec2 start{2.5, -3.75};
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const double length = std::hypot(chord.x, chord.y);
            const auto [center, radius] = referenceCircle(start, chord, bulge);
            const Vec2 middle = start + chord * 0.5;
            const auto points = sampleArc(start, start + chord, bulge, 73);
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const Vec2 radial = points[i] - center;
                require(std::abs(std::hypot(radial.x / radius, radial.y / radius) - 1) <= 1e-12,
                    "Arc sample left the expected circle");
                if (i == 0 || i + 1 == points.size()) continue;
                const Vec2 fromChord = points[i] - middle;
                const double side = (chord.x / length) * (fromChord.y / radius) -
                    (chord.y / length) * (fromChord.x / radius);
                require(side * bulge < 0, "Arc sample is on the wrong side of the chord");
            }
        }
    }

    void consecutiveSamplesHaveUniformSpacingAndCorrectWinding()
    {
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const Vec2 start{-8, 11};
            const auto [center, radius] = referenceCircle(start, chord, bulge);
            const auto points = sampleArc(start, start + chord, bulge, 32);
            const Vec2 firstStep = points[1] - points[0];
            const double spacing = std::hypot(firstStep.x, firstStep.y) / radius;
            for (std::size_t i = 1; i < points.size(); ++i)
            {
                const Vec2 step = points[i] - points[i - 1];
                require(std::abs(std::hypot(step.x, step.y) / radius - spacing) <= 1e-12,
                    "Arc samples have nonuniform spacing");
                const Vec2 a = points[i - 1] - center;
                const Vec2 b = points[i] - center;
                const double turn = (a.x / radius) * (b.y / radius) - (a.y / radius) * (b.x / radius);
                require(turn * bulge > 0, "Arc traverses the wrong direction");
            }
        }
    }

    void reversingEndpointsAndBulgeRetracesTheSameArc()
    {
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const Vec2 start{-4.5, 8.25};
            const Vec2 end = start + chord;
            const auto forward = sampleArc(start, end, bulge, 37);
            const auto reverse = sampleArc(end, start, -bulge, 37);
            const double radius = std::hypot(chord.x, chord.y) * 0.5;
            for (std::size_t i = 0; i < forward.size(); ++i)
                requireNear(forward[i], reverse[37 - i], radius);
        }
    }

    void samplesSupportScalingTranslationAndKeepInputs()
    {
        for (const double scale : {1e-90, 1e-6, 1.0, 1e6, 1e90})
        for (const double bulge : {-1.0, 1.0})
        {
            const Vec2 offset{100 * scale, -300 * scale};
            const Vec2 start = offset;
            const Vec2 end = offset + Vec2{10 * scale, 0};
            const Vec2 originalStart = start, originalEnd = end;
            const auto points = sampleArc(start, end, bulge, 4);
            const double diagonal = 5 * std::sqrt(0.5);
            const std::array expected{Vec2{0, 0}, Vec2{5 - diagonal, -bulge * diagonal},
                Vec2{5, -bulge * 5}, Vec2{5 + diagonal, -bulge * diagonal}, Vec2{10, 0}};
            for (std::size_t i = 0; i < expected.size(); ++i)
                requireNear(points[i], offset + expected[i] * scale, 5 * scale);
            require(start == originalStart && end == originalEnd, "Arc sampling changed its inputs");
            require(points.front() == start && points.back() == end, "Scaled endpoints are not exact");
        }
    }

    void rejectsZeroSteps()
    {
        for (const double bulge : {-1.0, 1.0})
            requireInvalidArgument([&] { sampleArc({0, 0}, {10, 0}, bulge, 0); });
    }

    void rejectsCoincidentEndpoints()
    {
        for (const Vec2 position : {Vec2{}, Vec2{2, -3}, Vec2{-1e90, 1e90}})
        for (const double bulge : {-1.0, 1.0})
            requireInvalidArgument([&] { sampleArc(position, position, bulge, 100); });
    }

    void rejectsZeroAndNonfiniteBulges()
    {
        for (const double bulge : {0.0, -0.0,
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity()})
            requireInvalidArgument([&] { sampleArc({0, 0}, {10, 0}, bulge, 100); });
    }

    void rejectsNonfiniteEndpointCoordinates()
    {
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (int coordinate = 0; coordinate < 4; ++coordinate)
        for (const double bulge : {-1.0, 1.0})
        {
            Vec2 start{0, 0}, end{10, 0};
            auto& point = coordinate < 2 ? start : end;
            (coordinate % 2 == 0 ? point.x : point.y) = invalid;
            requireInvalidArgument([&] { sampleArc(start, end, bulge, 100); });
        }
    }

    void rejectsUnrepresentableRadiusAndSamples()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const double bulge : {-1.0, 1.0})
            requireInvalidArgument([&] { sampleArc({-maximum, -maximum}, {maximum, maximum}, bulge, 4); });

        // The centre and radius fit in double, but this upper semicircle does not.
        const Vec2 start{-maximum * 0.75, maximum * 0.75};
        const Vec2 end{maximum * 0.75, maximum * 0.75};
        requireInvalidArgument([&] { sampleArc(start, end, -1, 4); });
        const auto lower = sampleArc(start, end, 1, 4);
        require(lower.front() == start && lower.back() == end, "Representable extreme arc lost its endpoints");
        for (const auto& point : lower)
            require(std::isfinite(point.x) && std::isfinite(point.y), "Representable extreme arc overflowed");
    }

    void quarterCircleMatchesKnownSamples()
    {
        const double root2 = std::sqrt(2.0);
        const double c = std::sqrt(2 + root2) * 0.5;
        const double s = std::sqrt(2 - root2) * 0.5;
        const double diagonal = std::sqrt(0.5);
        const std::array expected{Vec2{1, 0}, Vec2{c, s}, Vec2{diagonal, diagonal}, Vec2{s, c}, Vec2{0, 1}};
        const auto points = sampleArc({1, 0}, {0, 1}, root2 - 1, 4);
        for (std::size_t i = 0; i < expected.size(); ++i)
            requireNear(points[i], expected[i], 1);
    }

    void majorArcMatchesKnownThreeQuarterCircle()
    {
        const double diagonal = std::sqrt(0.5);
        const std::array expected{Vec2{1, 0}, Vec2{diagonal, -diagonal}, Vec2{0, -1},
            Vec2{-diagonal, -diagonal}, Vec2{-1, 0}, Vec2{-diagonal, diagonal}, Vec2{0, 1}};
        const auto points = sampleArc({1, 0}, {0, 1}, -(std::sqrt(2.0) + 1), 6);
        for (std::size_t i = 0; i < expected.size(); ++i)
            requireNear(points[i], expected[i], 1);
    }

    void generalArcsRespectScaledRotationsAndReflections()
    {
        const Vec2 start{-4, 3}, end{7, -2};
        for (const double bulge : {-2.0, -0.5, 0.5, 2.0})
        for (const double scale : {1e-90, 1e-6, 1.0, 1e6, 1e90})
        for (const bool reflected : {false, true})
        {
            const Vec2 offset{100 * scale, -300 * scale};
            const auto transform = [&](Vec2 point)
            {
                if (reflected) point.x = -point.x;
                return Vec2{-point.y, point.x} * scale + offset;
            };
            const auto reference = sampleArc(start, end, bulge, 37);
            const Vec2 transformedStart = transform(start), transformedEnd = transform(end);
            const auto actual = sampleArc(transformedStart, transformedEnd, reflected ? -bulge : bulge, 37);
            require(actual.front() == transformedStart && actual.back() == transformedEnd,
                "General arc lost exact transformed endpoints");
            for (std::size_t i = 0; i < actual.size(); ++i)
                requireNear(actual[i], transform(reference[i]), 10 * scale);
        }
    }

    void rejectsUnrepresentableCircleCenters()
    {
        for (const double bulge : {std::numeric_limits<double>::denorm_min(),
            -std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max()})
            requireInvalidArgument([&] { sampleArc({0, 0}, {10, 0}, bulge, 4); });
    }

    void rejectsUnrepresentableSampleCountBeforeAllocation()
    {
        const auto capacity = std::vector<Vec2>{}.max_size();
        for (const auto steps : {capacity, std::numeric_limits<std::size_t>::max()})
        {
            bool rejected = false;
            try { sampleArc({0, 0}, {10, 0}, 1, steps); }
            catch (const std::length_error&) { rejected = true; }
            require(rejected, "Unrepresentable sample count was not rejected with length_error");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> arcTests()
    {
        static const std::array tests{
            TestCase{"sample count and exact endpoints", sampleCountAndExactEndpoints},
            TestCase{"one step returns both endpoints", oneStepReturnsBothEndpoints},
            TestCase{"horizontal quarter samples match known coordinates", horizontalQuarterSamplesMatchKnownCoordinates},
            TestCase{"midpoint matches perpendicular chord in every direction", midpointMatchesPerpendicularChordInEveryDirection},
            TestCase{"every sample lies on circle and chosen side", everySampleLiesOnCircleAndChosenSide},
            TestCase{"uniform spacing and correct winding", consecutiveSamplesHaveUniformSpacingAndCorrectWinding},
            TestCase{"reversed endpoints and sign retrace the same arc", reversingEndpointsAndBulgeRetracesTheSameArc},
            TestCase{"scaling translation and input preservation", samplesSupportScalingTranslationAndKeepInputs},
            TestCase{"zero steps reject", rejectsZeroSteps},
            TestCase{"coincident endpoints reject", rejectsCoincidentEndpoints},
            TestCase{"zero and nonfinite bulges reject", rejectsZeroAndNonfiniteBulges},
            TestCase{"nonfinite endpoint coordinates reject", rejectsNonfiniteEndpointCoordinates},
            TestCase{"unrepresentable radius and samples reject", rejectsUnrepresentableRadiusAndSamples},
            TestCase{"unrepresentable sample count rejects before allocation", rejectsUnrepresentableSampleCountBeforeAllocation},
            TestCase{"quarter circle matches known samples", quarterCircleMatchesKnownSamples},
            TestCase{"major arc matches known three-quarter circle", majorArcMatchesKnownThreeQuarterCircle},
            TestCase{"general arcs respect scaled rotations and reflections", generalArcsRespectScaledRotationsAndReflections},
            TestCase{"unrepresentable circle centers reject", rejectsUnrepresentableCircleCenters}
        };
        return tests;
    }
}
