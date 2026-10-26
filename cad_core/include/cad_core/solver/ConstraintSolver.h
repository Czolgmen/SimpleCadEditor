#pragma once

#include <span>
#include <cad_core/constraints/FixedLengthConstraint.h>
#include <cad_core/constraints/ParallelConstraint.h>
#include <cad_core/model/ControlPointRef.h>

namespace cad::solver
{
    enum class SolveStatus 
    {
        Converged,
        InvalidInput,
        NoConvergence,
        DegenerateGeometry
    };

    struct SolveOptions
    {
        double lengthTolerance = 1e-6; // World units.
        unsigned maxIterations = 100;
        double parallelTolerance = 1e-6; // Absolute normalized cross product (sin of the angle).
        double g1Tolerance = 1e-6;
        double c1Tolerance = 1e-6;
        // Part of maxIterations, not an additional budget. Zero skips endpoint preference.
        unsigned maxFixedEndpointIterations = 8;
    };

    struct SolveResult{
        SolveStatus status;
        unsigned iterations;
    };

    // Solves supplied lengths and active continuity joins from the document.
    // Commits positions only on Converged. iterations counts accepted corrections.
    // The fixed vertex stays exactly at its input position; zero maxIterations
    // permits analytic arc fitting but no coordinate corrections. Arc bulges are
    // fitted on candidates, including G0/G0 semicircles, and commit only on success.
    // Requires a topologically valid document.
    SolveResult solveFixedLengths(
        Document& document,
        std::span<const cad::constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex,
        const SolveOptions& options = {}
    );

    // Solves supplied lengths/parallels and active continuity joins from the document.
    // Parallel edges may point in opposite directions; G1 requires matching tangent directions.
    // The same transaction, exact pin and iteration semantics as solveFixedLengths apply.
    SolveResult solveConstraints(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        VertexId fixedVertex,
        const SolveOptions& options = {}
    );

    // The control point is pinned exactly. First tries with both endpoints of its
    // Bezier fixed, for at most maxFixedEndpointIterations; on failure retries
    // with movable endpoints and the remaining total iteration budget.
    // Only a converged attempt commits; failure changes nothing.
    SolveResult solveConstraints(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        ControlPointRef fixedControl,
        const SolveOptions& options = {});
} // namespace cad::solver
