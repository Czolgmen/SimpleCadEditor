#include <cad_core/solver/ConstraintSolver.h>
#include <cad_core/model/Document.h>
#include "SolverDetail.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cad::solver::detail
{
    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints)
    {
        return evaluateErrors(document, constraints, {});
    }

    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels)
    {
        std::vector<double> errors;
        errors.reserve(lengths.size() + parallels.size());
        for (const auto& relation : lengths)
        {
            const auto error = constraint::fixedLengthError(document, relation);
            if (!error)
                return std::nullopt;
            errors.push_back(*error);
        }
        for (const auto& relation : parallels)
        {
            const auto error = constraint::parallelError(document, relation);
            if (!error)
                return std::nullopt;
            errors.push_back(*error);
        }
        return errors;
    }
}

namespace
{
    using namespace cad;
    using namespace cad::solver;
    using namespace cad::solver::detail;

    bool allSatisfied(std::span<const double> errors, std::size_t lengthCount, const SolveOptions& options)
    {
        if (lengthCount > errors.size())
            return false;
        for (std::size_t i = 0; i < errors.size(); ++i)
        {
            const double tolerance = i < lengthCount ? options.lengthTolerance : options.parallelTolerance;
            if (!std::isfinite(errors[i]) || std::abs(errors[i]) > tolerance)
                return false;
        }
        return true;
    }

    double scaledErrorNorm(std::span<const double> errors, std::size_t lengthCount, double scale)
    {
        double norm = 0;
        for (std::size_t i = 0; i < errors.size(); ++i)
        {
            const double error = i < lengthCount ? errors[i] / scale : errors[i];
            norm = std::hypot(norm, error);
        }
        return norm;
    }

    std::optional<double> edgeLength(const Document& document, EdgeId id)
    {
        const auto* edge = document.findEdge(id);
        if (!edge)
            return std::nullopt;
        const auto* start = document.findVertex(edge->start);
        const auto* end = document.findVertex(edge->end);
        if (!start || !end)
            return std::nullopt;
        const double length = std::hypot(end->position.x - start->position.x, end->position.y - start->position.y);
        return std::isfinite(length) ? std::optional<double>{length} : std::nullopt;
    }

    std::optional<SolveStatus> validateRelations(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels)
    {
        for (const auto& relation : lengths)
            if (!constraint::fixedLengthError(document, relation))
                return SolveStatus::InvalidInput;

        bool zeroDirection = false;
        for (const auto& relation : parallels)
        {
            if (relation.first == relation.second)
                return SolveStatus::InvalidInput;
            const auto first = edgeLength(document, relation.first);
            const auto second = edgeLength(document, relation.second);
            if (!first || !second)
                return SolveStatus::InvalidInput;
            zeroDirection = zeroDirection || *first == 0 || *second == 0;
        }
        // Validate every reference before diagnosing a zero parallel direction.
        if (zeroDirection)
            return SolveStatus::DegenerateGeometry;
        return std::nullopt;
    }

    std::vector<VertexId> collectVariables(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        VertexId fixedVertex)
    {
        std::vector<VertexId> variables;
        auto addVertex = [&](VertexId id)
        {
            if (id != fixedVertex && std::find(variables.begin(), variables.end(), id) == variables.end())
                variables.push_back(id);
        };
        auto addEdge = [&](EdgeId id)
        {
            const auto* edge = document.findEdge(id);
            addVertex(edge->start);
            addVertex(edge->end);
        };
        // References were validated before collecting unknowns.
        for (const auto& relation : lengths)
            addEdge(relation.edge);
        for (const auto& relation : parallels)
        {
            addEdge(relation.first);
            addEdge(relation.second);
        }
        return variables;
    }

    double geometryScale(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels)
    {
        double scale = 0;
        auto includeEdge = [&](EdgeId id) { scale = std::max(scale, *edgeLength(document, id)); };
        for (const auto& relation : lengths)
        {
            includeEdge(relation.edge);
            scale = std::max(scale, relation.length);
        }
        for (const auto& relation : parallels)
        {
            includeEdge(relation.first);
            includeEdge(relation.second);
        }
        return scale > 0 ? scale : 1.0;
    }

    bool hasZeroLengthEdge(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels)
    {
        auto isZero = [&](EdgeId id)
        {
            const auto length = edgeLength(document, id);
            return length && *length == 0;
        };
        for (const auto& relation : lengths)
            if (isZero(relation.edge))
                return true;
        for (const auto& relation : parallels)
            if (isZero(relation.first) || isZero(relation.second))
                return true;
        return false;
    }

