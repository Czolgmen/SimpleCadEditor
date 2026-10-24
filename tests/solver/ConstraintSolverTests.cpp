#include <cad_core/model/Document.h>
#include <cad_core/solver/ConstraintSolver.h>

#include "../../cad_core/src/solver/SolverDetail.h"
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    using namespace cad;
    using namespace cad::solver;
    using namespace cad::solver::detail;
    using constraint::FixedLengthConstraint;
    using cad::tests::require;

    struct Fixture
    {
        Document document;
        PolygonId polygon;
        VertexId start, end, other;
        EdgeId edge;

        Fixture(double scale = 1)
        {
            const std::array points{Vec2{0, 0}, Vec2{10 * scale, 0}, Vec2{20 * scale, 0}};
            polygon = document.addPolygon(points);
            const auto* p = document.findPolygon(polygon);
            start = p->vertices[0];
            end = p->vertices[1];
            other = p->vertices[2];
            edge = p->edges[0];
        }
    };

    // These fixtures supply SMALL KNOWN MATRICES, not a Jacobian implementation.
    // They exercise the remaining solver on axis-aligned, known test configurations.
    std::optional<DenseMatrix> forwardRow(const Document& document,
        std::span<const FixedLengthConstraint> constraints, std::span<const VertexId> variables,
        std::span<const double> baseErrors, double scale)
    {
        require(constraints.size() == 1 && variables.size() == 1 && baseErrors.size() == 1, "Wrong matrix layout");
        const auto* edge = document.findEdge(constraints[0].edge);
        require(variables[0] == edge->end, "Pinned start was included or end was omitted");
        const auto actual = constraint::fixedLengthError(document, constraints[0]);
        require(actual && *actual == baseErrors[0] && scale > 0 && std::isfinite(scale), "Wrong matrix inputs");
        return DenseMatrix{{1, 0}};
    }

    using C = std::span<const FixedLengthConstraint>;
    using V = std::span<const VertexId>;
    using E = std::span<const double>;

    std::optional<DenseMatrix> reverseRow(const Document&, C, V, E, double) { return DenseMatrix{{-1, 0}}; }
    std::optional<DenseMatrix> freePair(const Document&, C, V, E, double) { return DenseMatrix{{-1, 0, 1, 0}}; }
    std::optional<DenseMatrix> chainRows(const Document&, C, V, E, double)
    { return DenseMatrix{{1, 0, 0, 0}, {-1, 0, 1, 0}}; }
    std::optional<DenseMatrix> separateRows(const Document&, C, V, E, double)
    { return DenseMatrix{{1, 0, 0, 0, 0, 0}, {0, 0, -1, 0, 1, 0}}; }
    std::optional<DenseMatrix> repeatedRows(const Document&, C, V, E, double) { return DenseMatrix{{1, 0}, {1, 0}}; }
    std::optional<DenseMatrix> zeroRow(const Document&, C, V, E, double) { return DenseMatrix{{0, 0}}; }
    std::optional<DenseMatrix> shortRow(const Document&, C, V, E, double) { return DenseMatrix{{1}}; }
    std::optional<DenseMatrix> noRows(const Document&, C, V, E, double) { return DenseMatrix{}; }
    std::optional<DenseMatrix> failedSampling(const Document&, C, V, E, double) { return std::nullopt; }
    std::optional<DenseMatrix> invalidRow(const Document&, C, V, E, double)
    { return DenseMatrix{{std::numeric_limits<double>::quiet_NaN(), 0}}; }
    std::optional<DenseMatrix> underscaledRow(const Document&, C, V, E, double) { return DenseMatrix{{0.1, 0}}; }
    std::optional<DenseMatrix> throwingBuilder(const Document&, C, V, E, double) { throw std::runtime_error("test failure"); }

    void requireSame(const Document& actual, const Document& before)
    {
        require(actual.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* after = actual.findPolygon(id);
            require(after && after->id == polygon.id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Polygon topology changed");
            for (const VertexId vertexId : polygon.vertices)
            {
                const auto* a = actual.findVertex(vertexId);
                const auto* b = before.findVertex(vertexId);
                require(a && a->id == b->id && a->position.x == b->position.x && a->position.y == b->position.y,
                    "Vertex changed during rejected solve");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const auto* a = actual.findEdge(edgeId);
                const auto* b = before.findEdge(edgeId);
                require(a && a->id == b->id && a->start == b->start && a->end == b->end, "Edge changed");
            }
        }
    }

    void requireSolved(const Document& document, C constraints, const SolveResult& result, double tolerance = 1e-6)
    {
        require(result.status == SolveStatus::Converged, "Solver did not converge");
        require(document.validate().empty(), "Solution invalidated the document");
        for (const auto& c : constraints)
        {
            const auto error = constraint::fixedLengthError(document, c);
            require(error && std::abs(*error) <= tolerance, "Converged reported with an unsatisfied constraint");
        }
    }

    void emptyConstraintsConvergeWithoutIterations()
    {
        Fixture f;
        const Document before = f.document;
        const auto result = solveFixedLengths(f.document, {}, f.start);
        require(result.status == SolveStatus::Converged && result.iterations == 0, "Empty solve failed");
        requireSame(f.document, before);
    }

    void satisfiedInputNeedsNoJacobian()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edge, 10}};
        const auto result = solveFixedLengths(f.document, constraints, f.start, {1e-6, 0});
        requireSolved(f.document, constraints, result);
        require(result.iterations == 0, "Satisfied input consumed iterations");
    }

    void rejectsInvalidTolerances()
    {
        Fixture f;
        const Document before = f.document;
        for (const double tolerance : {0.0, -1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        {
            const auto result = solveFixedLengths(f.document, {}, f.start, {tolerance, 50});
            require(result.status == SolveStatus::InvalidInput && result.iterations == 0, "Invalid tolerance accepted");
            requireSame(f.document, before);
        }
    }

    void rejectsInvalidFixedVertex()
    {
        Fixture f;
        for (const VertexId id : {VertexId{}, VertexId{999999}})
            require(solveFixedLengths(f.document, {}, id).status == SolveStatus::InvalidInput, "Unknown pin accepted");
        auto* vertex = const_cast<Vertex*>(f.document.findVertex(f.other));
        vertex->position.x = std::numeric_limits<double>::quiet_NaN();
        const std::array constraints{FixedLengthConstraint{f.edge, 10}};
        require(solveFixedLengths(f.document, constraints, f.other).status == SolveStatus::InvalidInput,
            "Non-finite fixed vertex outside the constraints was accepted");
    }

    void validatesEveryConstraintBeforeReportingSuccess()
    {
        Fixture f;
        const Document before = f.document;
        for (const FixedLengthConstraint invalid : {FixedLengthConstraint{EdgeId{}, 5},
            FixedLengthConstraint{f.edge, -1}, FixedLengthConstraint{f.edge, std::numeric_limits<double>::infinity()}})
        {
            const std::array constraints{FixedLengthConstraint{f.edge, 10}, invalid};
            require(solveFixedLengths(f.document, constraints, f.start).status == SolveStatus::InvalidInput,
                "Invalid later constraint was accepted");
            requireSame(f.document, before);
        }
    }

    void rejectsBrokenTopology()
    {
        Fixture f;
        auto* polygon = const_cast<Polygon*>(f.document.findPolygon(f.polygon));
        polygon->edges.pop_back();
        require(solveFixedLengths(f.document, {}, f.start).status == SolveStatus::InvalidInput,
            "Broken document topology was accepted");
    }

    void zeroBudgetDoesNotChangeDocument()
    {
        Fixture f;
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start, {1e-6, 0});
        require(result.status == SolveStatus::NoConvergence && result.iterations == 0, "Zero budget was ignored");
        requireSame(f.document, before);
    }

    void zeroLengthNeedsDirection()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {0, 0}), "Fixture move failed");
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        require(result.status == SolveStatus::DegenerateGeometry && result.iterations == 0, "Zero edge not diagnosed");
        requireSame(f.document, before);
    }

    void solvesOneEdgeWithPinnedStart()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, forwardRow);
        requireSolved(f.document, constraints, result);
        require(result.iterations > 0 && result.iterations <= 50, "Wrong correction count");
        const Vec2 start = f.document.findVertex(f.start)->position;
        const Vec2 end = f.document.findVertex(f.end)->position;
        require(start.x == 0 && start.y == 0 && end.y == 0 && std::abs(end.x - 5) <= 1e-6, "Wrong endpoint movement");
        require(f.document.findVertex(f.other)->position.x == 20, "Unconstrained vertex moved");
    }

    void solvesOneEdgeWithPinnedEnd()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.end, {}, reverseRow);
        requireSolved(f.document, constraints, result);
        require(f.document.findVertex(f.end)->position.x == 10 && std::abs(f.document.findVertex(f.start)->position.x - 5) <= 1e-6,
            "Pinned end moved or start did not move");
    }

    void freeEndpointsShareCorrection()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.other, {}, freePair);
        requireSolved(f.document, constraints, result);
        require(std::abs(f.document.findVertex(f.start)->position.x - 2.5) <= 1e-6 &&
            std::abs(f.document.findVertex(f.end)->position.x - 7.5) <= 1e-6 &&
            f.document.findVertex(f.other)->position.x == 20, "Minimum-norm correction failed");
    }

    void solvesCoupledLengthsTogether()
    {
        Fixture f;
        const auto* p = f.document.findPolygon(f.polygon);
        const std::array constraints{FixedLengthConstraint{p->edges[0], 5}, FixedLengthConstraint{p->edges[1], 7}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, chainRows);
        requireSolved(f.document, constraints, result);
        require(f.document.findVertex(f.start)->position.x == 0 &&
            std::abs(f.document.findVertex(f.end)->position.x - 5) <= 1e-6 &&
            std::abs(f.document.findVertex(f.other)->position.x - 12) <= 2e-6, "Wrong coupled solution");
    }

    void solvesConstraintsAcrossPolygons()
    {
        Fixture f;
        const std::array points{Vec2{30, 0}, Vec2{50, 0}, Vec2{60, 10}};
        const auto* second = f.document.findPolygon(f.document.addPolygon(points));
        const VertexId a = second->vertices[0], b = second->vertices[1], untouched = second->vertices[2];
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{second->edges[0], 8}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, separateRows);
        requireSolved(f.document, constraints, result);
        require(std::abs(f.document.findVertex(a)->position.x - 36) <= 1e-6 &&
            std::abs(f.document.findVertex(b)->position.x - 44) <= 1e-6 &&
            f.document.findVertex(untouched)->position.x == 60, "Wrong cross-polygon correction");
    }

    void supportsRedundantConstraints()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, repeatedRows);
        requireSolved(f.document, constraints, result);
    }

    void conflictingConstraintsRollBack()
    {
        Fixture f;
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{f.edge, 7}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, repeatedRows);
        require(result.status == SolveStatus::NoConvergence && result.iterations <= 50, "Conflict reported as solved");
        requireSame(f.document, before);
    }

    void iterationLimitRollsBackAcceptedPreview()
    {
        Fixture f;
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {1e-12, 1}, forwardRow);
        require(result.status == SolveStatus::NoConvergence && result.iterations == 1, "Iteration limit ignored");
        requireSame(f.document, before);
    }

    void rejectsUnusableMatrices()
    {
        for (const JacobianBuilder builder : {failedSampling, noRows, shortRow, invalidRow, zeroRow})
        {
            Fixture f;
            const Document before = f.document;
            const std::array constraints{FixedLengthConstraint{f.edge, 5}};
            const auto result = solveWithJacobian(f.document, constraints, f.start, {}, builder);
            require(result.status == SolveStatus::NoConvergence && result.iterations == 0, "Unusable matrix was accepted");
            requireSame(f.document, before);
        }
    }

    void rejectsNonImprovingSteps()
    {
        Fixture f;
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, reverseRow);
        require(result.status == SolveStatus::NoConvergence && result.iterations == 0, "Increasing error was accepted");
        requireSame(f.document, before);
    }

    void backtracksOversizedCorrections()
    {
        Fixture f;
        // A full correction from the deliberately underscaled row worsens the error.
        // Keep the target near the initial length to avoid changing the edge's orientation.
        const std::array constraints{FixedLengthConstraint{f.edge, 9}};
        const auto result = solveWithJacobian(f.document, constraints, f.start, {}, underscaledRow);
        requireSolved(f.document, constraints, result);
        require(f.document.findVertex(f.start)->position.x == 0, "Pin moved during backtracking");
    }

    void exceptionsPreserveDocument()
    {
        Fixture f;
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        bool thrown = false;
        try { (void)solveWithJacobian(f.document, constraints, f.start, {}, throwingBuilder); }
        catch (const std::runtime_error&) { thrown = true; }
        require(thrown, "Expected matrix builder exception");
        requireSame(f.document, before);
    }

    void scalesSmallAndLargeScenes()
    {
        for (const double scale : {1e-100, 1e200})
        {
            Fixture f{scale};
            const std::array constraints{FixedLengthConstraint{f.edge, 5 * scale}};
            const double tolerance = scale * 1e-10;
            const auto result = solveWithJacobian(f.document, constraints, f.start, {tolerance, 50}, forwardRow);
            requireSolved(f.document, constraints, result, tolerance);
            require(f.document.findVertex(f.start)->position.x == 0 &&
                std::abs(f.document.findVertex(f.end)->position.x / scale - 5) <= 1e-10, "Scene scale changed the result");
        }
    }

    void publicSolverMovesDiagonalEnd()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}), "Fixture move failed");
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        requireSolved(f.document, constraints, result);
        const Vec2 start = f.document.findVertex(f.start)->position;
        const Vec2 end = f.document.findVertex(f.end)->position;
        require(start.x == 0 && start.y == 0, "Pinned vertex moved");
        require(std::abs(end.x - 3) < 2e-6 && std::abs(end.y - 4) < 2e-6, "Wrong diagonal correction");
        require(f.document.findVertex(f.other)->position.x == 20, "Unconstrained vertex moved");
    }

    void publicSolverMovesDiagonalStart()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}), "Fixture move failed");
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.end);
        requireSolved(f.document, constraints, result);
        const Vec2 start = f.document.findVertex(f.start)->position;
        const Vec2 end = f.document.findVertex(f.end)->position;
        require(end.x == 6 && end.y == 8, "Pinned endpoint moved");
        require(std::abs(start.x - 3) < 2e-6 && std::abs(start.y - 4) < 2e-6, "Wrong start correction");
    }

    void publicSolverSolvesTriangle()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}) && f.document.moveVertex(f.other, {12, 5}), "Fixture move failed");
        const auto* polygon = f.document.findPolygon(f.polygon);
        const std::array constraints{FixedLengthConstraint{polygon->edges[0], 3},
            FixedLengthConstraint{polygon->edges[1], 4}, FixedLengthConstraint{polygon->edges[2], 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        requireSolved(f.document, constraints, result);
        require(f.document.findVertex(f.start)->position.x == 0 && f.document.findVertex(f.start)->position.y == 0,
            "Triangle pin moved");
    }

    void publicSolverHandlesSeveralPolygons()
    {
        Fixture f;
        const std::array points{Vec2{30, 20}, Vec2{42, 36}, Vec2{60, 50}};
        const auto* second = f.document.findPolygon(f.document.addPolygon(points));
        const VertexId untouched = second->vertices[2];
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{second->edges[0], 10}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        requireSolved(f.document, constraints, result);
        require(f.document.findVertex(f.start)->position.x == 0 && f.document.findVertex(f.start)->position.y == 0,
            "Pin moved across polygons");
        require(f.document.findVertex(untouched)->position.x == 60 && f.document.findVertex(untouched)->position.y == 50,
            "Unrelated vertex of second polygon moved");
    }

    void publicSolverHandlesRedundantConstraints()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}), "Fixture move failed");
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        requireSolved(f.document, constraints, result);
    }

    void publicSolverRollsBackConflictingConstraints()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}), "Fixture move failed");
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}, FixedLengthConstraint{f.edge, 7}};
        const auto result = solveFixedLengths(f.document, constraints, f.start);
        require(result.status == SolveStatus::NoConvergence && result.iterations <= 50, "Conflicting lengths accepted");
        requireSame(f.document, before);
    }

    void publicSolverRollsBackAtIterationLimit()
    {
        Fixture f;
        require(f.document.moveVertex(f.end, {6, 8}), "Fixture move failed");
        const Document before = f.document;
        const std::array constraints{FixedLengthConstraint{f.edge, 5}};
        const auto result = solveFixedLengths(f.document, constraints, f.start, {1e-12, 1});
        require(result.status == SolveStatus::NoConvergence && result.iterations == 1, "Iteration limit ignored");
        requireSame(f.document, before);
    }

    void publicSolverSupportsSmallAndLargeScales()
    {
        for (const double scale : {1e-100, 1e150})
        {
            Fixture f{scale};
            require(f.document.moveVertex(f.end, {6 * scale, 8 * scale}), "Fixture move failed");
            const std::array constraints{FixedLengthConstraint{f.edge, 5 * scale}};
            const double tolerance = 1e-10 * scale;
            const auto result = solveFixedLengths(f.document, constraints, f.start, {tolerance, 50});
            requireSolved(f.document, constraints, result, tolerance);
            require(f.document.findVertex(f.start)->position.x == 0 && f.document.findVertex(f.start)->position.y == 0,
                "Scaled pin moved");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> constraintSolverTests()
    {
        static const std::array tests{
            TestCase{"empty constraint set", emptyConstraintsConvergeWithoutIterations},
            TestCase{"satisfied input with zero budget needs no Jacobian", satisfiedInputNeedsNoJacobian},
            TestCase{"invalid tolerances", rejectsInvalidTolerances},
            TestCase{"missing or non-finite fixed vertex", rejectsInvalidFixedVertex},
            TestCase{"all constraints validated before success", validatesEveryConstraintBeforeReportingSuccess},
            TestCase{"broken document topology", rejectsBrokenTopology},
            TestCase{"zero budget preserves document", zeroBudgetDoesNotChangeDocument},
            TestCase{"zero-length edge is degenerate", zeroLengthNeedsDirection},
            TestCase{"known matrix with pinned start", solvesOneEdgeWithPinnedStart},
            TestCase{"known matrix with pinned end", solvesOneEdgeWithPinnedEnd},
            TestCase{"free endpoints share correction", freeEndpointsShareCorrection},
            TestCase{"coupled lengths use one system", solvesCoupledLengthsTogether},
            TestCase{"constraints across polygons", solvesConstraintsAcrossPolygons},
            TestCase{"redundant constraints", supportsRedundantConstraints},
            TestCase{"conflicting constraints roll back", conflictingConstraintsRollBack},
            TestCase{"iteration limit discards accepted preview", iterationLimitRollsBackAcceptedPreview},
            TestCase{"failed sampling and unusable matrices", rejectsUnusableMatrices},
            TestCase{"non-improving steps are rejected", rejectsNonImprovingSteps},
            TestCase{"backtracking reduces oversized corrections", backtracksOversizedCorrections},
            TestCase{"builder exceptions preserve document", exceptionsPreserveDocument},
            TestCase{"normalization supports small and large scenes", scalesSmallAndLargeScenes},
            TestCase{"public solver with computed Jacobian: diagonal end", publicSolverMovesDiagonalEnd},
            TestCase{"public solver with computed Jacobian: diagonal start", publicSolverMovesDiagonalStart},
            TestCase{"public solver with computed Jacobian: coupled triangle", publicSolverSolvesTriangle},
            TestCase{"public solver with computed Jacobian: multiple polygons", publicSolverHandlesSeveralPolygons},
            TestCase{"public solver with computed Jacobian: redundant lengths", publicSolverHandlesRedundantConstraints},
            TestCase{"public solver with computed Jacobian: conflicting lengths roll back", publicSolverRollsBackConflictingConstraints},
            TestCase{"public solver with computed Jacobian: iteration limit rolls back", publicSolverRollsBackAtIterationLimit},
            TestCase{"public solver with computed Jacobian: small and large scales", publicSolverSupportsSmallAndLargeScales}
        };
        return tests;
    }
}
