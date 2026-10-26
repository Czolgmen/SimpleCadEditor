#include <cad_core/geometry/Arc.h>
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
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
    const std::array tangentFunctions{cad::arcStartTangent, cad::arcEndTangent};
    const std::array bulgeFunctions{cad::arcBulgeFromStartTangent, cad::arcBulgeFromEndTangent};

    void requireBulge(std::optional<double> actual, double expected)
    {
        require(actual && std::isfinite(*actual) && *actual != 0.0, "Expected a finite nonzero bulge");
        require(std::abs(*actual / expected - 1.0) <= 2e-12, "Unexpected bulge or arc direction");
    }

    void tangentBulgesMatchKnownArcs()
    {
        const double root2 = std::sqrt(2.0);
        for (const double sign : {-1.0, 1.0})
        {
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {0, -sign}), sign);
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {0, sign}), sign);
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {1, -sign}), sign * (root2 - 1));
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {1, sign}), sign * (root2 - 1));
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {-1, -sign}), sign * (root2 + 1));
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {-1, sign}), sign * (root2 + 1));
        }
    }

    std::array<Vec2, 2> referenceDirections(Vec2 chord, double bulge)
    {
        // Independently rotate the chord by -/+ half the sweep, using the
        // rational sine/cosine identities for tan(sweep/4) = bulge.
        const double denominator = 1 + bulge * bulge;
        const double cosine = (1 - bulge * bulge) / denominator;
        const double sine = 2 * bulge / denominator;
        const Vec2 perpendicular{-chord.y, chord.x};
        return {chord * cosine - perpendicular * sine,
            chord * cosine + perpendicular * sine};
    }

    void tangentBulgesRecoverEveryChordAndSweep()
    {
        const Vec2 start{-4.5, 8.25};
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const auto tangents = referenceDirections(chord, bulge);
            for (std::size_t i = 0; i < bulgeFunctions.size(); ++i)
                requireBulge(bulgeFunctions[i](start, start + chord, tangents[i]), bulge);
        }
    }

    void tangentBulgesIgnorePositiveMagnitude()
    {
        for (const double scale : {std::numeric_limits<double>::denorm_min(),
            1e-300, 1e-150, 1.0, 1e150, 1e300, std::numeric_limits<double>::max()})
        {
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {scale, -scale}), std::sqrt(2.0) - 1);
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {scale, scale}), std::sqrt(2.0) - 1);
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {-scale, -scale}), std::sqrt(2.0) + 1);
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {-scale, scale}), std::sqrt(2.0) + 1);
        }
    }

    void tangentBulgesRespectGeometricTransforms()
    {
        const Vec2 start{-4, 3}, end{7, -2};
        for (const double bulge : {-2.0, -0.5, 0.5, 2.0})
        for (const double scale : {1e-200, 1.0, 1e200})
        for (const bool reflected : {false, true})
        {
            const auto transformVector = [&](Vec2 vector)
            {
                if (reflected) vector.x = -vector.x;
                return Vec2{-vector.y, vector.x} * scale;
            };
            const Vec2 offset{100 * scale, -300 * scale};
            const auto tangents = referenceDirections(end - start, bulge);
            for (std::size_t i = 0; i < bulgeFunctions.size(); ++i)
                requireBulge(bulgeFunctions[i](transformVector(start) + offset,
                    transformVector(end) + offset, transformVector(tangents[i])), reflected ? -bulge : bulge);
        }
    }

    void tangentBulgesRespectReversalAndTangentOrientation()
    {
        const Vec2 start{-4, 3}, end{7, -2};
        for (const double bulge : bulges)
        {
            const auto tangents = referenceDirections(end - start, bulge);
            for (std::size_t i = 0; i < bulgeFunctions.size(); ++i)
            {
                requireBulge(bulgeFunctions[1 - i](end, start, tangents[i] * -1), -bulge);
                // Reversing just the tangent selects the complementary arc.
                requireBulge(bulgeFunctions[i](start, end, tangents[i] * -1), -1 / bulge);
            }
        }
    }

    void tangentBulgesRejectInvalidCoordinates()
    {
        for (const auto function : bulgeFunctions)
        {
            require(!function({2, 3}, {2, 3}, {1, 1}), "Coincident endpoints accepted");
            require(!function({}, {10, 0}, {}), "Zero tangent accepted");
            for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
            for (int coordinate = 0; coordinate < 6; ++coordinate)
            {
                std::array points{Vec2{}, Vec2{10, 0}, Vec2{1, -1}};
                auto& point = points[coordinate / 2];
                (coordinate % 2 == 0 ? point.x : point.y) = invalid;
                require(!function(points[0], points[1], points[2]), "Non-finite coordinate accepted");
            }
        }
    }

    void tangentBulgesRejectBothParallelDirections()
    {
        const Vec2 start{-4.5, 8.25};
        for (const auto function : bulgeFunctions)
        for (const auto& chord : directions)
        for (const double scale : {-4.0, -1.0, 0.25, 4.0})
            require(!function(start, start + chord, chord * scale), "Parallel tangent accepted as a finite arc");
    }

    void tangentBulgesHandleNearParallelDirections()
    {
        // In particular, atan2 can round the major-arc angle to pi here.
        // The half-angle identity must retain the small nonzero transverse component.
        for (const double epsilon : {1e-12, 1e-50, 1e-150})
        for (const double sign : {-1.0, 1.0})
        {
            const double norm = std::hypot(1.0, epsilon);
            const double minor = sign * epsilon / (norm + 1);
            const double major = sign * (norm + 1) / epsilon;
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {1, -sign * epsilon}), minor);
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {1, sign * epsilon}), minor);
            requireBulge(cad::arcBulgeFromStartTangent({}, {10, 0}, {-1, -sign * epsilon}), major);
            requireBulge(cad::arcBulgeFromEndTangent({}, {10, 0}, {-1, sign * epsilon}), major);
        }
    }

    void tangentBulgesCheckCircleRepresentability()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const auto function : bulgeFunctions)
        {
            // Direct subtraction overflows, but the semicircle itself is representable.
            const Vec2 tangent = function == cad::arcBulgeFromStartTangent ? Vec2{0, -1} : Vec2{0, 1};
            requireBulge(function({-maximum * 0.75, 0}, {maximum * 0.75, 0}, tangent), 1);
            require(!function({-maximum, -maximum}, {maximum, maximum}, {1, -1}),
                "Unrepresentable circle radius accepted");
            // Finite inputs can demand an infinite bulge or a circle centre outside double.
            for (const double x : {-1.0, 1.0})
            for (const double y : {-1e-310, 1e-310})
                require(!function({}, {10, 0}, {x, y}), "Unrepresentable near-parallel arc accepted");
        }
    }

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

    void computedParametersMatchKnownCircles()
    {
        const double pi = std::numbers::pi_v<double>;
        for (const double bulge : {-1.0, 1.0})
        {
            const auto parameters = cad::computeArcParameters({0, 0}, {10, 0}, bulge);
            require(parameters.has_value(), "Semicircle parameters unavailable");
            requireNear(parameters->center, {5, 0}, 5);
            require(std::abs(parameters->radius - 5) <= 1e-12 &&
                std::abs(parameters->startAngle - pi) <= 1e-12 &&
                std::abs(parameters->sweep - bulge * pi) <= 1e-12,
                "Incorrect semicircle parameters");
        }
        for (const bool major : {false, true})
        {
            const double bulge = major ? -(std::sqrt(2.0) + 1) : std::sqrt(2.0) - 1;
            const auto parameters = cad::computeArcParameters({1, 0}, {0, 1}, bulge);
            require(parameters.has_value(), "General circle parameters unavailable");
            requireNear(parameters->center, {0, 0}, 1);
            require(std::abs(parameters->radius - 1) <= 1e-12 &&
                std::abs(parameters->startAngle) <= 1e-12 &&
                std::abs(parameters->sweep - (major ? -1.5 * pi : pi * 0.5)) <= 1e-12,
                "Incorrect minor or major circle parameters");
        }
    }

    void parameterErrorsReturnNulloptWithoutThrowing()
    {
        require(!cad::computeArcParameters({2, 3}, {2, 3}, 1), "Coincident ends have circle parameters");
        for (const double bulge : {0.0, -0.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max()})
            require(!cad::computeArcParameters({0, 0}, {10, 0}, bulge), "Invalid bulge has circle parameters");
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (int coordinate = 0; coordinate < 4; ++coordinate)
        {
            Vec2 start{0, 0}, end{10, 0};
            auto& point = coordinate < 2 ? start : end;
            (coordinate % 2 == 0 ? point.x : point.y) = invalid;
            require(!cad::computeArcParameters(start, end, 1), "Non-finite ends have circle parameters");
        }
        const double maximum = std::numeric_limits<double>::max();
        require(!cad::computeArcParameters({-maximum, -maximum}, {maximum, maximum}, 1),
            "Unrepresentable radius has circle parameters");
    }

    void semicircleTangentsMatchKnownDerivatives()
    {
        const double magnitude = 5 * std::numbers::pi_v<double>;
        for (const double bulge : {-1.0, 1.0})
        {
            const auto start = cad::arcStartTangent({0, 0}, {10, 0}, bulge);
            const auto end = cad::arcEndTangent({0, 0}, {10, 0}, bulge);
            require(start && end, "Semicircle tangent unavailable");
            requireNear(*start, {0, -bulge * magnitude}, magnitude);
            requireNear(*end, {0, bulge * magnitude}, magnitude);
        }
    }

    void minorAndMajorTangentsMatchKnownUnitCircles()
    {
        for (const bool major : {false, true})
        {
            const double bulge = major ? -(std::sqrt(2.0) + 1) : std::sqrt(2.0) - 1;
            const double sweep = (major ? -1.5 : 0.5) * std::numbers::pi_v<double>;
            const auto start = cad::arcStartTangent({1, 0}, {0, 1}, bulge);
            const auto end = cad::arcEndTangent({1, 0}, {0, 1}, bulge);
            require(start && end, "Minor or major tangent unavailable");
            requireNear(*start, {0, sweep}, std::abs(sweep));
            requireNear(*end, {-sweep, 0}, std::abs(sweep));
        }
    }

    void tangentsArePerpendicularWithCorrectMagnitudeAndDirection()
    {
        for (const auto& direction : directions)
        for (const double bulge : bulges)
        for (const double scale : {1e-90, 1.0, 1e90})
        {
            const Vec2 start{100 * scale, -300 * scale};
            const Vec2 chord = direction * scale;
            const Vec2 end = start + chord;
            const auto circle = referenceCircle(start, chord, bulge);
            const double expectedLength = circle.radius * std::abs(4 * std::atan(bulge));
            for (std::size_t i = 0; i < tangentFunctions.size(); ++i)
            {
                const auto tangent = tangentFunctions[i](start, end, bulge);
                require(tangent.has_value(), "Finite nondegenerate arc lost a tangent");
                const Vec2 radial = (i == 0 ? start : end) - circle.center;
                const Vec2 unitRadial{radial.x / circle.radius, radial.y / circle.radius};
                const Vec2 unitTangent{tangent->x / expectedLength, tangent->y / expectedLength};
                require(std::abs(std::hypot(unitTangent.x, unitTangent.y) - 1) <= 1e-10,
                    "Tangent was normalized or has the wrong derivative magnitude");
                require(std::abs(unitRadial.x * unitTangent.x + unitRadial.y * unitTangent.y) <= 1e-10,
                    "Tangent is not perpendicular to its radius");
                require((unitRadial.x * unitTangent.y - unitRadial.y * unitTangent.x) * bulge > 0,
                    "Tangent points against the arc traversal direction");
            }
        }
    }

    void reversingArcNegatesAndExchangesEndpointTangents()
    {
        const Vec2 start{-4.5, 8.25};
        for (const auto& chord : directions)
        for (const double bulge : bulges)
        {
            const Vec2 end = start + chord;
            const auto first = cad::arcStartTangent(start, end, bulge);
            const auto last = cad::arcEndTangent(start, end, bulge);
            const auto reversedFirst = cad::arcStartTangent(end, start, -bulge);
            const auto reversedLast = cad::arcEndTangent(end, start, -bulge);
            require(first && last && reversedFirst && reversedLast, "Reversed arc lost a tangent");
            const double magnitude = referenceCircle(start, chord, bulge).radius * std::abs(4 * std::atan(bulge));
            requireNear(*reversedFirst, *last * -1, magnitude);
            requireNear(*reversedLast, *first * -1, magnitude);
        }
    }

    void tangentsAgreeWithSampledEndpointDerivatives()
    {
        constexpr std::size_t steps = 4096;
        const Vec2 start{-4, 3}, end{7, -2};
        for (const double bulge : {-2.0, -0.5, 0.5, 2.0})
        {
            const auto points = sampleArc(start, end, bulge, steps);
            // Independent second-order one-sided differences for parameter t,
            // using the sampled curve rather than the radius-rotation formula.
            const Vec2 first = (points[0] * -3 + points[1] * 4 - points[2]) * (steps * 0.5);
            const Vec2 last = (points[steps] * 3 - points[steps - 1] * 4 + points[steps - 2]) * (steps * 0.5);
            const std::array expected{first, last};
            for (std::size_t i = 0; i < tangentFunctions.size(); ++i)
            {
                const auto actual = tangentFunctions[i](start, end, bulge);
                require(actual.has_value(), "Sampled curve lost its analytic tangent");
                const double magnitude = std::hypot(expected[i].x, expected[i].y);
                require(std::hypot((actual->x - expected[i].x) / magnitude,
                    (actual->y - expected[i].y) / magnitude) <= 2e-6,
                    "Tangent disagrees with the sampled curve derivative");
            }
        }
    }

    void invalidTangentInputsReturnNulloptWithoutThrowing()
    {
        for (const auto tangent : tangentFunctions)
        {
            require(!tangent({2, 3}, {2, 3}, 1), "Coincident arc ends have a tangent");
            for (const double bulge : {0.0, -0.0, std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min(),
                std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()})
                require(!tangent({0, 0}, {10, 0}, bulge), "Invalid arc parameters have a tangent");
            for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
            for (int coordinate = 0; coordinate < 4; ++coordinate)
            {
                Vec2 start{0, 0}, end{10, 0};
                auto& point = coordinate < 2 ? start : end;
                (coordinate % 2 == 0 ? point.x : point.y) = invalid;
                require(!tangent(start, end, 1), "Non-finite arc endpoint has a tangent");
            }
            const double maximum = std::numeric_limits<double>::max();
            require(!tangent({-maximum, -maximum}, {maximum, maximum}, 1),
                "Unrepresentable arc radius has a tangent");
        }
    }

    void tangentOverflowRejectsEvenWhenCircleParametersAreFinite()
    {
        const double halfMaximum = std::numeric_limits<double>::max() * 0.5;
        const Vec2 start{-halfMaximum, 0}, end{halfMaximum, 0};
        for (const double bulge : {-1.0, 1.0})
        {
            const auto parameters = cad::computeArcParameters(start, end, bulge);
            require(parameters && std::isfinite(parameters->radius), "Overflow fixture has invalid circle parameters");
            require(!cad::arcStartTangent(start, end, bulge) && !cad::arcEndTangent(start, end, bulge),
                "Non-finite derivative components were accepted");
        }
    }

    void tangentsRespectScaledRotationsAndReflections()
    {
        const Vec2 start{-4, 3}, end{7, -2};
        for (const double bulge : {-2.0, -0.5, 0.5, 2.0})
        for (const double scale : {1e-90, 1.0, 1e90})
        for (const bool reflected : {false, true})
        {
            const auto transformVector = [&](Vec2 vector)
            {
                if (reflected) vector.x = -vector.x;
                return Vec2{-vector.y, vector.x} * scale;
            };
            const Vec2 offset{100 * scale, -300 * scale};
            const Vec2 shiftedStart = transformVector(start) + offset;
            const Vec2 shiftedEnd = transformVector(end) + offset;
            for (const auto tangent : tangentFunctions)
            {
                const auto original = tangent(start, end, bulge);
                const auto actual = tangent(shiftedStart, shiftedEnd, reflected ? -bulge : bulge);
                require(original && actual, "Transformed arc lost a tangent");
                const double magnitude = std::hypot(original->x, original->y) * scale;
                requireNear(*actual, transformVector(*original), magnitude);
            }
        }
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
            TestCase{"unrepresentable circle centers reject", rejectsUnrepresentableCircleCenters},
            TestCase{"computed parameters match known circles", computedParametersMatchKnownCircles},
            TestCase{"parameter errors return nullopt without throwing", parameterErrorsReturnNulloptWithoutThrowing},
            TestCase{"semicircle tangents match known derivatives", semicircleTangentsMatchKnownDerivatives},
            TestCase{"minor and major tangents match known unit circles", minorAndMajorTangentsMatchKnownUnitCircles},
            TestCase{"tangents are perpendicular with correct magnitude and direction", tangentsArePerpendicularWithCorrectMagnitudeAndDirection},
            TestCase{"reversed arc negates and exchanges endpoint tangents", reversingArcNegatesAndExchangesEndpointTangents},
            TestCase{"tangents agree with sampled endpoint derivatives", tangentsAgreeWithSampledEndpointDerivatives},
            TestCase{"invalid tangent input returns nullopt without throwing", invalidTangentInputsReturnNulloptWithoutThrowing},
            TestCase{"tangent overflow rejects with finite circle parameters", tangentOverflowRejectsEvenWhenCircleParametersAreFinite},
            TestCase{"tangents respect scaled rotations and reflections", tangentsRespectScaledRotationsAndReflections},
            TestCase{"tangent bulges match known arcs", tangentBulgesMatchKnownArcs},
            TestCase{"tangent bulges recover every chord and sweep", tangentBulgesRecoverEveryChordAndSweep},
            TestCase{"tangent bulges ignore positive magnitude", tangentBulgesIgnorePositiveMagnitude},
            TestCase{"tangent bulges respect geometric transforms", tangentBulgesRespectGeometricTransforms},
            TestCase{"tangent bulges respect reversal and tangent orientation", tangentBulgesRespectReversalAndTangentOrientation},
            TestCase{"tangent bulges reject invalid coordinates", tangentBulgesRejectInvalidCoordinates},
            TestCase{"tangent bulges reject both parallel directions", tangentBulgesRejectBothParallelDirections},
            TestCase{"tangent bulges handle near-parallel directions", tangentBulgesHandleNearParallelDirections},
            TestCase{"tangent bulges check circle representability", tangentBulgesCheckCircleRepresentability}
        };
        return tests;
    }
}
