#include "SolverDetail.h"

#include <cad_core/model/Document.h>
#include <cad_core/constraints/ContinuityConstraint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
    using namespace cad;
    using namespace cad::solver::detail;

    struct ResidualGroup
    {
        ResidualKind kind;
        std::size_t relation;
        std::size_t row;
        std::array<SolverPoint, 4> dependencies{};
        std::size_t count = 0;

        void add(SolverPoint point) { dependencies[count++] = point; }
        bool dependsOn(const SolverPoint& point) const
        {
            return std::find(dependencies.begin(), dependencies.begin() + count, point) != dependencies.begin() + count;
        }
    };

    // The source residuals are validated before constructing these dependencies.
    std::vector<ResidualGroup> buildGroups(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins)
    {
        std::vector<ResidualGroup> groups;
        groups.reserve(lengths.size() + parallels.size() + joins.size());
        std::size_t row = 0;
        for (std::size_t i = 0; i < lengths.size(); ++i)
        {
            const auto* edge = document.findEdge(lengths[i].edge);
            ResidualGroup group{ResidualKind::Length, i, row++};
            group.add(edge->start);
            group.add(edge->end);
            groups.push_back(group);
        }
        for (std::size_t i = 0; i < parallels.size(); ++i)
        {
            ResidualGroup group{ResidualKind::Parallel, i, row++};
            for (const auto id : {parallels[i].first, parallels[i].second})
            {
                const auto* edge = document.findEdge(id);
                group.add(edge->start);
                group.add(edge->end);
            }
            groups.push_back(group);
        }
        for (std::size_t i = 0; i < joins.size(); ++i)
        {
            const auto& join = joins[i];
            if (join.continuity == Continuity::G0)
                continue;
            ResidualGroup group{join.continuity == Continuity::G1 ? ResidualKind::G1 : ResidualKind::C1, i, row};
            row += join.continuity == Continuity::G1 ? 1 : 2;
            group.add(join.vertex);
            const auto* incoming = document.findEdge(join.incoming);
            const auto* outgoing = document.findEdge(join.outgoing);
            if (std::holds_alternative<CubicBezier>(incoming->geometry))
                group.add(ControlPointRef{join.incoming, BezierControlPoint::Second});
            else
                group.add(incoming->start);
            if (std::holds_alternative<CubicBezier>(outgoing->geometry))
                group.add(ControlPointRef{join.outgoing, BezierControlPoint::First});
            else
                group.add(outgoing->end);
            groups.push_back(group);
        }
        return groups;
    }
}

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

        // Validate every relation, including those unrelated to the supplied variables.
        const auto sourceErrors = evaluateErrors(document, lengths, parallels, joins);
        if (!sourceErrors || sourceErrors->size() != rowCount)
            return std::nullopt;
        const bool currentBase = std::equal(sourceErrors->begin(), sourceErrors->end(), baseErrors.begin());
        const auto groups = buildGroups(document, lengths, parallels, joins);
        std::vector<const ResidualGroup*> affected;
        affected.reserve(groups.size());

        DenseMatrix jacobian(rowCount, std::vector<double>(2 * variables.size(), 0.0));
        Document probe = document;
        for (std::size_t pointIndex = 0; pointIndex < variables.size(); ++pointIndex)
        {
            const auto position = getPointPosition(document, variables[pointIndex]);
            if (!position)
                return std::nullopt;

            affected.clear();
            for (const auto& group : groups)
            {
                // Keep the previous finite-difference behavior for stale base errors.
                if (!currentBase || group.dependsOn(variables[pointIndex]))
                    affected.push_back(&group);
            }

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
                if (!setPointPosition(probe, variables[pointIndex], samplePosition))
                    return std::nullopt;

                const std::size_t column = 2 * pointIndex + axis;
                const auto storeDerivative = [&](std::size_t row, double sampleError)
                {
                    const ResidualKind kind = (*kinds)[row];
                    double difference = sampleError - baseErrors[row];
                    // atan2 jumps between +pi and -pi; use the local angular change.
                    if (kind == ResidualKind::G1)
                        difference = std::remainder(difference, 2.0 * std::numbers::pi_v<double>);

                    // Length and C1 errors are scaled by geometryScale; angles and
                    // normalized parallel errors are dimensionless. q = position / scale.
                    const bool dimensional = kind == ResidualKind::Length || kind == ResidualKind::C1;
                    const double denominator = dimensional ? actualStep : actualStep / geometryScale;
                    if (!std::isfinite(denominator) || denominator <= 0)
                        return false;
                    const double derivative = difference / denominator;
                    if (!std::isfinite(derivative))
                        return false;
                    jacobian[row][column] = derivative;
                    return true;
                };
                for (const auto* group : affected)
                {
                    if (group->kind == ResidualKind::Length)
                    {
                        const auto error = constraint::fixedLengthError(probe, lengths[group->relation]);
                        if (!error || !storeDerivative(group->row, *error))
                            return std::nullopt;
                    }
                    else if (group->kind == ResidualKind::Parallel)
                    {
                        const auto error = constraint::parallelError(probe, parallels[group->relation]);
                        if (!error || !storeDerivative(group->row, *error))
                            return std::nullopt;
                    }
                    else
                    {
                        const auto errors = constraint::continuityErrors(probe, joins[group->relation]);
                        const std::size_t count = group->kind == ResidualKind::G1 ? 1 : 2;
                        if (!errors || errors->size() != count)
                            return std::nullopt;
                        for (std::size_t i = 0; i < count; ++i)
                            if (!storeDerivative(group->row + i, (*errors)[i]))
                                return std::nullopt;
                    }
                }
                // Restore exactly, so neither X/Y nor successive points share a perturbation.
                if (!setPointPosition(probe, variables[pointIndex], *position))
                    return std::nullopt;
            }
        }
        return jacobian;
    }
}
