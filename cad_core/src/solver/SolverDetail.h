#pragma once

#include "DenseLinearAlgebra.h"

#include <cad_core/solver/ConstraintSolver.h>
#include <cad_core/model/ContinuityJoins.h>
#include <cad_core/solver/SolverPoint.h>

namespace cad::solver::detail
{
    // Raw signed errors in world units, in the same order as constraints.
    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints);

    // Forward finite differences, sampled independently on a private working copy.
    // Each perturbation is restored exactly before sampling another coordinate.
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

    // Rows: lengths, then parallels; baseErrors contain raw signed errors.
    // Derivatives are with respect to normalized coordinates q = position / scale.
    // Length residuals are divided by scale; parallel residuals are dimensionless.
    // Samples leave document unchanged; invalid samples return nullopt.
    std::optional<DenseMatrix> computeConstraintJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const VertexId> variableVertices,
        std::span<const double> baseErrors,
        double geometryScale);

    // Private injection point for testing the algorithm with supplied known matrices.
    SolveResult solveWithJacobian(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex,
        const SolveOptions& options,
        JacobianBuilder buildJacobian);

    // Same row order and scaling as computeConstraintJacobian.
    // Each vertex or Bezier control point contributes X and Y columns.
    // This overload supplies an empty list of continuity joins.
    std::optional<DenseMatrix> computePointJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const SolverPoint> variables,
        std::span<const double> baseErrors,
        double geometryScale);

    // Rows: lengths, parallels, then joins in input order (G0: 0, G1: 1, C1: 2).
    // baseErrors are raw. Length/C1 residuals are divided by geometryScale;
    // Parallel/G1 residuals are dimensionless. G1 differences wrap across +/-pi.
    // Returns derivatives with respect to q = position / geometryScale.
    std::optional<DenseMatrix> computePointJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins,
        std::span<const SolverPoint> variables,
        std::span<const double> baseErrors,
        double geometryScale);


    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels
    );

    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins);

    enum class ResidualKind
    {
        Length,
        Parallel,
        G1,
        C1
    };

    std::optional<std::vector<ResidualKind>> buildResidualKinds(
        std::size_t lengthCount,
        std::size_t parallelCount,
        std::span<const ContinuityJoin> joins);

    // Private entry point for the point-based solver. Collects active continuity
    // joins from document and commits only on Converged. The pinned vertex or
    // control point and any additional fixedVertices are preserved exactly.
    // iterations counts accepted corrections.
    SolveResult solvePointConstraintSystem(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        const SolverPoint& fixedPoint,
        const SolveOptions& options = {},
        std::span<const VertexId> fixedVertices = {});
}
