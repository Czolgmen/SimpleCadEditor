#pragma once

#include <optional>
#include <vector>

namespace cad::solver::detail
{
    // Row-major: matrix[row][column]. Private to the solver implementation.
    using DenseMatrix = std::vector<std::vector<double>>;

    // Gaussian elimination with partial pivoting; inputs are copied intentionally.
    std::optional<std::vector<double>> solveLinearSystem(
        DenseMatrix matrix, std::vector<double> rhs);
}
