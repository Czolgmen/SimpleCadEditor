#include <cad_core/constraints/ParallelConstraint.h>
#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad;
    using constraint::ParallelConstraint;
    using constraint::parallelError;
    using cad::tests::require;

    EdgeId addEdge(Document& document, Vec2 start, Vec2 end)
    {
        const std::array positions{start, end, Vec2{11, 13}};
        return document.findPolygon(document.addPolygon(positions))->edges.front();
    }

    struct Fixture
    {
        Document document;
        EdgeId first;
        EdgeId second;

        Fixture(Vec2 firstDirection = {3, 4}, Vec2 secondDirection = {6, 8},
            Vec2 firstOrigin = {}, Vec2 secondOrigin = {})
        {
            first = addEdge(document, firstOrigin, firstOrigin + firstDirection);
            second = addEdge(document, secondOrigin, secondOrigin + secondDirection);
        }
    };

    void requireErrorNear(const std::optional<double>& result, double expected, double tolerance = 1e-12)
    {
        require(result.has_value(), "Valid parallel relation did not produce a residual");
        require(std::isfinite(*result) && std::abs(*result - expected) <= tolerance,
            "Unexpected signed parallel residual");
    }

    void sameDirectionIsParallel()
    {
        for (const Vec2 direction : {Vec2{1, 0}, Vec2{0, -1}, Vec2{3, 4}})
        {
            const Fixture f{direction, direction * 7};
            requireErrorNear(parallelError(f.document, {f.first, f.second}), 0);
        }
    }

    void oppositeDirectionIsParallel()
    {
        for (const Vec2 direction : {Vec2{1, 0}, Vec2{0, -1}, Vec2{3, 4}})
        {
            const Fixture f{direction, direction * -7};
            requireErrorNear(parallelError(f.document, {f.first, f.second}), 0);
        }
    }

    void perpendicularDirectionsHaveSignedUnitError()
    {
        const Fixture leftTurn{{2, 0}, {0, 5}};
        requireErrorNear(parallelError(leftTurn.document, {leftTurn.first, leftTurn.second}), 1);
        const Fixture rightTurn{{2, 0}, {0, -5}};
        requireErrorNear(parallelError(rightTurn.document, {rightTurn.first, rightTurn.second}), -1);
    }

    void obliqueDirectionsHaveKnownResidual()
    {
        const Fixture positive{{3, 4}, {0, 2}};
        requireErrorNear(parallelError(positive.document, {positive.first, positive.second}), 0.6);
        const Fixture negative{{3, 4}, {0, -2}};
        requireErrorNear(parallelError(negative.document, {negative.first, negative.second}), -0.6);
        const Fixture general{{2, 3}, {4, -5}};
        requireErrorNear(parallelError(general.document, {general.first, general.second}), -22 / std::sqrt(533.0));
    }

    void swappingAndReversingDirectionsChangesSign()
    {
        const Fixture f{{3, 4}, {0, 2}};
        requireErrorNear(parallelError(f.document, {f.first, f.second}), 0.6);
        requireErrorNear(parallelError(f.document, {f.second, f.first}), -0.6);
        const Fixture reversedFirst{{-3, -4}, {0, 2}};
        requireErrorNear(parallelError(reversedFirst.document, {reversedFirst.first, reversedFirst.second}), -0.6);
        const Fixture reversedBoth{{-3, -4}, {0, -2}};
        requireErrorNear(parallelError(reversedBoth.document, {reversedBoth.first, reversedBoth.second}), 0.6);
    }

    void independentLengthScalesDoNotAffectError()
    {
        for (const double firstScale : {1e-200, 1.0, 1e200})
        {
            for (const double secondScale : {1e-200, 1.0, 1e200})
            {
                const Fixture f{{3 * firstScale, 4 * firstScale}, {0, 2 * secondScale}};
                requireErrorNear(parallelError(f.document, {f.first, f.second}), 0.6);
            }
        }
    }

    void translationAndFractionalOriginsDoNotAffectError()
    {
        const Fixture f{{3, 4}, {0, 2}, {-20.5, 13.25}, {0.125, -2.5}};
        require(f.document.getPolygons().size() == 2, "Fixture must use different polygons");
        requireErrorNear(parallelError(f.document, {f.first, f.second}), 0.6);
    }

    void adjacentEdgesCanShareAVertex()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{3, 4}, Vec2{3, 6}};
        const auto* polygon = document.findPolygon(document.addPolygon(positions));
        requireErrorNear(parallelError(document, {polygon->edges[0], polygon->edges[1]}), 0.6);
    }

    void rejectsSameEdgeTwice()
    {
        const Fixture f;
        for (const EdgeId edge : {f.first, f.second, EdgeId{}})
            require(!parallelError(f.document, {edge, edge}), "Self-relation was accepted");
    }

    void rejectsMissingEdgesInEitherSlot()
    {
        const Fixture f;
        const Document empty;
        require(!parallelError(empty, {EdgeId{1}, EdgeId{2}}), "Empty document accepted a relation");
        for (const EdgeId missing : {EdgeId{}, EdgeId{999999}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!parallelError(f.document, {missing, f.second}), "Missing first edge was accepted");
            require(!parallelError(f.document, {f.first, missing}), "Missing second edge was accepted");
        }
    }

    void rejectsMissingEndpointInEitherEdge()
    {
        for (const bool changeFirst : {true, false})
        {
            for (const bool changeStart : {true, false})
            {
                Fixture f;
                // Public mutations preserve references; invalid fixtures alone bypass the read-only API.
                auto* edge = const_cast<Edge*>(f.document.findEdge(changeFirst ? f.first : f.second));
                (changeStart ? edge->start : edge->end) = VertexId{999999};
                require(!parallelError(f.document, {f.first, f.second}), "Missing endpoint was accepted");
            }
        }
    }

    void rejectsEitherOrBothZeroLengthEdges()
    {
        for (const auto& directions : std::array{
            std::array{Vec2{}, Vec2{1, 0}}, std::array{Vec2{1, 0}, Vec2{}}, std::array{Vec2{}, Vec2{}}})
        {
            const Fixture f{directions[0], directions[1], {2, -3}, {-4, 5}};
            require(!parallelError(f.document, {f.first, f.second}), "Undefined zero-length direction was accepted");
        }
    }

    void rejectsNonFiniteCoordinatesOfAllEndpoints()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            for (const bool changeFirst : {true, false})
            {
                for (const bool changeStart : {true, false})
                {
                    for (const bool changeX : {true, false})
                    {
                        Fixture f;
                        const auto* edge = f.document.findEdge(changeFirst ? f.first : f.second);
                        auto* vertex = const_cast<Vertex*>(f.document.findVertex(changeStart ? edge->start : edge->end));
                        (changeX ? vertex->position.x : vertex->position.y) = invalid;
                        require(!parallelError(f.document, {f.first, f.second}), "Non-finite coordinate was accepted");
                    }
                }
            }
        }
    }

    void rejectsOverflowingCoordinateDifferences()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool changeFirst : {true, false})
        {
            for (const bool changeX : {true, false})
            {
                for (const double sign : {-1.0, 1.0})
                {
                    Fixture f;
                    const auto* edge = f.document.findEdge(changeFirst ? f.first : f.second);
                    const Vec2 start = changeX ? Vec2{sign * maximum, 0} : Vec2{0, sign * maximum};
                    require(f.document.moveVertex(edge->start, start) && f.document.moveVertex(edge->end, start * -1),
                        "Fixture move failed");
                    require(!parallelError(f.document, {f.first, f.second}), "Overflowing direction was accepted");
                }
            }
        }
    }

    void rejectsUnrepresentableLength()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool changeFirst : {true, false})
        {
            Fixture f;
            const auto* edge = f.document.findEdge(changeFirst ? f.first : f.second);
            require(f.document.moveVertex(edge->end, {maximum, maximum}), "Fixture move failed");
            require(!parallelError(f.document, {f.first, f.second}), "Unrepresentable length was accepted");
        }
    }

    void smallNonzeroErrorIsNotRoundedAway()
    {
        const Fixture f{{1, 0}, {1, 1e-12}};
        const auto result = parallelError(f.document, {f.first, f.second});
        requireErrorNear(result, 1e-12, 1e-24);
        require(*result > 0, "Small angular error was rounded to zero");
    }

    void readsCurrentGeometryAfterMovement()
    {
        Fixture f;
        const ParallelConstraint relation{f.first, f.second};
        requireErrorNear(parallelError(f.document, relation), 0);
        const VertexId secondEnd = f.document.findEdge(f.second)->end;
        require(f.document.moveVertex(secondEnd, {0, 5}), "Fixture move failed");
        requireErrorNear(parallelError(f.document, relation), 0.6);
        const VertexId firstEnd = f.document.findEdge(f.first)->end;
        require(f.document.moveVertex(firstEnd, {0, -5}), "Fixture move failed");
        requireErrorNear(parallelError(f.document, relation), 0);
    }

    void evaluationPreservesDocumentAndRelation()
    {
        Fixture f;
        require(f.document.setFixedLength(f.first, 5) && f.document.setFixedLength(f.second, 10), "Fixture relation failed");
        const Document before = f.document;
        const ParallelConstraint relation{f.first, f.second};
        for (int i = 0; i < 3; ++i)
        {
            requireErrorNear(parallelError(f.document, relation), 0);
            require(!parallelError(f.document, {f.first, f.first}), "Self-relation was accepted");
            require(!parallelError(f.document, {EdgeId{}, f.second}), "Missing edge was accepted");
        }
        require(relation.first == f.first && relation.second == f.second, "Input relation was modified");
        require(f.document.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* after = f.document.findPolygon(id);
            require(after && after->id == polygon.id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Evaluation modified topology");
            for (const VertexId vertexId : polygon.vertices)
            {
                const auto* a = f.document.findVertex(vertexId);
                const auto* b = before.findVertex(vertexId);
                require(a && a->id == b->id && a->position.x == b->position.x && a->position.y == b->position.y,
                    "Evaluation modified a vertex");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const auto* a = f.document.findEdge(edgeId);
                const auto* b = before.findEdge(edgeId);
                require(a && a->id == b->id && a->start == b->start && a->end == b->end, "Evaluation modified an edge");
            }
        }
        const auto actualLengths = f.document.getFixedLengthConstraints();
        const auto oldLengths = before.getFixedLengthConstraints();
        require(actualLengths.size() == oldLengths.size(), "Fixed-length count changed");
        for (std::size_t i = 0; i < actualLengths.size(); ++i)
            require(actualLengths[i].edge == oldLengths[i].edge && actualLengths[i].length == oldLengths[i].length,
                "Parallel evaluation changed a length relation");
        require(f.document.validate().empty(), "Evaluation invalidated the document");
    }
}

