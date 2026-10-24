#pragma once

#include <span>
#include <cad_core/constraints/FixedLengthConstraint.h>

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
        double lengthTolerance = 1e-6;
        unsigned maxIterations = 50;
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

} // namespace cad::solver
