#include "SolverDetail.h"

#include <cad_core/model/Document.h>

#include <cmath>
#include <limits>
#include <numbers>

namespace cad::solver::detail
{
    std::optional<DenseMatrix> computeJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        std::span<const VertexId> variableVertices,
        std::span<const double> baseErrors,
        double geometryScale)
    {
        return computeConstraintJacobian(document, constraints, {}, variableVertices, baseErrors, geometryScale);
    }

    std::optional<DenseMatrix> computeConstraintJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const VertexId> variableVertices,
        std::span<const double> baseErrors,
        double geometryScale)
    {
        std::vector<SolverPoint> variables;
        variables.reserve(variableVertices.size());
        for (const VertexId vertex : variableVertices)
            variables.emplace_back(vertex);

        return computePointJacobian(document, lengths, parallels, variables, baseErrors, geometryScale);
    }

    std::optional<DenseMatrix> computePointJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const SolverPoint> variables,
        std::span<const double> baseErrors,
        double geometryScale)
    {
        return computePointJacobian(document, lengths, parallels, {}, variables, baseErrors, geometryScale);
    }

    std::optional<DenseMatrix> computePointJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins,
        std::span<const SolverPoint> variables,
        std::span<const double> baseErrors,
        double geometryScale)
    {
        const auto kinds = buildResidualKinds(lengths.size(), parallels.size(), joins);
        if (!kinds || baseErrors.size() != kinds->size() ||
            !std::isfinite(geometryScale) || geometryScale <= 0)
            return std::nullopt;
        const std::size_t rowCount = kinds->size();
        for (const double error : baseErrors)
            if (!std::isfinite(error))
                return std::nullopt;

        const double step = std::sqrt(std::numeric_limits<double>::epsilon()) * geometryScale;
        if (!std::isfinite(step) || step <= 0)
            return std::nullopt;

        DenseMatrix jacobian(rowCount, std::vector<double>(2 * variables.size(), 0.0));
        for (std::size_t pointIndex = 0; pointIndex < variables.size(); ++pointIndex)
        {
            const auto position = getPointPosition(document, variables[pointIndex]);
            if (!position)
                return std::nullopt;

            for (std::size_t axis = 0; axis < 2; ++axis)
            {
                Vec2 samplePosition = *position;
                const double original = axis == 0 ? samplePosition.x : samplePosition.y;
                double shifted = original + step;
                // Addition can round back to the original coordinate at large offsets.
                if (shifted == original)
                    shifted = std::nextafter(original, std::numeric_limits<double>::infinity());
                const double actualStep = shifted - original;
                if (!std::isfinite(shifted) || !std::isfinite(actualStep) || actualStep <= 0)
                    return std::nullopt;

                (axis == 0 ? samplePosition.x : samplePosition.y) = shifted;
                // A fresh probe prevents the X sample from affecting the Y sample.
                Document probe = document;
                if (!setPointPosition(probe, variables[pointIndex], samplePosition))
                    return std::nullopt;
                const auto sampleErrors = evaluateErrors(probe, lengths, parallels, joins);
                if (!sampleErrors || sampleErrors->size() != rowCount)
                    return std::nullopt;

                const std::size_t column = 2 * pointIndex + axis;
                for (std::size_t row = 0; row < rowCount; ++row)
                {
                    const ResidualKind kind = (*kinds)[row];
                    double difference = (*sampleErrors)[row] - baseErrors[row];
                    // atan2 jumps between +pi and -pi; use the local angular change.
                    if (kind == ResidualKind::G1)
                        difference = std::remainder(difference, 2.0 * std::numbers::pi_v<double>);

                    // Length and C1 errors are scaled by geometryScale; angles and
                    // normalized parallel errors are dimensionless. q = position / scale.
                    const bool dimensional = kind == ResidualKind::Length || kind == ResidualKind::C1;
                    const double denominator = dimensional ? actualStep : actualStep / geometryScale;
                    if (!std::isfinite(denominator) || denominator <= 0)
                        return std::nullopt;
                    const double derivative = difference / denominator;
                    if (!std::isfinite(derivative))
                        return std::nullopt;
                    jacobian[row][column] = derivative;
                }
            }
        }
        return jacobian;
    }
}