namespace cad::tests
{
    std::span<const TestCase> parallelConstraintTests()
    {
        static const std::array tests{
            TestCase{"parallel axes and diagonal", sameDirectionIsParallel},
            TestCase{"antiparallel axes and diagonal", oppositeDirectionIsParallel},
            TestCase{"signed perpendicular directions", perpendicularDirectionsHaveSignedUnitError},
            TestCase{"known oblique residuals", obliqueDirectionsHaveKnownResidual},
            TestCase{"swapping edges and reversing directions", swappingAndReversingDirectionsChangesSign},
            TestCase{"independent small and large length scales", independentLengthScalesDoNotAffectError},
            TestCase{"translation and fractional origins across polygons", translationAndFractionalOriginsDoNotAffectError},
            TestCase{"adjacent edges sharing a vertex", adjacentEdgesCanShareAVertex},
            TestCase{"same edge twice", rejectsSameEdgeTwice},
            TestCase{"missing edges in either slot and empty document", rejectsMissingEdgesInEitherSlot},
            TestCase{"missing endpoints of either edge", rejectsMissingEndpointInEitherEdge},
            TestCase{"either or both directions have zero length", rejectsEitherOrBothZeroLengthEdges},
            TestCase{"non-finite coordinates of every endpoint", rejectsNonFiniteCoordinatesOfAllEndpoints},
            TestCase{"overflowing coordinate differences on either edge", rejectsOverflowingCoordinateDifferences},
            TestCase{"unrepresentable length on either edge", rejectsUnrepresentableLength},
            TestCase{"small nonzero angular error", smallNonzeroErrorIsNotRoundedAway},
            TestCase{"current geometry after movement", readsCurrentGeometryAfterMovement},
            TestCase{"repeated evaluation preserves geometry and existing relations", evaluationPreservesDocumentAndRelation}
        };
        return tests;
    }
}
