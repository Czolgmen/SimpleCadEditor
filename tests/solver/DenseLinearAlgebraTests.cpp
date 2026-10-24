#include "../../cad_core/src/solver/DenseLinearAlgebra.h"
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad::solver::detail;
    using cad::tests::require;

    void knownSystem()
    {
        const auto result = solveLinearSystem({{3, 2, -1}, {2, -2, 4}, {-1, 0.5, -1}}, {1, -2, 0});
        require(result && result->size() == 3, "Known system was rejected");
        require(std::abs((*result)[0] - 1) < 1e-12 && std::abs((*result)[1] + 2) < 1e-12 &&
            std::abs((*result)[2] + 2) < 1e-12, "Wrong solution of known system");
    }

    void swapsPivotRows()
    {
        const auto result = solveLinearSystem({{0, 2}, {1, 3}}, {4, 7});
        require(result && std::abs((*result)[0] - 1) < 1e-12 && std::abs((*result)[1] - 2) < 1e-12,
            "System requiring a pivot row swap failed");
    }

    void rejectsSingularSystems()
    {
        require(!solveLinearSystem({{1, 2}, {2, 4}}, {3, 6}), "Dependent rows were accepted");
        require(!solveLinearSystem({{0}}, {0}), "Zero matrix was accepted");
    }

    void rejectsInvalidDimensions()
    {
        require(!solveLinearSystem({{1, 2}}, {3}), "Non-square matrix was accepted");
        require(!solveLinearSystem({{1, 0}, {0}}, {3, 4}), "Ragged matrix was accepted");
        require(!solveLinearSystem({{1}}, {}), "Wrong RHS size was accepted");
    }

    void rejectsNonFiniteValues()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            require(!solveLinearSystem({{invalid}}, {1}), "Non-finite matrix was accepted");
            require(!solveLinearSystem({{1}}, {invalid}), "Non-finite RHS was accepted");
        }
        require(!solveLinearSystem({{1e308, 1e308}, {-1e308, 1e308}}, {1e308, 1e308}),
            "Overflow during elimination was accepted");
    }

    void supportsUniformlyScaledSystems()
    {
        for (const double scale : {1e-200, 1.0, 1e200})
        {
            const auto result = solveLinearSystem({{2 * scale, 0}, {0, 4 * scale}}, {6 * scale, 20 * scale});
            require(result && std::abs((*result)[0] - 3) < 1e-12 && std::abs((*result)[1] - 5) < 1e-12,
                "Scaling changed the solution");
        }
    }

    void acceptsEmptySystem()
    {
        const auto result = solveLinearSystem({}, {});
        require(result && result->empty(), "Empty system should have an empty solution");
    }

    void preservesArguments()
    {
        const DenseMatrix matrix{{0, 2}, {1, 3}};
        const std::vector<double> rhs{4, 7};
        require(solveLinearSystem(matrix, rhs).has_value(), "Fixture system failed");
        require(matrix == DenseMatrix{{0, 2}, {1, 3}} && rhs == std::vector<double>{4, 7}, "Input changed");
    }
}

namespace cad::tests
{
    std::span<const TestCase> denseLinearAlgebraTests()
    {
        static const std::array tests{
            TestCase{"known 3x3 system", knownSystem},
            TestCase{"partial pivoting swaps rows", swapsPivotRows},
            TestCase{"singular systems", rejectsSingularSystems},
            TestCase{"invalid dimensions", rejectsInvalidDimensions},
            TestCase{"non-finite inputs and arithmetic overflow", rejectsNonFiniteValues},
            TestCase{"uniformly scaled systems", supportsUniformlyScaledSystems},
            TestCase{"empty system", acceptsEmptySystem},
            TestCase{"input arguments are preserved", preservesArguments}
        };
        return tests;
    }
}
