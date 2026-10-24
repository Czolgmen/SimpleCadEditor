#include "DenseLinearAlgebra.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cad::solver::detail
{
    std::optional<std::vector<double>> solveLinearSystem(DenseMatrix matrix, std::vector<double> rhs)
    {
        const std::size_t size = matrix.size();
        if (rhs.size() != size)
            return std::nullopt;

        double scale = 0;
        for (std::size_t row = 0; row < size; ++row)
        {
            if (matrix[row].size() != size || !std::isfinite(rhs[row]))
                return std::nullopt;
            for (const double value : matrix[row])
            {
                if (!std::isfinite(value))
                    return std::nullopt;
                scale = std::max(scale, std::abs(value));
            }
        }

        const double pivotTolerance = scale * std::numeric_limits<double>::epsilon() *
            static_cast<double>(size);
        for (std::size_t column = 0; column < size; ++column)
        {
            std::size_t pivot = column;
            for (std::size_t row = column + 1; row < size; ++row)
                if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                    pivot = row;

            if (std::abs(matrix[pivot][column]) <= pivotTolerance)
                return std::nullopt;
            std::swap(matrix[column], matrix[pivot]);
            std::swap(rhs[column], rhs[pivot]);

            for (std::size_t row = column + 1; row < size; ++row)
            {
                const double factor = matrix[row][column] / matrix[column][column];
                matrix[row][column] = 0;
                for (std::size_t j = column + 1; j < size; ++j)
                {
                    matrix[row][j] -= factor * matrix[column][j];
                    if (!std::isfinite(matrix[row][j]))
                        return std::nullopt;
                }
                rhs[row] -= factor * rhs[column];
                if (!std::isfinite(rhs[row]))
                    return std::nullopt;
            }
        }

        std::vector<double> solution(size);
        for (std::size_t row = size; row-- > 0;)
        {
            double value = rhs[row];
            for (std::size_t column = row + 1; column < size; ++column)
                value -= matrix[row][column] * solution[column];
            solution[row] = value / matrix[row][row];
            if (!std::isfinite(solution[row]))
                return std::nullopt;
        }
        return solution;
    }
}
