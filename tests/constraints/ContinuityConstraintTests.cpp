#include <cad_core/constraints/ContinuityConstraint.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
    using namespace cad;
    using cad::constraint::continuityErrors;
    using cad::tests::require;
    constexpr double pi = std::numbers::pi_v<double>;

    double angle(Vec2 incoming, Vec2 outgoing)
    {
        const auto result = continuityErrors(Continuity::G1, incoming, outgoing);
        require(result && result->size() == 1, "G1 must return exactly one angular residual");
        require(std::isfinite((*result)[0]), "G1 returned a non-finite angular residual");
        return (*result)[0];
    }

    void requireAngle(double actual, double expected)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= 1e-12, "Wrong signed angle in radians");
    }

    void requireC1(Vec2 incoming, Vec2 outgoing, Vec2 expected)
    {
        const auto result = continuityErrors(Continuity::C1, incoming, outgoing);
        require(result && result->size() == 2, "C1 must return exactly two derivative components");
        double scale = (std::max)(std::abs(expected.x), std::abs(expected.y));
        if (scale == 0) scale = 1;
        require(std::isfinite((*result)[0]) && std::isfinite((*result)[1]) &&
            std::abs((*result)[0] - expected.x) <= 1e-12 * scale &&
            std::abs((*result)[1] - expected.y) <= 1e-12 * scale, "Wrong C1 residual values or component order");
    }

    void g0ReturnsPresentEmptyVector()
    {
        for (const Vec2 incoming : {Vec2{}, Vec2{3, 4}})
        for (const Vec2 outgoing : {Vec2{}, Vec2{-4, 3}})
        {
            const auto result = continuityErrors(Continuity::G0, incoming, outgoing);
            require(result.has_value() && result->empty(), "G0 must return an empty vector rather than nullopt");
        }
    }

    void g1AlignedDirectionsHaveZeroError()
    {
        for (const Vec2 incoming : {Vec2{1, 0}, Vec2{0, -2}, Vec2{3, 4}, Vec2{-5, 7}})
            requireAngle(angle(incoming, incoming * 5), 0);
    }

    void g1KnownAnglesHaveCorrectSignsAndUnits()
    {
        requireAngle(angle({1, 0}, {0, 1}), pi / 2);
        requireAngle(angle({1, 0}, {0, -1}), -pi / 2);
        requireAngle(angle({0, 1}, {1, 0}), -pi / 2);
        requireAngle(angle({1, 0}, {1, 1}), pi / 4);
        requireAngle(angle({1, 0}, {-1, 1}), 3 * pi / 4);
        requireAngle(angle({1, 0}, {-1, -1}), -3 * pi / 4);
    }

    void g1OppositeDirectionsAreNotSatisfied()
    {
        for (const Vec2 incoming : {Vec2{1, 0}, Vec2{0, -1}, Vec2{3, 4}})
            requireAngle(std::abs(angle(incoming, incoming * -2)), pi);
        constexpr double epsilon = 1e-6;
        requireAngle(angle({1, 0}, {-1, epsilon}), pi - std::atan(epsilon));
        requireAngle(angle({1, 0}, {-1, -epsilon}), -pi + std::atan(epsilon));
    }

    void g1SmallNonzeroAngleIsPreserved()
    {
        for (const double slope : {-1e-9, 1e-9})
        {
            const double actual = angle({1, 0}, {1, slope});
            requireAngle(actual, std::atan(slope));
            require(actual != 0 && actual * slope > 0, "Small G1 error was rounded to zero or lost its sign");
        }
    }

    void g1IndependentPositiveScalesDoNotChangeAngle()
    {
        for (const double incomingScale : {1e-200, 1e-12, 1.0, 1e200})
        for (const double outgoingScale : {1e-200, 1e-12, 1.0, 1e200})
            requireAngle(angle(Vec2{3, 4} * incomingScale, Vec2{-4, 3} * outgoingScale), pi / 2);
    }

    void g1RejectsEitherZeroDirection()
    {
        require(!continuityErrors(Continuity::G1, {}, {3, 4}), "Zero incoming direction accepted");
        require(!continuityErrors(Continuity::G1, {3, 4}, {}), "Zero outgoing direction accepted");
        require(!continuityErrors(Continuity::G1, {}, {}), "Two zero directions accepted");
        require(!continuityErrors(Continuity::G1, {-0.0, 0.0}, {3, 4}), "Signed zero direction accepted");
    }

    void g1AndC1RejectNonFiniteTangents()
    {
        for (const auto type : {Continuity::G1, Continuity::C1})
        for (const bool incoming : {false, true})
        for (const bool x : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Vec2 a{3, 4}, b{5, 6};
            auto& damaged = incoming ? a : b;
            (x ? damaged.x : damaged.y) = invalid;
            require(!continuityErrors(type, a, b), "Non-finite tangent produced a residual vector");
        }
    }

    void g1RejectsUnrepresentableTangentNorm()
    {
        const double maximum = std::numeric_limits<double>::max();
        require(!continuityErrors(Continuity::G1, {maximum, maximum}, {1, 0}), "Incoming norm overflow accepted");
        require(!continuityErrors(Continuity::G1, {1, 0}, {maximum, maximum}), "Outgoing norm overflow accepted");
    }

    void c1EqualDerivativesIncludingZeroAreSatisfied()
    {
        for (const Vec2 derivative : {Vec2{}, Vec2{3, 4}, Vec2{-5.25, 7.5}})
            requireC1(derivative, derivative, {});
    }

    void c1ReturnsSignedXYDifferenceInOrder()
    {
        requireC1({2, -3}, {-5, 7}, {-7, 10});
        requireC1({-5, 7}, {2, -3}, {7, -10});
    }

    void c1PreservesMagnitudeDifferencesIgnoredByG1()
    {
        requireAngle(angle({3, 4}, {6, 8}), 0);
        requireC1({3, 4}, {6, 8}, {3, 4});
    }

    void c1AllowsOneZeroDerivative()
    {
        requireC1({}, {3, -4}, {3, -4});
        requireC1({3, -4}, {}, {-3, 4});
    }

    void c1RejectsOverflowInEitherDifferenceComponent()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool x : {false, true})
        for (const double sign : {-1.0, 1.0})
        {
            const Vec2 incoming = x ? Vec2{sign * maximum, 1} : Vec2{1, sign * maximum};
            const Vec2 outgoing = x ? Vec2{-sign * maximum, 1} : Vec2{1, -sign * maximum};
            require(!continuityErrors(Continuity::C1, incoming, outgoing), "Derivative difference overflow accepted");
        }
    }

    void c1SupportsTinyAndLargeResiduals()
    {
        for (const double scale : {1e-200, 1e-12, 1.0, 1e200})
            requireC1(Vec2{2, -3} * scale, Vec2{5, 7} * scale, Vec2{3, 10} * scale);
    }

    void invalidContinuityClassReturnsNullopt()
    {
        for (const auto value : {static_cast<Continuity>(-1), static_cast<Continuity>(3), static_cast<Continuity>(999)})
            require(!continuityErrors(value, {3, 4}, {6, 8}), "Invalid continuity enum produced residuals");
    }
}