    bool validJacobian(const DenseMatrix& matrix, std::size_t rows, std::size_t columns)
    {
        if (matrix.size() != rows)
            return false;
        for (const auto& row : matrix)
        {
            if (row.size() != columns)
                return false;
            for (const double value : row)
                if (!std::isfinite(value))
                    return false;
        }
        return true;
    }

    std::optional<std::vector<double>> dampedCorrection(
        const DenseMatrix& jacobian, std::span<const double> errors,
        std::size_t lengthCount, double scale, double damping)
    {
        const std::size_t rows = errors.size();
        const std::size_t columns = jacobian.front().size();
        DenseMatrix system(rows, std::vector<double>(rows, 0));
        std::vector<double> rhs(rows);
        for (std::size_t i = 0; i < rows; ++i)
        {
            rhs[i] = i < lengthCount ? -errors[i] / scale : -errors[i];
            for (std::size_t j = 0; j < rows; ++j)
                for (std::size_t k = 0; k < columns; ++k)
                    system[i][j] += jacobian[i][k] * jacobian[j][k];
            system[i][i] += damping;
        }
        // (J J^T + lambda I)y = -Fscaled; deltaNormalized = J^T y.
        const auto y = solveLinearSystem(std::move(system), std::move(rhs));
        if (!y)
            return std::nullopt;
        std::vector<double> delta(columns, 0);
        for (std::size_t column = 0; column < columns; ++column)
        {
            for (std::size_t row = 0; row < rows; ++row)
                delta[column] += jacobian[row][column] * (*y)[row];
            if (!std::isfinite(delta[column]))
                return std::nullopt;
        }
        return delta;
    }

    bool applyCorrection(Document& trial, const Document& candidate,
        std::span<const VertexId> variables, std::span<const double> delta, double scale, double step)
    {
        for (std::size_t i = 0; i < variables.size(); ++i)
        {
            const Vec2 original = candidate.findVertex(variables[i])->position;
            const Vec2 position{
                original.x + (step * delta[2 * i]) * scale,
                original.y + (step * delta[2 * i + 1]) * scale};
            if (!trial.moveVertex(variables[i], position))
                return false;
        }
        return true;
    }

