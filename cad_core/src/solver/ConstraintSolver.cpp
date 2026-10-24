#include <cad_core/solver/ConstraintSolver.h>
#include <cad_core/model/Document.h>
#include "SolverDetail.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cad::solver::detail
{
    std::optional<std::vector<double>> evaluateErrors(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints)
    {
        std::vector<double> errors;
        errors.reserve(constraints.size());
        for (const auto& constraint : constraints)
        {
            const auto error = constraint::fixedLengthError(document, constraint);
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

    bool allSatisfied(std::span<const double> errors, double tolerance)
    {
        return std::all_of(errors.begin(), errors.end(),
            [tolerance](double error) { return std::abs(error) <= tolerance; });
    }

    double scaledErrorNorm(std::span<const double> errors, double scale)
    {
        double norm = 0;
        for (const double error : errors)
            norm = std::hypot(norm, error / scale);
        return norm;
    }

    std::vector<VertexId> collectVariables(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex)
    {
        std::vector<VertexId> variables;
        for (const auto& constraint : constraints)
        {
            const auto* edge = document.findEdge(constraint.edge);
            for (const VertexId id : {edge->start, edge->end})
                if (id != fixedVertex && std::find(variables.begin(), variables.end(), id) == variables.end())
                    variables.push_back(id);
        }
        return variables;
    }

    double geometryScale(const Document& document, std::span<const constraint::FixedLengthConstraint> constraints)
    {
        double scale = 0;
        for (const auto& constraint : constraints)
        {
            const auto* edge = document.findEdge(constraint.edge);
            const Vec2 difference = document.findVertex(edge->end)->position - document.findVertex(edge->start)->position;
            scale = std::max({scale, constraint.length, std::hypot(difference.x, difference.y)});
        }
        return scale;
    }

    bool hasZeroLengthEdge(const Document& document, std::span<const constraint::FixedLengthConstraint> constraints)
    {
        for (const auto& constraint : constraints)
        {
            const auto* edge = document.findEdge(constraint.edge);
            const Vec2 start = document.findVertex(edge->start)->position;
            const Vec2 end = document.findVertex(edge->end)->position;
            if (start.x == end.x && start.y == end.y)
                return true;
        }
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
        const DenseMatrix& jacobian, std::span<const double> errors, double scale, double damping)
    {
        const std::size_t rows = errors.size();
        const std::size_t columns = jacobian.front().size();
        DenseMatrix system(rows, std::vector<double>(rows, 0));
        std::vector<double> rhs(rows);
        for (std::size_t i = 0; i < rows; ++i)
        {
            rhs[i] = -errors[i] / scale;
            for (std::size_t j = 0; j < rows; ++j)
                for (std::size_t k = 0; k < columns; ++k)
                    system[i][j] += jacobian[i][k] * jacobian[j][k];
            system[i][i] += damping;
        }
        // (J J^T + lambda I)y = -F/scale; deltaNormalized = J^T y.
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
}

namespace cad::solver::detail
{
    SolveResult solveWithJacobian(Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex, const SolveOptions& options, JacobianBuilder buildJacobian)
    {
        if (!std::isfinite(options.lengthTolerance) || options.lengthTolerance <= 0 || !buildJacobian)
            return {SolveStatus::InvalidInput, 0};

        const auto* fixed = document.findVertex(fixedVertex);
        if (!fixed || !std::isfinite(fixed->position.x) || !std::isfinite(fixed->position.y) || !document.validate().empty())
            return {SolveStatus::InvalidInput, 0};

        auto errors = evaluateErrors(document, constraints);
        if (!errors)
            return {SolveStatus::InvalidInput, 0};
        if (allSatisfied(*errors, options.lengthTolerance))
            return {SolveStatus::Converged, 0};
        if (options.maxIterations == 0)
            return {SolveStatus::NoConvergence, 0};

        Document candidate = document;
        const auto variables = collectVariables(candidate, constraints, fixedVertex);
        const double scale = geometryScale(candidate, constraints);
        double damping = 1e-3;
        unsigned iterations = 0;

        while (true)
        {
            // Acceptance uses raw world-space errors, independent of normalization.
            if (allSatisfied(*errors, options.lengthTolerance))
            {
                if (!candidate.validate().empty())
                    return {SolveStatus::NoConvergence, iterations};
                document = std::move(candidate);
                return {SolveStatus::Converged, iterations};
            }
            if (iterations >= options.maxIterations)
                return {SolveStatus::NoConvergence, iterations};
            if (variables.empty() || hasZeroLengthEdge(candidate, constraints))
                return {SolveStatus::DegenerateGeometry, iterations};

            const auto jacobian = buildJacobian(candidate, constraints, variables, *errors, scale);
            if (!jacobian || !validJacobian(*jacobian, constraints.size(), 2 * variables.size()))
                return {SolveStatus::NoConvergence, iterations};

            const double currentNorm = scaledErrorNorm(*errors, scale);
            bool accepted = false;
            for (unsigned attempt = 0; attempt < 10 && !accepted; ++attempt)
            {
                const auto delta = dampedCorrection(*jacobian, *errors, scale, damping);
                if (delta)
                {
                    double step = 1;
                    for (unsigned backtrack = 0; backtrack < 16; ++backtrack, step *= 0.5)
                    {
                        Document trial = candidate;
                        if (!applyCorrection(trial, candidate, variables, *delta, scale, step))
                            continue;
                        auto trialErrors = evaluateErrors(trial, constraints);
                        if (!trialErrors || hasZeroLengthEdge(trial, constraints))
                            continue;
                        const double trialNorm = scaledErrorNorm(*trialErrors, scale);
                        if (std::isfinite(trialNorm) && trialNorm < currentNorm)
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
            if (!accepted)
                return {SolveStatus::NoConvergence, iterations};
            ++iterations;
        }
    }
}

namespace cad::solver
{
    SolveResult solveFixedLengths(Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex, const SolveOptions& options)
    {
        return detail::solveWithJacobian(document, constraints, fixedVertex, options, detail::computeJacobian);
    }
}