namespace cad::tests
{
    std::span<const TestCase> continuityConstraintTests()
    {
        static const std::array tests{
            TestCase{"G0 returns a present empty vector", g0ReturnsPresentEmptyVector},
            TestCase{"G1 aligned directions have zero error", g1AlignedDirectionsHaveZeroError},
            TestCase{"G1 known angles have correct signs and radians", g1KnownAnglesHaveCorrectSignsAndUnits},
            TestCase{"G1 opposite directions are not satisfied", g1OppositeDirectionsAreNotSatisfied},
            TestCase{"G1 small nonzero angle is preserved", g1SmallNonzeroAngleIsPreserved},
            TestCase{"G1 ignores independent positive scales", g1IndependentPositiveScalesDoNotChangeAngle},
            TestCase{"G1 rejects either zero direction", g1RejectsEitherZeroDirection},
            TestCase{"G1 and C1 reject non-finite tangents", g1AndC1RejectNonFiniteTangents},
            TestCase{"G1 rejects an unrepresentable norm", g1RejectsUnrepresentableTangentNorm},
            TestCase{"C1 equal derivatives including zero are satisfied", c1EqualDerivativesIncludingZeroAreSatisfied},
            TestCase{"C1 returns signed XY difference in order", c1ReturnsSignedXYDifferenceInOrder},
            TestCase{"C1 retains magnitude differences ignored by G1", c1PreservesMagnitudeDifferencesIgnoredByG1},
            TestCase{"C1 permits one zero derivative", c1AllowsOneZeroDerivative},
            TestCase{"C1 rejects overflow in either component", c1RejectsOverflowInEitherDifferenceComponent},
            TestCase{"C1 supports tiny and large residuals", c1SupportsTinyAndLargeResiduals},
            TestCase{"invalid continuity class returns nullopt", invalidContinuityClassReturnsNullopt}
        };
        return tests;
    }
}
