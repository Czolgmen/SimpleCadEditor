#include <cad_core/geometry/Bezier.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace
{
    using cad::Vec2;
    using cad::tests::require;
    using Curve = std::array<Vec2, 4>;

    const std::array curves{
        Curve{Vec2{0, 0}, Vec2{0, 4}, Vec2{4, 4}, Vec2{4, 0}},
        Curve{Vec2{-7.5, 2.25}, Vec2{13, -18}, Vec2{-11, 25}, Vec2{4.5, -3}},
        Curve{Vec2{0, 0}, Vec2{2, 3}, Vec2{4, 6}, Vec2{6, 9}},
        Curve{Vec2{0, 0}, Vec2{10, 15}, Vec2{-10, 15}, Vec2{0, 0}},
        Curve{Vec2{2, -3}, Vec2{2, -3}, Vec2{2, -3}, Vec2{2, -3}},
        Curve{Vec2{0, 0}, Vec2{20, 0}, Vec2{-10, 0}, Vec2{3, 0}},
        Curve{Vec2{0, 0}, Vec2{0, 0}, Vec2{4, 7}, Vec2{4, 7}}
    };

    Vec2 point(const Curve& curve, double t)
    {
        return cad::bezierPoint(curve[0], curve[1], curve[2], curve[3], t);
    }

    std::vector<Vec2> sample(const Curve& curve, std::size_t steps)
    {
        return cad::sampleBezier(curve[0], curve[1], curve[2], curve[3], steps);
    }

    // Independent scalar Bernstein evaluation: no power coefficients or forward differences.
    Vec2 bernstein(const Curve& curve, double t)
    {
        const double u = 1.0 - t;
        const std::array weights{u * u * u, 3.0 * u * u * t, 3.0 * u * t * t, t * t * t};
        Vec2 result{0, 0};
        for (std::size_t i = 0; i < curve.size(); ++i)
        {
            result.x += weights[i] * curve[i].x;
            result.y += weights[i] * curve[i].y;
        }
        return result;
    }

    double coordinateScale(const Curve& curve)
    {
        double scale = 0;
        for (const auto& control : curve)
            scale = (std::max)({scale, std::abs(control.x), std::abs(control.y)});
        return scale == 0 ? 1.0 : scale;
    }

    void requireNear(Vec2 actual, Vec2 expected, double scale)
    {
        const double tolerance = 1e-10 * scale;
        require(std::isfinite(actual.x) && std::isfinite(actual.y) &&
            std::isfinite(expected.x) && std::isfinite(expected.y), "Non-finite Bezier result");
        require(std::abs(actual.x - expected.x) <= tolerance && std::abs(actual.y - expected.y) <= tolerance,
            "Bezier point differs from the independent expected value");
    }

    void requireExact(Vec2 actual, Vec2 expected)
    {
        require(actual.x == expected.x && actual.y == expected.y, "Bezier endpoint must be exact");
    }

    Curve transformed(Curve curve, double scale, Vec2 offset = {})
    {
        for (auto& control : curve) control = control * scale + offset;
        return curve;
    }

    void pointReturnsExactEndpoints()
    {
        const Curve fractional{Vec2{0.1, -0.2}, Vec2{1.75, 8.125}, Vec2{-3.3, 0.7}, Vec2{4.2, -5.6}};
        for (const double scale : {1e-90, 1.0, 1e90})
        {
            const Curve curve = transformed(fractional, scale);
            requireExact(point(curve, 0.0), curve.front());
            requireExact(point(curve, -0.0), curve.front());
            requireExact(point(curve, 1.0), curve.back());
        }
    }

    void pointMatchesKnownInteriorValues()
    {
        requireNear(point(curves[0], 0.25), {0.625, 2.25}, 1);
        requireNear(point(curves[0], 0.5), {2, 3}, 1);
        requireNear(point(curves[0], 0.75), {3.375, 2.25}, 1);
    }

    void pointMatchesBernsteinAcrossShapes()
    {
        for (const auto& curve : curves)
            for (const double t : {1e-8, 0.01, 0.125, 0.3, 0.5, 0.8, 0.99999999})
                requireNear(point(curve, t), bernstein(curve, t), coordinateScale(curve));
    }

    void pointOnUniformLineIsLinear()
    {
        for (const double t : {0.0, 0.1, 0.4, 0.7, 1.0})
            requireNear(point(curves[2], t), {6 * t, 9 * t}, 9);
    }

    void pointWithIdenticalControlsIsConstant()
    {
        for (const Vec2 position : {Vec2{}, Vec2{2, -3}, Vec2{-0.125, 0.75}})
        {
            const Curve curve{position, position, position, position};
            for (const double t : {0.0, 0.13, 0.5, 0.91, 1.0}) requireExact(point(curve, t), position);
        }
    }

    void pointReversesWithControlOrder()
    {
        for (const auto& curve : curves)
        {
            Curve reversed = curve;
            std::reverse(reversed.begin(), reversed.end());
            for (const double t : {0.0, 0.13, 0.5, 0.81, 1.0})
                requireNear(point(curve, t), point(reversed, 1.0 - t), coordinateScale(curve));
        }
    }

    void pointSupportsScalesAndTranslation()
    {
        const Curve& base = curves[1];
        for (const double scale : {-3.5, 1e-90, 1e-6, 1.0, 1e6, 1e90})
        {
            const Curve curve = transformed(base, scale);
            for (const double t : {0.0, 0.125, 0.4, 0.75, 1.0})
                requireNear(point(curve, t), bernstein(curve, t), coordinateScale(curve));
        }
        const Vec2 offset{100.25, -50.5};
        const Curve shifted = transformed(base, 1, offset);
        for (const double t : {0.1, 0.5, 0.9})
            requireNear(point(shifted, t), point(base, t) + offset, coordinateScale(shifted));
    }

    void samplesHaveExpectedCountAndExactEndpoints()
    {
        for (const auto& base : curves)
        for (const double scale : {1e-90, 1.0, 1e90})
        for (const std::size_t steps : {1, 2, 3, 7, 16, 100, 1024})
        {
            const Curve curve = transformed(base, scale);
            const auto points = sample(curve, steps);
            require(points.size() == steps + 1, "Wrong number of Bezier samples");
            requireExact(points.front(), curve.front());
            requireExact(points.back(), curve.back());
            if (steps == 1) require(points.size() == 2, "One segment must contain only its endpoints");
        }
    }

    void samplesMatchKnownQuarterPoints()
    {
        const auto points = sample(curves[0], 4);
        require(points.size() == 5, "Four segments need five points");
        requireNear(points[1], {0.625, 2.25}, 1);
        requireNear(points[2], {2, 3}, 1);
        requireNear(points[3], {3.375, 2.25}, 1);
    }

    void samplesMatchBernsteinAtEveryStep()
    {
        for (const auto& curve : curves)
        for (const std::size_t steps : {2, 3, 7, 31, 100, 1024})
        {
            const auto points = sample(curve, steps);
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(steps);
                requireNear(points[i], bernstein(curve, t), coordinateScale(curve));
            }
        }
    }

    void samplesOnUniformLineAreEvenlySpaced()
    {
        for (const std::size_t steps : {1, 3, 10, 100})
        {
            const auto points = sample(curves[2], steps);
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(steps);
                requireNear(points[i], {6 * t, 9 * t}, 9);
            }
        }
    }

    void samplesWithIdenticalControlsStayConstant()
    {
        for (const Vec2 position : {Vec2{}, Vec2{2, -3}, Vec2{-0.125, 0.75}})
        {
            const Curve curve{position, position, position, position};
            for (const auto& value : sample(curve, 100)) requireExact(value, position);
        }
    }

    void coincidentEndpointsDoNotCollapseTheCurve()
    {
        const auto points = sample(curves[3], 4);
        requireExact(points.front(), points.back());
        requireNear(points[2], {0, 11.25}, 15);
        require(points[1].x > 0 && points[3].x < 0, "Closed curve lost its interior or traversal order");
    }

    void samplesReverseWithControlOrder()
    {
        for (const auto& curve : curves)
        for (const std::size_t steps : {1, 7, 100})
        {
            Curve reversed = curve;
            std::reverse(reversed.begin(), reversed.end());
            const auto forward = sample(curve, steps);
            const auto backward = sample(reversed, steps);
            for (std::size_t i = 0; i < forward.size(); ++i)
                requireNear(forward[i], backward[steps - i], coordinateScale(curve));
        }
    }

    void samplesSupportScalesAndTranslation()
    {
        const Curve& base = curves[1];
        for (const double scale : {-3.5, 1e-90, 1e-6, 1.0, 1e6, 1e90})
        {
            const Curve curve = transformed(base, scale);
            const auto points = sample(curve, 127);
            for (std::size_t i = 0; i < points.size(); ++i)
                requireNear(points[i], bernstein(curve, static_cast<double>(i) / 127.0), coordinateScale(curve));
        }
        const Vec2 offset{100.25, -50.5};
        const Curve shifted = transformed(base, 1, offset);
        const auto original = sample(base, 100);
        const auto translated = sample(shifted, 100);
        for (std::size_t i = 0; i < original.size(); ++i)
            requireNear(translated[i], original[i] + offset, coordinateScale(shifted));
    }

    void splitHalvesPreserveParameterizationAcrossShapesAndScales()
    {
        for (const auto& base : curves)
        for (const double scale : {1e-90, 1.0, 1e90})
        {
            const Curve curve = transformed(base, scale, {100 * scale, -30 * scale});
            const auto split = cad::splitBezierHalf(curve[0], curve[1], curve[2], curve[3]);
            requireExact(split.left[0], curve[0]);
            requireExact(split.right[3], curve[3]);
            requireExact(split.left[3], split.right[0]);
            for (int i = 0; i <= 32; ++i)
            {
                const double t = i / 32.0;
                requireNear(bernstein(split.left, t), bernstein(curve, t * 0.5), coordinateScale(curve));
                requireNear(bernstein(split.right, t), bernstein(curve, (t + 1) * 0.5), coordinateScale(curve));
            }
        }
    }

    void splitMatchesKnownControlsAndRescalesEndpointDerivatives()
    {
        const Curve curve{Vec2{0, 0}, Vec2{0, 8}, Vec2{8, 8}, Vec2{8, 0}};
        const auto split = cad::splitBezierHalf(curve[0], curve[1], curve[2], curve[3]);
        const Curve left{Vec2{0, 0}, Vec2{0, 4}, Vec2{2, 6}, Vec2{4, 6}};
        const Curve right{Vec2{4, 6}, Vec2{6, 6}, Vec2{8, 4}, Vec2{8, 0}};
        for (std::size_t i = 0; i < 4; ++i)
        {
            requireExact(split.left[i], left[i]);
            requireExact(split.right[i], right[i]);
        }
        requireExact((split.left[1] - split.left[0]) * 3, (curve[1] - curve[0]) * 1.5);
        requireExact((split.right[3] - split.right[2]) * 3, (curve[3] - curve[2]) * 1.5);
        requireExact((split.left[3] - split.left[2]) * 3, (split.right[1] - split.right[0]) * 3);
    }

    void splitAvoidsOverflowAtFiniteExtremeCoordinates()
    {
        const double maximum = std::numeric_limits<double>::max();
        const std::array extremes{
            Curve{Vec2{maximum, -maximum}, Vec2{maximum, -maximum},
                Vec2{maximum, -maximum}, Vec2{maximum, -maximum}},
            Curve{Vec2{maximum, maximum}, Vec2{-maximum, maximum},
                Vec2{maximum, -maximum}, Vec2{-maximum, -maximum}}};
        for (const auto& curve : extremes)
        {
            const auto split = cad::splitBezierHalf(curve[0], curve[1], curve[2], curve[3]);
            for (const auto& half : {split.left, split.right})
            for (const auto& point : half)
                require(std::isfinite(point.x) && std::isfinite(point.y), "Finite split overflowed");
            requireExact(split.left[0], curve[0]);
            requireExact(split.right[3], curve[3]);
            requireExact(split.left[3], split.right[0]);
        }
    }

    void zeroStepsThrowInvalidArgument()
    {
        for (const auto& curve : curves)
        {
            bool thrown = false;
            try { (void)sample(curve, 0); }
            catch (const std::invalid_argument&) { thrown = true; }
            require(thrown, "Zero steps must throw std::invalid_argument");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> bezierTests()
    {
        static const std::array tests{
            TestCase{"point has exact endpoints", pointReturnsExactEndpoints},
            TestCase{"point matches known interior values", pointMatchesKnownInteriorValues},
            TestCase{"point matches independent Bernstein evaluation", pointMatchesBernsteinAcrossShapes},
            TestCase{"point on uniform straight line", pointOnUniformLineIsLinear},
            TestCase{"point with identical controls is constant", pointWithIdenticalControlsIsConstant},
            TestCase{"point reverses with control order", pointReversesWithControlOrder},
            TestCase{"point supports scales and translation", pointSupportsScalesAndTranslation},
            TestCase{"sample count and exact endpoints", samplesHaveExpectedCountAndExactEndpoints},
            TestCase{"samples match known quarter points", samplesMatchKnownQuarterPoints},
            TestCase{"every sample matches independent Bernstein evaluation", samplesMatchBernsteinAtEveryStep},
            TestCase{"uniform line samples are evenly spaced", samplesOnUniformLineAreEvenlySpaced},
            TestCase{"identical controls give constant samples", samplesWithIdenticalControlsStayConstant},
            TestCase{"coincident endpoints retain the interior curve", coincidentEndpointsDoNotCollapseTheCurve},
            TestCase{"samples reverse with control order", samplesReverseWithControlOrder},
            TestCase{"samples support scales and translation", samplesSupportScalesAndTranslation},
            TestCase{"zero sample steps throw invalid_argument", zeroStepsThrowInvalidArgument},
            TestCase{"split halves preserve parameterization across shapes and scales", splitHalvesPreserveParameterizationAcrossShapesAndScales},
            TestCase{"split matches known controls and rescales endpoint derivatives", splitMatchesKnownControlsAndRescalesEndpointDerivatives},
            TestCase{"split avoids overflow at finite extreme coordinates", splitAvoidsOverflowAtFiniteExtremeCoordinates}
        };
        return tests;
    }
}