    bool tryPerpendicularStep(Document& candidate, std::vector<double>& errors,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const VertexId> variables, double scale)
    {
        // sin(angle) has a stationary maximum at 90 degrees. A small coordinate
        // probe can leave that maximum when the linearized step cannot improve it.
        const auto parallelErrors = std::span<const double>{errors}.subspan(lengths.size());
        if (!std::any_of(parallelErrors.begin(), parallelErrors.end(),
            [](double error) { return std::abs(error) >= 1 - 1e-8; }))
            return false;

        const double currentNorm = scaledErrorNorm(errors, lengths.size(), scale);
        for (const VertexId vertex : variables)
        {
            const Vec2 original = candidate.findVertex(vertex)->position;
            for (const bool changeX : {true, false})
            {
                for (const double sign : {-1.0, 1.0})
                {
                    double fraction = 1e-4;
                    for (unsigned attempt = 0; attempt < 8; ++attempt, fraction *= 0.5)
                    {
                        Vec2 position = original;
                        double& coordinate = changeX ? position.x : position.y;
                        const double before = coordinate;
                        coordinate += (sign * fraction) * scale;
                        if (coordinate == before)
                            coordinate = std::nextafter(before, sign * std::numeric_limits<double>::infinity());
                        if (!std::isfinite(coordinate) || coordinate == before)
                            continue;
                        Document trial = candidate;
                        if (!trial.moveVertex(vertex, position))
                            continue;
                        auto trialErrors = evaluateErrors(trial, lengths, parallels);
                        if (!trialErrors || hasZeroLengthEdge(trial, lengths, parallels))
                            continue;
                        const double trialNorm = scaledErrorNorm(*trialErrors, lengths.size(), scale);
                        if (std::isfinite(trialNorm) && trialNorm < currentNorm)
                        {
                            candidate = std::move(trial);
                            errors = std::move(*trialErrors);
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    // The same loop serves the public solver and tests supplying known matrices.
    template<class JacobianFunction>
    SolveResult solveConstraintSystem(Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        VertexId fixedVertex, const SolveOptions& options, JacobianFunction buildJacobian)
    {
        if (!std::isfinite(options.lengthTolerance) || options.lengthTolerance <= 0 ||
            !std::isfinite(options.parallelTolerance) || options.parallelTolerance <= 0)
            return {SolveStatus::InvalidInput, 0};
        const auto* fixed = document.findVertex(fixedVertex);
        if (!fixed || !std::isfinite(fixed->position.x) || !std::isfinite(fixed->position.y) ||
            !document.validate().empty())
            return {SolveStatus::InvalidInput, 0};
        if (const auto failure = validateRelations(document, lengths, parallels))
            return {*failure, 0};

        const Vec2 fixedPosition = fixed->position;
        auto errors = evaluateErrors(document, lengths, parallels);
        if (!errors)
            return {SolveStatus::InvalidInput, 0};
        if (allSatisfied(*errors, lengths.size(), options))
            return {SolveStatus::Converged, 0};
        if (options.maxIterations == 0)
            return {SolveStatus::NoConvergence, 0};

        Document candidate = document;
        const auto variables = collectVariables(candidate, lengths, parallels, fixedVertex);
        const double scale = geometryScale(candidate, lengths, parallels);
        if (!std::isfinite(scale) || scale <= 0)
            return {SolveStatus::InvalidInput, 0};
        const std::size_t rowCount = lengths.size() + parallels.size();
        double damping = 1e-3;
        unsigned iterations = 0;

        while (true)
        {
            if (allSatisfied(*errors, lengths.size(), options))
            {
                const auto finalErrors = evaluateErrors(candidate, lengths, parallels);
                const Vec2 finalFixed = candidate.findVertex(fixedVertex)->position;
                if (!finalErrors || !allSatisfied(*finalErrors, lengths.size(), options) ||
                    !candidate.validate().empty() ||
                    finalFixed.x != fixedPosition.x || finalFixed.y != fixedPosition.y)
                    return {SolveStatus::NoConvergence, iterations};
                document = std::move(candidate);
                return {SolveStatus::Converged, iterations};
            }
            if (iterations >= options.maxIterations)
                return {SolveStatus::NoConvergence, iterations};
            if (variables.empty() || hasZeroLengthEdge(candidate, lengths, parallels))
                return {SolveStatus::DegenerateGeometry, iterations};

            const auto jacobian = buildJacobian(candidate, lengths, parallels, variables, *errors, scale);
            if (!jacobian || !validJacobian(*jacobian, rowCount, 2 * variables.size()))
                return {SolveStatus::NoConvergence, iterations};
            const double currentNorm = scaledErrorNorm(*errors, lengths.size(), scale);
            if (!std::isfinite(currentNorm))
                return {SolveStatus::NoConvergence, iterations};

            bool accepted = false;
            for (unsigned attempt = 0; attempt < 10 && !accepted; ++attempt)
            {
                const auto delta = dampedCorrection(*jacobian, *errors, lengths.size(), scale, damping);
                if (delta)
                {
                    double step = 1;
                    for (unsigned backtrack = 0; backtrack < 16; ++backtrack, step *= 0.5)
                    {
                        Document trial = candidate;
                        if (!applyCorrection(trial, candidate, variables, *delta, scale, step))
                            continue;
                        auto trialErrors = evaluateErrors(trial, lengths, parallels);
                        if (!trialErrors || hasZeroLengthEdge(trial, lengths, parallels))
                            continue;
                        const double trialNorm = scaledErrorNorm(*trialErrors, lengths.size(), scale);
                        if (std::isfinite(trialNorm) && (trialNorm < currentNorm ||
                            allSatisfied(*trialErrors, lengths.size(), options)))
                        {
                            candidate = std::move(trial);
                            errors = std::move(trialErrors);
                            damping = std::max(1e-12, damping * 0.3);
                            accepted = true;
                            break;
                        }
                    }
                }
                if (!accepted)
                    damping *= 10;
            }
            if (!accepted && tryPerpendicularStep(candidate, *errors, lengths, parallels, variables, scale))
            {
                damping = 1e-3;
                accepted = true;
            }
            if (!accepted)
                return {SolveStatus::NoConvergence, iterations};
            ++iterations;
        }
    }
}

namespace cad::solver::detail
{
    SolveResult solveWithJacobian(Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex, const SolveOptions& options, JacobianBuilder buildJacobian)
    {
        if (!buildJacobian)
            return {SolveStatus::InvalidInput, 0};
        return solveConstraintSystem(document, constraints, {}, fixedVertex, options,
            [buildJacobian](const Document& candidate,
                std::span<const constraint::FixedLengthConstraint> lengths,
                std::span<const constraint::ParallelConstraint>,
                std::span<const VertexId> variables, std::span<const double> errors, double scale)
            {
                return buildJacobian(candidate, lengths, variables, errors, scale);
            });
    }
}

namespace cad::solver
{
    SolveResult solveFixedLengths(Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex, const SolveOptions& options)
    {
        return solveConstraints(document, constraints, {}, fixedVertex, options);
    }

    SolveResult solveConstraints(Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        VertexId fixedVertex, const SolveOptions& options)
    {
        return solveConstraintSystem(document, lengths, parallels, fixedVertex, options, detail::computeConstraintJacobian);
    }
}
