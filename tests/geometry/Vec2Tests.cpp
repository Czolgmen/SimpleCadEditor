#include <cad_core/geometry/Vec2.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>

namespace
{
    using cad::Vec2;
    using cad::cross;
    using cad::dot;
    using cad::tests::require;

    // Arithmetic and geometry operations must be callable on const vectors.
    static_assert(std::is_same_v<
        decltype(std::declval<const Vec2&>() + std::declval<const Vec2&>()), Vec2>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Vec2&>() - std::declval<const Vec2&>()), Vec2>);
    static_assert(std::is_same_v<decltype(std::declval<const Vec2&>() * 2.0), Vec2>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Vec2&>() / 2.0), std::optional<Vec2>>);
    static_assert(std::is_same_v<decltype(std::declval<const Vec2&>().Length()), double>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Vec2&>().Normalized()), std::optional<Vec2>>);

    void requireNear(double actual, double expected, std::string_view context)
    {
        constexpr double absoluteTolerance = 1e-12;
        constexpr double relativeTolerance = 1e-12;
        const double tolerance = absoluteTolerance +
            relativeTolerance * std::max(std::abs(actual), std::abs(expected));

        if (!std::isfinite(actual) || !std::isfinite(expected) ||
            std::abs(actual - expected) > tolerance)
        {
            std::ostringstream message;
            message << context << ": expected " << std::setprecision(17)
                << expected << ", got " << actual;
            throw std::runtime_error(message.str());
        }
    }

    void requireVectorNear(const Vec2& actual, const Vec2& expected)
    {
        requireNear(actual.x, expected.x, "X coordinate");
        requireNear(actual.y, expected.y, "Y coordinate");
    }

    void defaultConstructorCreatesZero()
    {
        const Vec2 vector;
        require(vector.x == 0.0 && vector.y == 0.0, "Default vector must be exactly zero");
    }

    void constructorPreservesCoordinates()
    {
        const Vec2 vector{-2.75, 4.125};
        require(vector.x == -2.75 && vector.y == 4.125, "Constructor changed coordinates");
    }

    void addsVectors()
    {
        const Vec2 first{1.25, -2.5};
        const Vec2 second{3.5, 4.75};
        requireVectorNear(first + second, Vec2{4.75, 2.25});
        requireVectorNear(second + first, Vec2{4.75, 2.25});
    }

    void subtractsVectors()
    {
        const Vec2 first{1.25, -2.5};
        const Vec2 second{3.5, 4.75};
        requireVectorNear(first - second, Vec2{-2.25, -7.25});
        requireVectorNear(second - first, Vec2{2.25, 7.25});
        requireVectorNear(first - first, Vec2{});
    }

    void multipliesByScalar()
    {
        const Vec2 vector{2.5, -3.0};
        requireVectorNear(vector * 2.0, Vec2{5.0, -6.0});
        requireVectorNear(vector * -0.5, Vec2{-1.25, 1.5});
        requireVectorNear(vector * 0.0, Vec2{});
    }

    void dividesByNonzeroScalar()
    {
        const Vec2 vector{-7.5, 3.0};
        const auto positive = vector / 2.5;
        const auto negative = vector / -0.5;
        require(positive.has_value() && negative.has_value(), "Nonzero division was rejected");
        requireVectorNear(*positive, Vec2{-3.0, 1.2});
        requireVectorNear(*negative, Vec2{15.0, -6.0});
    }

    void rejectsDivisionBySignedZero()
    {
        const Vec2 vector{1.0, -2.0};
        require(!(vector / 0.0).has_value(), "Division by positive zero must return nullopt");
        require(!(vector / -0.0).has_value(), "Division by negative zero must return nullopt");
    }

    void acceptsSmallNonzeroDivisor()
    {
        const Vec2 vector{2e-12, -3e-12};
        const auto result = vector / 1e-12;
        require(result.has_value(), "A small nonzero divisor must be accepted");
        requireVectorNear(*result, Vec2{2.0, -3.0});
    }

    void arithmeticPreservesOperands()
    {
        const Vec2 first{3.0, -4.0};
        const Vec2 second{-2.0, 5.0};
        requireVectorNear(first + second, Vec2{1.0, 1.0});
        requireVectorNear(first - second, Vec2{5.0, -9.0});
        requireVectorNear(first * 2.0, Vec2{6.0, -8.0});
        const auto divided = first / 2.0;
        require(divided.has_value(), "Division unexpectedly failed");
        requireVectorNear(*divided, Vec2{1.5, -2.0});
        requireVectorNear(first, Vec2{3.0, -4.0});
        requireVectorNear(second, Vec2{-2.0, 5.0});
    }

    void computesZeroAndAxisLengths()
    {
        requireNear(Vec2{}.Length(), 0.0, "Zero vector length");
        requireNear(Vec2{-7.0, 0.0}.Length(), 7.0, "Horizontal vector length");
        requireNear(Vec2{0.0, -2.5}.Length(), 2.5, "Vertical vector length");
    }

    void computesLengthsAcrossSignsAndScales()
    {
        requireNear(Vec2{3.0, 4.0}.Length(), 5.0, "Pythagorean length");
        requireNear(Vec2{-3.0, -4.0}.Length(), 5.0, "Negative coordinates length");
        requireNear(Vec2{0.3, -0.4}.Length(), 0.5, "Fractional length");
        requireNear(Vec2{3e100, 4e100}.Length(), 5e100, "Large representable length");
    }

    void normalizesWithoutChangingOriginal()
    {
        const Vec2 vector{3.0, 4.0};
        const auto normalized = vector.Normalized();
        require(normalized.has_value(), "Nonzero vector normalization failed");
        requireVectorNear(*normalized, Vec2{0.6, 0.8});
        requireNear(normalized->Length(), 1.0, "Normalized vector length");
        requireVectorNear(vector, Vec2{3.0, 4.0});
    }

    void normalizationPreservesDirection()
    {
        const Vec2 vector{-0.3, 0.4};
        const auto normalized = vector.Normalized();
        require(normalized.has_value(), "Fractional vector normalization failed");
        requireVectorNear(*normalized, Vec2{-0.6, 0.8});
        requireNear(normalized->Length(), 1.0, "Normalized fractional vector length");
        require(dot(vector, *normalized) > 0.0, "Normalization reversed vector direction");
    }

    void rejectsZeroAndShortNormalization()
    {
        require(!Vec2{}.Normalized().has_value(), "Zero vector must not have a direction");
        require(!Vec2{1e-12, -1e-12}.Normalized().has_value(),
            "A vector below the length threshold must be rejected");
    }

    void rejectsNormalizationAtThreshold()
    {
        require(!Vec2{1e-10, 0.0}.Normalized().has_value(),
            "A vector at the length threshold must be rejected");
        require(!Vec2{0.0, -1e-10}.Normalized().has_value(),
            "The length threshold must apply to both axes and signs");
    }

    void acceptsNormalizationAboveThreshold()
    {
        const auto axis = Vec2{2e-10, 0.0}.Normalized();
        require(axis.has_value(), "A vector above the threshold was rejected");
        requireVectorNear(*axis, Vec2{1.0, 0.0});

        // Each component is below the threshold, but their combined length is above it.
        const auto diagonal = Vec2{0.8e-10, 0.8e-10}.Normalized();
        require(diagonal.has_value(), "The threshold must apply to length, not each component");
        requireVectorNear(*diagonal, Vec2{std::sqrt(0.5), std::sqrt(0.5)});
    }

    void normalizingTwicePreservesResult()
    {
        const auto first = Vec2{2.0, -7.0}.Normalized();
        require(first.has_value(), "First normalization failed");
        const auto second = first->Normalized();
        require(second.has_value(), "Unit vector normalization failed");
        requireVectorNear(*second, *first);
        requireNear(second->Length(), 1.0, "Repeated normalization length");
    }

    void computesDotProduct()
    {
        const Vec2 first{2.0, 3.0};
        const Vec2 second{4.0, -5.0};
        requireNear(dot(first, second), -7.0, "Dot product");
        requireNear(dot(second, first), -7.0, "Dot product symmetry");
        requireNear(dot(first, first), 13.0, "Dot product with itself");
        requireNear(dot(first, Vec2{}), 0.0, "Dot product with zero");
    }

    void dotProductDetectsOrthogonalAndOppositeDirections()
    {
        requireNear(dot(Vec2{2.0, 3.0}, Vec2{-3.0, 2.0}), 0.0, "Perpendicular vectors");
        requireNear(dot(Vec2{2.0, 3.0}, Vec2{-4.0, -6.0}), -26.0, "Opposite vectors");
    }

    void crossProductHasCorrectOrientation()
    {
        const Vec2 right{1.0, 0.0};
        const Vec2 up{0.0, 1.0};
        requireNear(cross(right, up), 1.0, "Counterclockwise cross product");
        requireNear(cross(up, right), -1.0, "Clockwise cross product");
    }

    void computesGeneralCrossProduct()
    {
        const Vec2 first{2.0, 3.0};
        const Vec2 second{4.0, -5.0};
        requireNear(cross(first, second), -22.0, "General cross product");
        requireNear(cross(second, first), 22.0, "Cross product antisymmetry");
    }

    void crossProductDetectsParallelVectors()
    {
        const Vec2 vector{2.0, 3.0};
        requireNear(cross(vector, Vec2{4.0, 6.0}), 0.0, "Parallel vectors");
        requireNear(cross(vector, Vec2{-4.0, -6.0}), 0.0, "Antiparallel vectors");
        requireNear(cross(vector, vector), 0.0, "Cross product with itself");
        requireNear(cross(vector, Vec2{}), 0.0, "Cross product with zero");
    }
}

