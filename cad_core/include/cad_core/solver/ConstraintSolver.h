#pragma once

#include <span>
#include <cad_core/constraints/FixedLengthConstraint.h>
#include <cad_core/constraints/ParallelConstraint.h>

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
        unsigned maxIterations = 50;
        double parallelTolerance = 1e-6; // Absolute normalized cross product (sin of the angle).
    };

    struct SolveResult{
        SolveStatus status;
        unsigned iterations;
    };

    // Commits positions only on Converged. iterations counts accepted corrections.
    // The fixed vertex stays exactly at its input position; zero maxIterations
    // checks the input without corrections. Requires a topologically valid document.
    SolveResult solveFixedLengths(
        Document& document,
        std::span<const cad::constraint::FixedLengthConstraint> constraints,
        VertexId fixedVertex,
        const SolveOptions& options = {}
    );

    // Solves all supplied relations together; opposite edge directions are allowed.
    // The same transaction, exact pin and iteration semantics as solveFixedLengths apply.
    SolveResult solveConstraints(
        Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        VertexId fixedVertex,
        const SolveOptions& options = {}
    );
} // namespace cad::solver
