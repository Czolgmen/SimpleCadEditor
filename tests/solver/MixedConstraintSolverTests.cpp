#include <cad_core/model/Document.h>
#include <cad_core/solver/ConstraintSolver.h>

#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace
{
    using namespace cad;
    using namespace cad::solver;
    using constraint::FixedLengthConstraint;
    using constraint::ParallelConstraint;
    using cad::tests::require;

    struct Scene
    {
        Document document;
        std::array<EdgeId, 2> edges;
        std::array<VertexId, 4> endpoints;
        std::array<VertexId, 2> untouched;
        double scale;

        explicit Scene(double size = 1) : scale(size)
        {
            const std::array firstPoints{Vec2{0, 0}, Vec2{10 * size, 0}, Vec2{0, 10 * size}};
            const std::array secondPoints{Vec2{30 * size, 0}, Vec2{38 * size, 6 * size}, Vec2{30 * size, 10 * size}};
            const auto* first = document.findPolygon(document.addPolygon(firstPoints));
            const auto* second = document.findPolygon(document.addPolygon(secondPoints));
            edges = {first->edges[0], second->edges[0]};
            endpoints = {first->vertices[0], first->vertices[1], second->vertices[0], second->vertices[1]};
            untouched = {first->vertices[2], second->vertices[2]};
        }

        std::array<FixedLengthConstraint, 2> lengths() const
        {
            return {FixedLengthConstraint{edges[0], 10 * scale}, FixedLengthConstraint{edges[1], 10 * scale}};
        }

        std::array<ParallelConstraint, 1> parallels() const
        {
            return {ParallelConstraint{edges[0], edges[1]}};
        }
    };

    void requirePosition(const Document& document, VertexId id, Vec2 expected)
    {
        const auto* vertex = document.findVertex(id);
        require(vertex && vertex->position.x == expected.x && vertex->position.y == expected.y,
            "A pinned or unrelated vertex moved");
    }

    void requireSameDocument(const Document& actual, const Document& expected)
    {
        require(actual.getPolygons().size() == expected.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : expected.getPolygons())
        {
            const auto* after = actual.findPolygon(id);
            require(after && after->id == polygon.id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Topology changed during rejected solve");
            for (const VertexId vertex : polygon.vertices)
                requirePosition(actual, vertex, expected.findVertex(vertex)->position);
            for (const EdgeId edge : polygon.edges)
            {
                const auto* a = actual.findEdge(edge);
                const auto* b = expected.findEdge(edge);
                require(a && a->id == b->id && a->start == b->start && a->end == b->end, "Edge topology changed");
            }
        }
        const auto a = actual.getFixedLengthConstraints();
        const auto b = expected.getFixedLengthConstraints();
        require(a.size() == b.size(), "Stored relation count changed");
        for (std::size_t i = 0; i < a.size(); ++i)
            require(a[i].edge == b[i].edge && a[i].length == b[i].length, "Stored relation changed");
    }

    void requireSolved(const Document& document, std::span<const FixedLengthConstraint> lengths,
        std::span<const ParallelConstraint> parallels, SolveResult result, const SolveOptions& options = {})
    {
        require(result.status == SolveStatus::Converged,
            "Mixed solver did not converge: status=" + std::to_string(static_cast<int>(result.status)) +
            ", iterations=" + std::to_string(result.iterations));
        require(document.validate().empty(), "Solution invalidated the document");
        for (const auto& relation : lengths)
        {
            const auto error = constraint::fixedLengthError(document, relation);
            require(error && std::isfinite(*error) && std::abs(*error) <= options.lengthTolerance,
                "Converged with an unsatisfied length");
        }
        for (const auto& relation : parallels)
        {
            const auto error = constraint::parallelError(document, relation);
            require(error && std::isfinite(*error) && std::abs(*error) <= options.parallelTolerance,
                "Converged with an unsatisfied parallel relation");
        }
    }

    void emptyAndSatisfiedInputsNeedNoIterations()
    {
        Scene scene;
        const Document before = scene.document;
        const auto empty = solveConstraints(scene.document, {}, {}, scene.endpoints[0], {1e-6, 0, 1e-6});
        require(empty.status == SolveStatus::Converged && empty.iterations == 0, "Empty solve failed");
        requireSameDocument(scene.document, before);
        for (const double direction : {-1.0, 1.0})
        {
            require(scene.document.moveVertex(scene.endpoints[3], {30 + direction * 10, 0}), "Fixture move failed");
            const auto lengths = scene.lengths();
            const auto parallels = scene.parallels();
            const auto result = solveConstraints(scene.document, lengths, parallels, scene.endpoints[0], {1e-6, 0, 1e-6});
            requireSolved(scene.document, lengths, parallels, result);
            require(result.iterations == 0, "Satisfied parallel/antiparallel input consumed iterations");
        }
    }

    void invalidOptionsAreRejectedWithoutMutation()
    {
        Scene scene;
        const Document before = scene.document;
        for (const double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        {
            for (const bool parallelOption : {false, true})
            {
                SolveOptions options;
                (parallelOption ? options.parallelTolerance : options.lengthTolerance) = invalid;
                const auto result = solveConstraints(scene.document, scene.lengths(), scene.parallels(), scene.endpoints[0], options);
                require(result.status == SolveStatus::InvalidInput && result.iterations == 0, "Invalid tolerance accepted");
                requireSameDocument(scene.document, before);
            }
        }
    }

    void invalidPinAndTopologyAreRejected()
    {
        Scene scene;
        const Document before = scene.document;
        for (const VertexId missing : {VertexId{}, VertexId{999999}})
            require(solveConstraints(scene.document, scene.lengths(), scene.parallels(), missing).status == SolveStatus::InvalidInput,
                "Unknown pin accepted");
        requireSameDocument(scene.document, before);
        auto* polygon = const_cast<Polygon*>(scene.document.findPolygon(scene.document.getPolygons().begin()->first));
        polygon->edges.pop_back();
        require(solveConstraints(scene.document, {}, {}, scene.endpoints[0]).status == SolveStatus::InvalidInput,
            "Invalid topology accepted");
    }

    void invalidRelationsAreRejectedBeforeSuccess()
    {
        Scene scene;
        const Document before = scene.document;
        const std::array badLengths{FixedLengthConstraint{scene.edges[0], -1}};
        require(solveConstraints(scene.document, badLengths, {}, scene.endpoints[0]).status == SolveStatus::InvalidInput,
            "Invalid target length accepted");
        for (const ParallelConstraint bad : {ParallelConstraint{scene.edges[0], scene.edges[0]},
            ParallelConstraint{EdgeId{}, scene.edges[1]}, ParallelConstraint{scene.edges[0], EdgeId{999999}}})
        {
            const std::array parallels{bad};
            require(solveConstraints(scene.document, scene.lengths(), parallels, scene.endpoints[0]).status == SolveStatus::InvalidInput,
                "Invalid parallel relation accepted");
            requireSameDocument(scene.document, before);
        }
    }

    void zeroParallelDirectionIsDiagnosed()
    {
        for (const bool zeroFirst : {false, true})
        {
            Scene scene;
            const std::size_t index = zeroFirst ? 0 : 2;
            const Vec2 start = scene.document.findVertex(scene.endpoints[index])->position;
            require(scene.document.moveVertex(scene.endpoints[index + 1], start), "Fixture move failed");
            const Document before = scene.document;
            const auto result = solveConstraints(scene.document, {}, scene.parallels(), scene.endpoints[0]);
            require(result.status == SolveStatus::DegenerateGeometry && result.iterations == 0, "Zero parallel direction not diagnosed");
            requireSameDocument(scene.document, before);
        }
    }

    void overflowingDirectionIsRejected()
    {
        Scene scene;
        const double maximum = std::numeric_limits<double>::max();
        require(scene.document.moveVertex(scene.endpoints[2], {maximum, 0}) &&
            scene.document.moveVertex(scene.endpoints[3], {-maximum, 0}), "Fixture move failed");
        const Document before = scene.document;
        const auto result = solveConstraints(scene.document, {}, scene.parallels(), scene.endpoints[0]);
        require(result.status == SolveStatus::InvalidInput, "Overflowing direction accepted");
        requireSameDocument(scene.document, before);
    }

    void parallelOnlySolveAdaptsBothPolygons()
    {
        Scene scene;
        const Document before = scene.document;
        const auto parallels = scene.parallels();
        const auto result = solveConstraints(scene.document, {}, parallels, scene.endpoints[0]);
        requireSolved(scene.document, {}, parallels, result);
        requirePosition(scene.document, scene.endpoints[0], {0, 0});
        for (const VertexId vertex : scene.untouched)
            requirePosition(scene.document, vertex, before.findVertex(vertex)->position);
    }

    void lengthsAndParallelismAreSolvedTogether()
    {
        Scene scene;
        const Document before = scene.document;
        const auto lengths = scene.lengths();
        const auto parallels = scene.parallels();
        const auto result = solveConstraints(scene.document, lengths, parallels, scene.endpoints[0]);
        requireSolved(scene.document, lengths, parallels, result);
        requirePosition(scene.document, scene.endpoints[0], {0, 0});
        for (const VertexId vertex : scene.untouched)
            requirePosition(scene.document, vertex, before.findVertex(vertex)->position);
    }

    void adjacentEdgesSolveWithSharedVertex()
    {
        Document document;
        const std::array points{Vec2{0, 0}, Vec2{6, 8}, Vec2{6, 18}};
        const auto* polygon = document.findPolygon(document.addPolygon(points));
        const VertexId pin = polygon->vertices[0];
        const std::array lengths{FixedLengthConstraint{polygon->edges[0], 10}, FixedLengthConstraint{polygon->edges[1], 10}};
        const std::array parallels{ParallelConstraint{polygon->edges[0], polygon->edges[1]}};
        const auto result = solveConstraints(document, lengths, parallels, pin);
        requireSolved(document, lengths, parallels, result);
        requirePosition(document, pin, {0, 0});
    }

    void redundantClosedCycleConverges()
    {
        Document document;
        const std::array points{Vec2{0, 0}, Vec2{10, 1}, Vec2{12, 11}, Vec2{0, 10}};
        const auto* polygon = document.findPolygon(document.addPolygon(points));
        const VertexId pin = polygon->vertices[0];
        const std::array lengths{FixedLengthConstraint{polygon->edges[0], 10}, FixedLengthConstraint{polygon->edges[1], 10},
            FixedLengthConstraint{polygon->edges[2], 10}, FixedLengthConstraint{polygon->edges[3], 10}};
        const std::array parallels{ParallelConstraint{polygon->edges[0], polygon->edges[2]},
            ParallelConstraint{polygon->edges[1], polygon->edges[3]}, ParallelConstraint{polygon->edges[0], polygon->edges[2]}};
        const auto result = solveConstraints(document, lengths, parallels, pin);
        requireSolved(document, lengths, parallels, result);
        requirePosition(document, pin, {0, 0});
    }

    void coupledParallelPairsAcrossThreePolygons()
    {
        Scene scene;
        const std::array points{Vec2{50, 0}, Vec2{56, -8}, Vec2{50, 10}};
        const EdgeId third = scene.document.findPolygon(scene.document.addPolygon(points))->edges[0];
        const std::array parallels{ParallelConstraint{scene.edges[0], scene.edges[1]}, ParallelConstraint{scene.edges[1], third}};
        const std::array lengths{FixedLengthConstraint{scene.edges[0], 10}, FixedLengthConstraint{scene.edges[1], 10},
            FixedLengthConstraint{third, 10}};
        const auto result = solveConstraints(scene.document, lengths, parallels, scene.endpoints[0]);
        requireSolved(scene.document, lengths, parallels, result);
        requirePosition(scene.document, scene.endpoints[0], {0, 0});
    }

    void unrelatedPinAndUnconstrainedVerticesStayFixed()
    {
        Scene scene;
        const Document before = scene.document;
        const auto parallels = scene.parallels();
        const auto result = solveConstraints(scene.document, {}, parallels, scene.untouched[0]);
        requireSolved(scene.document, {}, parallels, result);
        for (const VertexId vertex : scene.untouched)
            requirePosition(scene.document, vertex, before.findVertex(vertex)->position);
    }

    void perpendicularStartingDirectionsConverge()
    {
        for (const double size : {2.0, 10.0})
        {
            for (const bool pinOnEdge : {false, true})
            {
                Document document;
                const std::array firstPoints{Vec2{0, 0}, Vec2{size, 0}, Vec2{0, size}};
                const std::array secondPoints{Vec2{3 * size, 0}, Vec2{3 * size, size}, Vec2{4 * size, 0}};
                const auto* first = document.findPolygon(document.addPolygon(firstPoints));
                const auto* second = document.findPolygon(document.addPolygon(secondPoints));
                const VertexId pin = first->vertices[pinOnEdge ? 0 : 2];
                const Vec2 pinnedPosition = document.findVertex(pin)->position;
                const std::array lengths{FixedLengthConstraint{first->edges[0], size}, FixedLengthConstraint{second->edges[0], size}};
                const std::array parallels{ParallelConstraint{first->edges[0], second->edges[0]}};
                const Document before = document;
                const auto limited = solveConstraints(document, lengths, parallels, pin, {1e-12, 1, 1e-12});
                require(limited.status == SolveStatus::NoConvergence && limited.iterations == 1,
                    "Perpendicular escape ignored the iteration budget");
                requireSameDocument(document, before);
                const auto result = solveConstraints(document, lengths, parallels, pin);
                requireSolved(document, lengths, parallels, result);
                requirePosition(document, pin, pinnedPosition);
            }
        }
    }

    void separateTolerancesAreRespected()
    {
        Scene scene;
        require(scene.document.moveVertex(scene.endpoints[3], {40, 0.001}), "Fixture move failed");
        const Document before = scene.document;
        const auto parallels = scene.parallels();
        const auto strictAngle = solveConstraints(scene.document, {}, parallels, scene.endpoints[0], {1, 0, 1e-6});
        require(strictAngle.status == SolveStatus::NoConvergence, "Length tolerance was applied to angular error");
        requireSameDocument(scene.document, before);
        const auto looseAngle = solveConstraints(scene.document, {}, parallels, scene.endpoints[0], {1e-12, 0, 1e-3});
        require(looseAngle.status == SolveStatus::Converged && looseAngle.iterations == 0, "Parallel tolerance ignored");
        const std::array lengths{FixedLengthConstraint{scene.edges[0], 10.001}};
        const auto strictLength = solveConstraints(scene.document, lengths, {}, scene.endpoints[0], {1e-6, 0, 1});
        require(strictLength.status == SolveStatus::NoConvergence, "Parallel tolerance was applied to length error");
        requireSameDocument(scene.document, before);
    }

    void iterationLimitDiscardsAcceptedCorrections()
    {
        Scene scene;
        const Document before = scene.document;
        const auto result = solveConstraints(scene.document, scene.lengths(), scene.parallels(), scene.endpoints[0], {1e-12, 1, 1e-12});
        require(result.status == SolveStatus::NoConvergence && result.iterations == 1, "Iteration limit ignored");
        requireSameDocument(scene.document, before);
    }

    void zeroBudgetPreservesInput()
    {
        Scene scene;
        const Document before = scene.document;
        const auto result = solveConstraints(scene.document, scene.lengths(), scene.parallels(), scene.endpoints[0], {1e-6, 0, 1e-6});
        require(result.status == SolveStatus::NoConvergence && result.iterations == 0, "Zero budget ignored");
        requireSameDocument(scene.document, before);
    }

    void conflictingLengthsAndParallelismRollBack()
    {
        Document document;
        const std::array points{Vec2{0, 0}, Vec2{3, 0}, Vec2{3, 4}};
        const auto* polygon = document.findPolygon(document.addPolygon(points));
        const VertexId pin = polygon->vertices[0];
        const std::array lengths{FixedLengthConstraint{polygon->edges[0], 3}, FixedLengthConstraint{polygon->edges[1], 4},
            FixedLengthConstraint{polygon->edges[2], 5}};
        for (const auto& relation : lengths)
            require(document.setFixedLength(relation.edge, relation.length), "Fixture relation failed");
        const Document before = document;
        const std::array parallels{ParallelConstraint{polygon->edges[0], polygon->edges[1]}};
        const auto result = solveConstraints(document, lengths, parallels, pin);
        require(result.status == SolveStatus::NoConvergence && result.iterations <= SolveOptions{}.maxIterations,
            "Conflicting system accepted or exceeded the configured iteration budget");
        requireSameDocument(document, before);
    }

    void smallAndLargeScenesUseConsistentScaling()
    {
        for (const double scale : {1e-100, 1.0, 1e100})
        {
            Scene scene{scale};
            const auto lengths = scene.lengths();
            const auto parallels = scene.parallels();
            const SolveOptions options{1e-9 * scale, 50, 1e-8};
            const auto result = solveConstraints(scene.document, lengths, parallels, scene.endpoints[0], options);
            requireSolved(scene.document, lengths, parallels, result, options);
            requirePosition(scene.document, scene.endpoints[0], {0, 0});
        }
    }

    void borrowedStoredLengthsSurviveCommit()
    {
        Scene scene;
        require(scene.document.setFixedLength(scene.edges[0], 12) && scene.document.setFixedLength(scene.edges[1], 8),
            "Fixture relations failed");
        const auto parallels = scene.parallels();
        const auto result = solveConstraints(scene.document, scene.document.getFixedLengthConstraints(), parallels, scene.endpoints[0]);
        // Reacquire the span after mutation; the old vector was replaced on commit.
        const auto stored = scene.document.getFixedLengthConstraints();
        requireSolved(scene.document, stored, parallels, result);
        require(stored.size() == 2 && stored[0].edge == scene.edges[0] && stored[0].length == 12 &&
            stored[1].edge == scene.edges[1] && stored[1].length == 8, "Commit changed stored relation data");
    }
}

namespace cad::tests
{
    std::span<const TestCase> mixedConstraintSolverTests()
    {
        static const std::array tests{
            TestCase{"empty, parallel and antiparallel inputs need no iterations", emptyAndSatisfiedInputsNeedNoIterations},
            TestCase{"invalid length and parallel tolerances", invalidOptionsAreRejectedWithoutMutation},
            TestCase{"invalid pin and topology", invalidPinAndTopologyAreRejected},
            TestCase{"invalid relation metadata", invalidRelationsAreRejectedBeforeSuccess},
            TestCase{"zero parallel direction is degenerate", zeroParallelDirectionIsDiagnosed},
            TestCase{"overflowing direction rejects", overflowingDirectionIsRejected},
            TestCase{"parallel-only solve across polygons", parallelOnlySolveAdaptsBothPolygons},
            TestCase{"lengths and parallelism solved together", lengthsAndParallelismAreSolvedTogether},
            TestCase{"shared vertex contributes to both relations", adjacentEdgesSolveWithSharedVertex},
            TestCase{"redundant closed cycle converges", redundantClosedCycleConverges},
            TestCase{"coupled pairs across three polygons", coupledParallelPairsAcrossThreePolygons},
            TestCase{"unrelated pin and unconstrained vertices stay fixed", unrelatedPinAndUnconstrainedVerticesStayFixed},
            TestCase{"perpendicular starting directions converge", perpendicularStartingDirectionsConverge},
            TestCase{"separate length and angle tolerances", separateTolerancesAreRespected},
            TestCase{"iteration limit discards accepted corrections", iterationLimitDiscardsAcceptedCorrections},
            TestCase{"zero budget preserves input", zeroBudgetPreservesInput},
            TestCase{"conflicting mixed constraints roll back", conflictingLengthsAndParallelismRollBack},
            TestCase{"small and large scenes use consistent scaling", smallAndLargeScenesUseConsistentScaling},
            TestCase{"borrowed stored lengths survive commit", borrowedStoredLengthsSurviveCommit}
        };
        return tests;
    }
}
