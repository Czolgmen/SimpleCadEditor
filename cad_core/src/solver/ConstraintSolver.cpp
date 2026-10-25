#include <cad_core/solver/ConstraintSolver.h>
#include <cad_core/model/Document.h>
#include <cad_core/model/ContinuityJoins.h>
#include <cad_core/solver/SolverPoint.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include "SolverDetail.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <variant>

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

    bool allSatisfied(
        std::span<const double> errors,
        std::span<const ResidualKind> kinds,
        const SolveOptions& options)
    {
        if (errors.size() != kinds.size())
            return false;

        for(std::size_t i = 0; i < errors.size(); i++)
        {
            if(!std::isfinite(errors[i]))
                return false;

            switch (kinds[i])
            {
            case ResidualKind::Length:
                if(std::abs(errors[i]) > options.lengthTolerance)
                    return false;
                break;
            case ResidualKind::Parallel:
                if(std::abs(errors[i]) > options.parallelTolerance)
                    return false;
                break;
                
            case ResidualKind::C1:
                if(std::abs(errors[i]) > options.c1Tolerance)
                    return false;
                break;
                
            case ResidualKind::G1:
                if(std::abs(errors[i]) > options.g1Tolerance)
                    return false;
                break;
                
            default:
                return false;
            }
        }

        return true;
    }

    std::optional<double> scaledErrorNorm(
        std::span<const double> errors,
        std::span<const ResidualKind> kinds,
        double scale)
    {
        if (errors.size() != kinds.size() || !std::isfinite(scale) || scale <= 0)
            return std::nullopt;

        double norm = 0.0;
        for (std::size_t i = 0; i < errors.size(); ++i)
        {
            double error = errors[i];
            switch (kinds[i])
            {
            case ResidualKind::Length:
            case ResidualKind::C1:
                error /= scale;
                break;

            case ResidualKind::Parallel:
            case ResidualKind::G1:
                break;

            default:
                return std::nullopt;
            }

            if (!std::isfinite(error))
                return std::nullopt;

            norm = std::hypot(norm, error);
        }

        if (!std::isfinite(norm))
            return std::nullopt;

        return norm;
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

    std::vector<SolverPoint> collectSolverPoints(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins,
        const SolverPoint& fixedPoint)
    {
        std::vector<SolverPoint> variables;

        auto addPoint = [&](const SolverPoint& point)
        {
            if (point != fixedPoint &&
                std::find(variables.begin(), variables.end(), point) == variables.end())
            {
                variables.push_back(point);
            }
        };

        // References and continuity classes must be validated before collection.
        auto addEdgeEndpoints = [&](EdgeId id)
        {
            const auto* edge = document.findEdge(id);
            addPoint(edge->start);
            addPoint(edge->end);
        };

        for (const auto& relation : lengths)
            addEdgeEndpoints(relation.edge);

        for (const auto& relation : parallels)
        {
            addEdgeEndpoints(relation.first);
            addEdgeEndpoints(relation.second);
        }

        for (const auto& join : joins)
        {
            if (join.continuity == Continuity::G0)
                continue;

            const auto* incoming = document.findEdge(join.incoming);
            const auto* outgoing = document.findEdge(join.outgoing);

            addPoint(join.vertex);

            if (std::holds_alternative<CubicBezier>(incoming->geometry))
                addPoint(ControlPointRef{join.incoming, BezierControlPoint::Second});
            else
                addPoint(incoming->start);

            if (std::holds_alternative<CubicBezier>(outgoing->geometry))
                addPoint(ControlPointRef{join.outgoing, BezierControlPoint::First});
            else
                addPoint(outgoing->end);
        }

        return variables;
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

    std::optional<double> geometryScale(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins)
    {
        double scale = 0.0;

        auto includeDistance = [&](const Vec2& a, const Vec2& b)
        {
            if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
                !std::isfinite(b.x) || !std::isfinite(b.y))
                return false;

            const double distance = std::hypot(b.x - a.x, b.y - a.y);
            if (!std::isfinite(distance))
                return false;

            scale = std::max(scale, distance);
            return true;
        };

        auto includeEdge = [&](EdgeId id)
        {
            const auto* edge = document.findEdge(id);
            if (!edge)
                return false;

            const auto* start = document.findVertex(edge->start);
            const auto* end = document.findVertex(edge->end);
            return start && end && includeDistance(start->position, end->position);
        };

        for (const auto& relation : lengths)
        {
            if (!std::isfinite(relation.length) || relation.length <= 0 || !includeEdge(relation.edge))
                return std::nullopt;

            scale = std::max(scale, relation.length);
        }

        for (const auto& relation : parallels)
        {
            if (!includeEdge(relation.first) || !includeEdge(relation.second))
                return std::nullopt;
        }

        for (const auto& join : joins)
        {
            switch (join.continuity)
            {
            case Continuity::G0:
                continue;
            case Continuity::G1:
            case Continuity::C1:
                break;
            default:
                return std::nullopt;
            }

            const auto* incoming = document.findEdge(join.incoming);
            const auto* outgoing = document.findEdge(join.outgoing);
            const auto* vertex = document.findVertex(join.vertex);
            if (!incoming || !outgoing || !vertex ||
                incoming->end != join.vertex || outgoing->start != join.vertex ||
                !includeEdge(join.incoming) || !includeEdge(join.outgoing))
                return std::nullopt;

            if (const auto* bezier = std::get_if<CubicBezier>(&incoming->geometry))
            {
                if (!includeDistance(bezier->control2, vertex->position))
                    return std::nullopt;
            }

            if (const auto* bezier = std::get_if<CubicBezier>(&outgoing->geometry))
            {
                if (!includeDistance(vertex->position, bezier->control1))
                    return std::nullopt;
            }
        }

        return scale > 0 ? scale : 1.0;
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
        const DenseMatrix& jacobian,
        std::span<const double> errors,
        std::span<const ResidualKind> kinds,
        double scale,
        double damping)
    {
        if (errors.empty() || errors.size() != kinds.size() ||
            jacobian.size() != errors.size() ||
            !std::isfinite(scale) || scale <= 0 ||
            !std::isfinite(damping) || damping <= 0)
        {
            return std::nullopt;
        }

        const std::size_t rows = errors.size();
        const std::size_t columns = jacobian.front().size();

        if (columns == 0 || !validJacobian(jacobian, rows, columns))
            return std::nullopt;

        DenseMatrix system(rows, std::vector<double>(rows, 0));
        std::vector<double> rhs(rows);
        for (std::size_t i = 0; i < rows; ++i)
        {
            switch (kinds[i])
            {
            case ResidualKind::Length:
            case ResidualKind::C1:
                rhs[i] = -errors[i] / scale;
                break;
            
            case ResidualKind::Parallel:
            case ResidualKind::G1:
                rhs[i] = -errors[i];
                break;
            
            default:
                return std::nullopt;
            }

            if (!std::isfinite(rhs[i]))
                return std::nullopt;
            // J*J^T is symmetric. Cache contiguous rows outside the hot loop;
            // Debug STL bounds/iterator machinery is expensive per scalar access.
            const double* rowI = jacobian[i].data();
            double* systemRow = system[i].data();
            for (std::size_t j = 0; j <= i; ++j)
            {
                const double* rowJ = jacobian[j].data();
                double value = 0.0;
                for (std::size_t k = 0; k < columns; ++k)
                    value += rowI[k] * rowJ[k];
                systemRow[j] = value;
                system[j][i] = value;
            }
            systemRow[i] += damping;
        }
        // (J J^T + lambda I)y = -Fscaled; deltaNormalized = J^T y.
        const auto y = solveLinearSystem(std::move(system), std::move(rhs));
        if (!y)
            return std::nullopt;
        std::vector<double> delta(columns, 0);
        double* deltaValues = delta.data();
        for (std::size_t row = 0; row < rows; ++row)
        {
            const double* jacobianRow = jacobian[row].data();
            const double value = (*y)[row];
            for (std::size_t column = 0; column < columns; ++column)
                deltaValues[column] += jacobianRow[column] * value;
        }
        for (const double value : delta)
            if (!std::isfinite(value))
                return std::nullopt;
        return delta;
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

    bool applyCorrection(
        Document& trial,
        const Document& candidate,
        std::span<const SolverPoint> variables,
        std::span<const double> delta,
        double scale,
        double step)
    {
        if (delta.size() != 2 * variables.size())
            return false;

        for (size_t i = 0; i < variables.size(); i++)
        {
            const auto original = getPointPosition(candidate, variables[i]);
            if (!original)
                return false;

            const Vec2 position{
                original->x + step * delta[2 * i] * scale,
                original->y + step * delta[2 * i + 1] * scale
            };

            if (!setPointPosition(trial, variables[i], position))
                return false;
            
        }
        return true;
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

    bool tryPerpendicularPointStep(
        Document& candidate,
        std::vector<double>& errors,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins,
        std::span<const ResidualKind> kinds,
        std::span<const SolverPoint> variables,
        double scale)
    {
        // Only parallel rows have the stationary maximum at perpendicularity.
        // Continuity rows following them must not trigger this escape step.
        const auto parallelErrors = std::span<const double>{errors}.subspan(lengths.size(), parallels.size());
        if (!std::any_of(parallelErrors.begin(), parallelErrors.end(),
            [](double error) { return std::abs(error) >= 1 - 1e-8; }))
            return false;

        const auto currentNorm = scaledErrorNorm(errors, kinds, scale);
        if (!currentNorm)
            return false;

        Document trial = candidate;
        for (const auto& point : variables)
        {
            const auto original = getPointPosition(candidate, point);
            if (!original)
                return false;

            for (const bool changeX : {true, false})
            {
                for (const double sign : {-1.0, 1.0})
                {
                    double fraction = 1e-4;
                    for (unsigned attempt = 0; attempt < 8; ++attempt, fraction *= 0.5)
                    {
                        Vec2 position = *original;
                        double& coordinate = changeX ? position.x : position.y;
                        const double before = coordinate;
                        coordinate += (sign * fraction) * scale;
                        if (coordinate == before)
                            coordinate = std::nextafter(before, sign * std::numeric_limits<double>::infinity());
                        if (!std::isfinite(coordinate) || coordinate == before)
                            continue;

                        if (!setPointPosition(trial, point, position))
                            continue;
                        auto trialErrors = evaluateErrors(trial, lengths, parallels, joins);
                        const auto trialNorm = trialErrors && !hasZeroLengthEdge(trial, lengths, parallels)
                            ? scaledErrorNorm(*trialErrors, kinds, scale) : std::nullopt;
                        if (trialNorm && *trialNorm < *currentNorm)
                        {
                            candidate = std::move(trial);
                            errors = std::move(*trialErrors);
                            return true;
                        }
                        if (!setPointPosition(trial, point, *original))
                            return false;
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
    SolveResult solvePointConstraintSystem(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        const SolverPoint& fixedPoint,
        const SolveOptions& options)
    {
        if (!std::isfinite(options.lengthTolerance) || options.lengthTolerance <= 0 ||
            !std::isfinite(options.parallelTolerance) || options.parallelTolerance <= 0 ||
            !std::isfinite(options.g1Tolerance) || options.g1Tolerance <= 0 ||
            !std::isfinite(options.c1Tolerance) || options.c1Tolerance <= 0 ||
            !document.validate().empty())
            return {SolveStatus::InvalidInput, 0};

        const auto fixedPosition = getPointPosition(document, fixedPoint);
        if (!fixedPosition)
            return {SolveStatus::InvalidInput, 0};
        if (const auto failure = validateRelations(document, lengths, parallels))
            return {*failure, 0};

        const auto joins = collectContinuityJoins(document);
        if (!joins)
            return {SolveStatus::InvalidInput, 0};
        const auto kinds = buildResidualKinds(lengths.size(), parallels.size(), *joins);
        auto errors = evaluateErrors(document, lengths, parallels, *joins);
        if (!kinds || !errors || kinds->size() != errors->size())
            return {SolveStatus::InvalidInput, 0};
        if (allSatisfied(*errors, *kinds, options))
            return {SolveStatus::Converged, 0};
        if (options.maxIterations == 0)
            return {SolveStatus::NoConvergence, 0};

        Document candidate = document;
        const auto variables = collectSolverPoints(candidate, lengths, parallels, *joins, fixedPoint);
        const auto scale = geometryScale(candidate, lengths, parallels, *joins);
        if (!scale)
            return {SolveStatus::InvalidInput, 0};

        const std::size_t rowCount = kinds->size();
        double damping = 1e-3;
        unsigned iterations = 0;
        while (true)
        {
            if (allSatisfied(*errors, *kinds, options))
            {
                const auto finalErrors = evaluateErrors(candidate, lengths, parallels, *joins);
                const auto finalFixed = getPointPosition(candidate, fixedPoint);
                if (!finalErrors || !allSatisfied(*finalErrors, *kinds, options) ||
                    !candidate.validate().empty() || !finalFixed ||
                    finalFixed->x != fixedPosition->x || finalFixed->y != fixedPosition->y)
                    return {SolveStatus::NoConvergence, iterations};

                document = std::move(candidate);
                return {SolveStatus::Converged, iterations};
            }
            if (iterations >= options.maxIterations)
                return {SolveStatus::NoConvergence, iterations};
            if (variables.empty() || hasZeroLengthEdge(candidate, lengths, parallels))
                return {SolveStatus::DegenerateGeometry, iterations};

            const auto jacobian = computePointJacobian(
                candidate, lengths, parallels, *joins, variables, *errors, *scale);
            if (!jacobian || !validJacobian(*jacobian, rowCount, 2 * variables.size()))
                return {SolveStatus::NoConvergence, iterations};
            const auto currentNorm = scaledErrorNorm(*errors, *kinds, *scale);
            if (!currentNorm)
                return {SolveStatus::NoConvergence, iterations};

            // Every correction overwrites all variables relative to candidate, so
            // rejected step sizes can reuse this document without copying its maps.
            Document trial = candidate;
            bool accepted = false;
            for (unsigned attempt = 0; attempt < 10 && !accepted; ++attempt)
            {
                if (!std::isfinite(damping))
                    break;
                const auto delta = dampedCorrection(*jacobian, *errors, *kinds, *scale, damping);
                if (delta)
                {
                    double step = 1.0;
                    for (unsigned backtrack = 0; backtrack < 16; ++backtrack, step *= 0.5)
                    {
                        if (!applyCorrection(trial, candidate, variables, *delta, *scale, step))
                            continue;
                        auto trialErrors = evaluateErrors(trial, lengths, parallels, *joins);
                        if (!trialErrors || hasZeroLengthEdge(trial, lengths, parallels))
                            continue;
                        const auto trialNorm = scaledErrorNorm(*trialErrors, *kinds, *scale);
                        if (trialNorm && (*trialNorm < *currentNorm || allSatisfied(*trialErrors, *kinds, options)))
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

            if (!accepted && tryPerpendicularPointStep(
                candidate, *errors, lengths, parallels, *joins, *kinds, variables, *scale))
            {
                damping = 1e-3;
                accepted = true;
            }
            if (!accepted)
                return {SolveStatus::NoConvergence, iterations};
            ++iterations;
        }
    }

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

    std::optional<std::vector<double>> evaluateErrors(const Document &document, std::span<const constraint::FixedLengthConstraint> lengths, std::span<const constraint::ParallelConstraint> parallels, std::span<const ContinuityJoin> joins)
    {
        auto errors = evaluateErrors(document, lengths, parallels);
        if (!errors)
            return std::nullopt;

        for (const auto &join : joins)
        {
            const auto joinErrors = constraint::continuityErrors(document, join);
            if (!joinErrors)
                return std::nullopt;

            errors->insert(
                errors->end(),
                joinErrors->begin(),
                joinErrors->end()
            );
        }
        return errors; 
    }

    std::optional<std::vector<ResidualKind>> buildResidualKinds(
        std::size_t lengthCount,
        std::size_t parallelCount,
        std::span<const ContinuityJoin> joins)
    {
        std::vector<ResidualKind> kinds(lengthCount, ResidualKind::Length);
        kinds.insert(kinds.end(), parallelCount, ResidualKind::Parallel);

        // Keep the same row order as evaluateErrors(document, lengths, parallels, joins).
        for (const auto& join : joins)
        {
            switch (join.continuity)
            {
            case Continuity::G0:
                break;
            case Continuity::G1:
                kinds.push_back(ResidualKind::G1);
                break;
            case Continuity::C1:
                kinds.insert(kinds.end(), 2, ResidualKind::C1);
                break;
            default:
                return std::nullopt;
            }
        }

        return kinds;
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
        return detail::solvePointConstraintSystem(
            document, lengths, parallels,
            detail::SolverPoint{fixedVertex}, options);
    }

    SolveResult solveConstraints(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        ControlPointRef fixedControl,
        const SolveOptions& options)
    {
        return detail::solvePointConstraintSystem(
            document, lengths, parallels,
            detail::SolverPoint{fixedControl}, options);
    }
}
