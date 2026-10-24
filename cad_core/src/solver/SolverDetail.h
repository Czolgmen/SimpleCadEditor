#pragma once

#include "DenseLinearAlgebra.h"

#include <cad_core/solver/ConstraintSolver.h>

namespace cad::solver::detail
{
    // Raw signed errors in world units, in the same order as constraints.
    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints);

    // Forward finite differences, sampled on independent document copies.
    // Rows: constraints. Columns: x0, y0, x1, y1, ... from variableVertices.
    // baseErrors are RAW errors, not divided by geometryScale.
    // Return d(error_i)/d(coordinate_j), or nullopt if sampling fails.
    // This also equals d(error_i / scale)/d(coordinate_j / scale).
    // document must remain unchanged; fixedVertex is absent from variableVertices.
    std::optional<DenseMatrix> computeJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        std::span<const VertexId> variableVertices,
        std::span<const double> baseErrors,
        double geometryScale);

    using JacobianBuilder = decltype(&computeJacobian);

    // Private injection point for testing the algorithm with supplied known matrices.
    SolveResult solveWithJacobian(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex,
        const SolveOptions& options,
        JacobianBuilder buildJacobian);
}
