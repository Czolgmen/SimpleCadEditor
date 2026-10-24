#include <cad_core/constraints/FixedLengthConstraint.h>
#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad;
    using cad::constraint::FixedLengthConstraint;
    using cad::constraint::fixedLengthError;
    using cad::tests::require;

    struct Fixture
    {
        Document document;
        EdgeId edge;

        Fixture(Vec2 start = {0, 0}, Vec2 end = {3, 4})
        {
            const std::array positions{start, end, Vec2{11, 13}};
            const PolygonId polygon = document.addPolygon(positions);
            edge = document.findPolygon(polygon)->edges.front();
        }
    };

    void requireErrorNear(const std::optional<double>& result, double expected)
    {
        require(result.has_value(), "Valid constraint did not produce a residual");
        require(std::isfinite(*result), "Residual must be finite");
        const double tolerance = 1e-12 + 1e-12 * std::max(std::abs(*result), std::abs(expected));
        require(std::abs(*result - expected) <= tolerance, "Unexpected signed length residual");
    }

    void satisfiedConstraintHasZeroError()
    {
        const Fixture fixture;
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 5}), 0);
    }

    void longerEdgeHasPositiveError()
    {
        const Fixture fixture;
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 3}), 2);
    }

    void shorterEdgeHasNegativeError()
    {
        const Fixture fixture;
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 7}), -2);
    }

    void handlesOrientationTranslationAndFractions()
    {
        const std::array starts{Vec2{2, -3}, Vec2{2, -3}, Vec2{-2.5, 1.25}, Vec2{-2.8, 1.65}};
        const std::array ends{Vec2{-3, -3}, Vec2{2, -8}, Vec2{-2.8, 1.65}, Vec2{-2.5, 1.25}};
        const std::array lengths{5.0, 5.0, 0.5, 0.5};
        for (std::size_t i = 0; i < starts.size(); ++i)
        {
            const Fixture fixture{starts[i], ends[i]};
            requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, lengths[i] / 2}), lengths[i] / 2);
        }
    }

    void zeroLengthEdgeHasComputableNegativeError()
    {
        const Fixture fixture{{2, -3}, {2, -3}};
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 7}), -7);
    }

    void acceptsTinyPositiveLengthsWithoutUnderflow()
    {
        const Fixture fixture{{0, 0}, {3e-200, 4e-200}};
        const auto result = fixedLengthError(fixture.document, {fixture.edge, 1e-200});
        require(result.has_value(), "Tiny positive target was rejected");
        // Scale before comparing so the absolute tolerance cannot hide an incorrect zero.
        requireErrorNear(*result / 1e-200, 4);
    }

    void largeRepresentableLengthDoesNotOverflow()
    {
        const Fixture fixture{{0, 0}, {3e200, 4e200}};
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 1e200}), 4e200);
    }

    void preservesSmallNonzeroResidual()
    {
        const Fixture fixture;
        const auto result = fixedLengthError(fixture.document, {fixture.edge, 5.0 + 1e-9});
        requireErrorNear(result, -1e-9);
        require(*result < 0, "Evaluator must not round a small residual to zero");
    }

    void rejectsUnknownEdges()
    {
        const Document empty;
        const Fixture fixture;
        for (const EdgeId id : {EdgeId{}, EdgeId{999999}})
        {
            require(!fixedLengthError(empty, {id, 5}), "Empty document accepted an edge");
            require(!fixedLengthError(fixture.document, {id, 5}), "Unknown edge was accepted");
        }
    }

    void rejectsInvalidTargetLengths()
    {
        const Fixture fixture;
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double length : {0.0, -0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
            require(!fixedLengthError(fixture.document, {fixture.edge, length}), "Invalid target length was accepted");
    }

    void rejectsMissingEndpoints()
    {
        for (const bool missingStart : {true, false})
        {
            Fixture fixture;
            // Only test fixtures bypass the read-only API to create an invalid reference.
            auto* edge = const_cast<Edge*>(fixture.document.findEdge(fixture.edge));
            (missingStart ? edge->start : edge->end) = VertexId{999999};
            require(!fixedLengthError(fixture.document, {fixture.edge, 5}), "Missing endpoint was accepted");
        }
    }

    void rejectsNonFiniteEndpointCoordinates()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            for (const bool changeStart : {true, false})
            {
                for (const bool changeX : {true, false})
                {
                    Fixture fixture;
                    const Edge* edge = fixture.document.findEdge(fixture.edge);
                    auto* vertex = const_cast<Vertex*>(fixture.document.findVertex(changeStart ? edge->start : edge->end));
                    (changeX ? vertex->position.x : vertex->position.y) = invalid;
                    require(!fixedLengthError(fixture.document, {fixture.edge, 5}), "Non-finite coordinate was accepted");
                }
            }
        }
    }

    void rejectsOverflowingCoordinateDifference()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool changeX : {true, false})
        {
            for (const double sign : {-1.0, 1.0})
            {
                const Vec2 start = changeX ? Vec2{sign * maximum, 0} : Vec2{0, sign * maximum};
                const Vec2 end = start * -1;
                const Fixture fixture{start, end};
                require(!fixedLengthError(fixture.document, {fixture.edge, 5}), "Overflowing difference was accepted");
            }
        }
    }

    void rejectsUnrepresentableLength()
    {
        const double maximum = std::numeric_limits<double>::max();
        const Fixture fixture{{0, 0}, {maximum, maximum}};
        require(!fixedLengthError(fixture.document, {fixture.edge, 5}), "Unrepresentable length was accepted");
    }

    void readsCurrentGeometryAfterMovement()
    {
        Fixture fixture;
        const FixedLengthConstraint constraint{fixture.edge, 5};
        requireErrorNear(fixedLengthError(fixture.document, constraint), 0);
        const VertexId end = fixture.document.findEdge(fixture.edge)->end;
        require(fixture.document.moveVertex(end, {6, 8}), "Fixture movement failed");
        requireErrorNear(fixedLengthError(fixture.document, constraint), 5);
    }

    void evaluatesRequestedEdgeAcrossPolygons()
    {
        Fixture fixture;
        const std::array positions{Vec2{20, 20}, Vec2{26, 20}, Vec2{26, 28}};
        const PolygonId id = fixture.document.addPolygon(positions);
        const Polygon* polygon = fixture.document.findPolygon(id);
        requireErrorNear(fixedLengthError(fixture.document, {fixture.edge, 5}), 0);
        requireErrorNear(fixedLengthError(fixture.document, {polygon->edges[0], 5}), 1);
        requireErrorNear(fixedLengthError(fixture.document, {polygon->edges[1], 5}), 3);
        requireErrorNear(fixedLengthError(fixture.document, {polygon->edges[2], 5}), 5);
    }

    void evaluationPreservesDocumentAndConstraint()
    {
        Fixture fixture;
        const Document before = fixture.document;
        const FixedLengthConstraint constraint{fixture.edge, 3};
        for (int i = 0; i < 3; ++i)
        {
            requireErrorNear(fixedLengthError(fixture.document, constraint), 2);
            require(!fixedLengthError(fixture.document, {fixture.edge, -1}), "Invalid target was accepted");
            require(!fixedLengthError(fixture.document, {EdgeId{}, 5}), "Invalid edge was accepted");
        }
        require(constraint.edge == fixture.edge && constraint.length == 3, "Evaluation modified the constraint");
        require(fixture.document.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const Polygon* after = fixture.document.findPolygon(id);
            require(after && after->id == id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Evaluation modified polygon topology");
            for (const VertexId vertexId : polygon.vertices)
            {
                const Vertex* oldVertex = before.findVertex(vertexId);
                const Vertex* newVertex = fixture.document.findVertex(vertexId);
                require(newVertex && newVertex->id == oldVertex->id &&
                    newVertex->position.x == oldVertex->position.x && newVertex->position.y == oldVertex->position.y,
                    "Evaluation modified a vertex");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const Edge* oldEdge = before.findEdge(edgeId);
                const Edge* newEdge = fixture.document.findEdge(edgeId);
                require(newEdge && newEdge->id == oldEdge->id &&
                    newEdge->start == oldEdge->start && newEdge->end == oldEdge->end,
                    "Evaluation modified an edge");
            }
        }
        require(fixture.document.validate().empty(), "Evaluation invalidated the document");
    }
}