namespace cad::tests
{
    std::span<const TestCase> vec2Tests()
    {
        static const std::array tests{
            TestCase{"default constructor creates zero", defaultConstructorCreatesZero},
            TestCase{"constructor preserves coordinates", constructorPreservesCoordinates},
            TestCase{"vector addition", addsVectors},
            TestCase{"vector subtraction", subtractsVectors},
            TestCase{"positive, negative and zero scalar multiplication", multipliesByScalar},
            TestCase{"positive and negative scalar division", dividesByNonzeroScalar},
            TestCase{"division by positive and negative zero", rejectsDivisionBySignedZero},
            TestCase{"small nonzero divisor", acceptsSmallNonzeroDivisor},
            TestCase{"arithmetic preserves operands", arithmeticPreservesOperands},
            TestCase{"zero and axis lengths", computesZeroAndAxisLengths},
            TestCase{"length across signs and scales", computesLengthsAcrossSignsAndScales},
            TestCase{"normalization creates a unit vector and preserves input", normalizesWithoutChangingOriginal},
            TestCase{"normalization preserves fractional direction", normalizationPreservesDirection},
            TestCase{"zero and short vector normalization", rejectsZeroAndShortNormalization},
            TestCase{"normalization at the length threshold", rejectsNormalizationAtThreshold},
            TestCase{"normalization above the length threshold", acceptsNormalizationAboveThreshold},
            TestCase{"repeated normalization", normalizingTwicePreservesResult},
            TestCase{"dot product and symmetry", computesDotProduct},
            TestCase{"dot product for perpendicular and opposite directions", dotProductDetectsOrthogonalAndOppositeDirections},
            TestCase{"cross product orientation", crossProductHasCorrectOrientation},
            TestCase{"general cross product and antisymmetry", computesGeneralCrossProduct},
            TestCase{"cross product for parallel vectors", crossProductDetectsParallelVectors}
        };
        return tests;
    }
}