namespace cad::tests
{
    std::span<const TestCase> fixedLengthConstraintTests()
    {
        static const std::array tests{
            TestCase{"satisfied constraint has zero residual", satisfiedConstraintHasZeroError},
            TestCase{"longer edge has positive residual", longerEdgeHasPositiveError},
            TestCase{"shorter edge has negative residual", shorterEdgeHasNegativeError},
            TestCase{"axis, reversed, translated and fractional edges", handlesOrientationTranslationAndFractions},
            TestCase{"zero-length edge has computable negative residual", zeroLengthEdgeHasComputableNegativeError},
            TestCase{"tiny positive lengths avoid underflow", acceptsTinyPositiveLengthsWithoutUnderflow},
            TestCase{"large representable length avoids overflow", largeRepresentableLengthDoesNotOverflow},
            TestCase{"small nonzero residual is preserved", preservesSmallNonzeroResidual},
            TestCase{"unknown edges in empty and populated documents", rejectsUnknownEdges},
            TestCase{"zero, negative and non-finite target lengths", rejectsInvalidTargetLengths},
            TestCase{"missing start and end vertices", rejectsMissingEndpoints},
            TestCase{"non-finite coordinates on both endpoints", rejectsNonFiniteEndpointCoordinates},
            TestCase{"overflowing coordinate differences", rejectsOverflowingCoordinateDifference},
            TestCase{"unrepresentable length with finite differences", rejectsUnrepresentableLength},
            TestCase{"current geometry after vertex movement", readsCurrentGeometryAfterMovement},
            TestCase{"requested edge across multiple polygons", evaluatesRequestedEdgeAcrossPolygons},
            TestCase{"repeated evaluation preserves document and constraint", evaluationPreservesDocumentAndConstraint}
        };
        return tests;
    }
}
