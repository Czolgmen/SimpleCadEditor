#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace
{
    using namespace cad;
    using cad::tests::require;

    std::array<Vec2, 3> triangle()
    {
        return {Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
    }

    void requireSameState(const Document& actual, const Document& expected, PolygonId skip = {})
    {
        require(actual.getPolygons().size() == expected.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : expected.getPolygons())
        {
            const Polygon* found = actual.findPolygon(id);
            require(found != nullptr && found->id == id, "Polygon disappeared or changed ID");
            if (id == skip)
                continue;
            require(found->vertices == polygon.vertices && found->edges == polygon.edges,
                "Polygon topology changed");
            for (const VertexId vertexId : polygon.vertices)
            {
                const Vertex* before = expected.findVertex(vertexId);
                const Vertex* after = actual.findVertex(vertexId);
                require(before && after && before->id == after->id, "Vertex disappeared or changed ID");
                require(before->position.x == after->position.x && before->position.y == after->position.y,
                    "Unrelated or rejected operation changed a position");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const Edge* before = expected.findEdge(edgeId);
                const Edge* after = actual.findEdge(edgeId);
                require(before && after && before->id == after->id &&
                        before->start == after->start && before->end == after->end,
                    "Unrelated or rejected operation changed an edge");
            }
        }
    }

    void requireCountersPreserved(Document& actual, Document expected)
    {
        actual.addPolygon(triangle());
        expected.addPolygon(triangle());
        requireSameState(actual, expected);
    }

    VertexId checkSplit(Document& document, PolygonId id, std::size_t index, const Vec2& midpoint)
    {
        const Document before = document;
        const Polygon original = *document.findPolygon(id);
        const EdgeId originalEdgeId = original.edges.at(index);
        const Edge originalEdge = *document.findEdge(originalEdgeId);
        const auto result = document.splitEdge(originalEdgeId);
        require(result.has_value() && result->value != 0, "Split did not return a valid vertex ID");
        require(std::find(original.vertices.begin(), original.vertices.end(), *result) == original.vertices.end(),
            "Split reused an existing vertex ID");
        const Vertex* added = document.findVertex(*result);
        require(added && added->id == *result &&
                added->position.x == midpoint.x && added->position.y == midpoint.y,
            "Split created an incorrect midpoint");

        const Polygon* polygon = document.findPolygon(id);
        require(polygon && polygon->vertices.size() == original.vertices.size() + 1 &&
                polygon->edges.size() == original.edges.size() + 1,
            "Split must add exactly one vertex and one edge to the cycle");
        auto expectedVertices = original.vertices;
        expectedVertices.insert(expectedVertices.begin() + static_cast<std::ptrdiff_t>(index + 1), *result);
        require(polygon->vertices == expectedVertices, "Midpoint inserted at the wrong position");

        const EdgeId newEdgeId = polygon->edges.at(index + 1);
        require(newEdgeId.value != 0 &&
                std::find(original.edges.begin(), original.edges.end(), newEdgeId) == original.edges.end(),
            "Split reused an existing edge ID");
        auto expectedEdges = original.edges;
        expectedEdges.insert(expectedEdges.begin() + static_cast<std::ptrdiff_t>(index + 1), newEdgeId);
        require(polygon->edges == expectedEdges, "Split changed unrelated edge IDs or their order");
        const Edge* first = document.findEdge(originalEdgeId);
        const Edge* second = document.findEdge(newEdgeId);
        require(first && second && first->start == originalEdge.start && first->end == *result &&
                second->start == *result && second->end == originalEdge.end,
            "Split halves have incorrect endpoints");

        for (const VertexId vertexId : original.vertices)
        {
            const Vertex* oldVertex = before.findVertex(vertexId);
            const Vertex* vertex = document.findVertex(vertexId);
            require(vertex && vertex->id == oldVertex->id &&
                    vertex->position.x == oldVertex->position.x && vertex->position.y == oldVertex->position.y,
                "Split changed an original vertex");
        }
        for (const EdgeId edgeId : original.edges)
        {
            if (edgeId == originalEdgeId)
                continue;
            const Edge* oldEdge = before.findEdge(edgeId);
            const Edge* edge = document.findEdge(edgeId);
            require(edge && edge->id == oldEdge->id && edge->start == oldEdge->start && edge->end == oldEdge->end,
                "Split changed an unrelated edge");
        }
        requireSameState(document, before, id);
        require(document.validate().empty(), "Split left invalid topology");
        return *result;
    }

    void splitsFirstMiddleAndClosingEdges()
    {
        const std::array midpoints{Vec2{5, 0}, Vec2{5, 5}, Vec2{0, 5}};
        for (std::size_t index = 0; index < midpoints.size(); ++index)
        {
            Document document;
            const PolygonId id = document.addPolygon(triangle());
            const std::array other{Vec2{100, 100}, Vec2{120, 100}, Vec2{100, 120}};
            const PolygonId otherId = document.addPolygon(other);
            const VertexId added = checkSplit(document, id, index, midpoints[index]);
            const EdgeId addedEdge = document.findPolygon(id)->edges.at(index + 1);
            const PolygonId followingId = document.addPolygon(triangle());
            const Polygon* following = document.findPolygon(followingId);
            require(followingId.value == otherId.value + 1 &&
                    following->vertices.front().value == added.value + 1 &&
                    following->edges.front().value == addedEdge.value + 1,
                "Split consumed an incorrect number of IDs");
        }
    }

    void repeatedlySplitsEitherHalf()
    {
        Document document;
        const PolygonId id = document.addPolygon(triangle());
        const VertexId first = checkSplit(document, id, 0, Vec2{5, 0});
        const VertexId second = checkSplit(document, id, 0, Vec2{2.5, 0});
        const VertexId third = checkSplit(document, id, 2, Vec2{7.5, 0});
        require(first != second && first != third && second != third, "Repeated splits reused IDs");
    }

    void rejectsUnknownEdgeWithoutMutationOrConsumedIds()
    {
        Document empty;
        require(!empty.splitEdge(EdgeId{1}), "Split in empty document must fail");
        requireCountersPreserved(empty, Document{});

        Document document;
        document.addPolygon(triangle());
        const Document before = document;
        for (const EdgeId id : {EdgeId{}, EdgeId{4}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!document.splitEdge(id), "Unknown edge split was accepted");
            require(document.findEdge(id) == nullptr, "Rejected split inserted an unknown edge");
            requireSameState(document, before);
        }
        requireCountersPreserved(document, before);
    }

    void rejectsMissingEndpoint()
    {
        for (const bool missingStart : {true, false})
        {
            Document document;
            const PolygonId id = document.addPolygon(triangle());
            const Document before = document;
            const EdgeId edgeId = document.findPolygon(id)->edges.front();
            // Deliberate test-only corruption of an originally non-const document.
            auto* edge = const_cast<Edge*>(document.findEdge(edgeId));
            const Edge original = *edge;
            const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
            if (missingStart) edge->start = missing;
            else edge->end = missing;
            require(!document.splitEdge(edgeId), "Split with a missing endpoint was accepted");
            require((missingStart ? edge->start : edge->end) == missing, "Rejected split changed the broken reference");
            *edge = original;
            requireSameState(document, before);
            requireCountersPreserved(document, before);
        }
    }

    void rejectsEdgeWithoutOwningPolygon()
    {
        Document document;
        const PolygonId id = document.addPolygon(triangle());
        const Document before = document;
        auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
        const auto originalEdges = polygon->edges;
        const EdgeId edgeId = polygon->edges.back();
        polygon->edges.pop_back();
        require(!document.splitEdge(edgeId), "Edge without an owning polygon was split");
        require(polygon->edges.size() == originalEdges.size() - 1, "Rejected split changed the edge list");
        polygon->edges = originalEdges;
        requireSameState(document, before);
        requireCountersPreserved(document, before);
    }

    void rejectsIncorrectEdgeConnection()
    {
        Document document;
        const PolygonId id = document.addPolygon(triangle());
        const Document before = document;
        const EdgeId edgeId = document.findPolygon(id)->edges.front();
        auto* edge = const_cast<Edge*>(document.findEdge(edgeId));
        const Edge original = *edge;
        std::swap(edge->start, edge->end);
        require(!document.splitEdge(edgeId), "Split accepted endpoints inconsistent with the vertex cycle");
        require(edge->start == original.end && edge->end == original.start, "Rejected split changed the edge");
        *edge = original;
        requireSameState(document, before);
        requireCountersPreserved(document, before);
    }

    void midpointDoesNotOverflowForFiniteCoordinates()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool oppositeSigns : {false, true})
        {
            Document document;
            const Vec2 start{maximum, -maximum};
            const Vec2 end = oppositeSigns ? Vec2{-maximum, maximum} : start;
            const std::array positions{start, end, Vec2{0, 1}};
            const PolygonId id = document.addPolygon(positions);
            checkSplit(document, id, 0, oppositeSigns ? Vec2{} : start);
        }
    }

    void splitsZeroLengthEdge()
    {
        Document document;
        const std::array positions{Vec2{2, 3}, Vec2{2, 3}, Vec2{10, 10}};
        const PolygonId id = document.addPolygon(positions);
        checkSplit(document, id, 0, Vec2{2, 3});
    }

    void rejectsNonFiniteEndpointCoordinates()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            for (std::size_t endpoint = 0; endpoint < 2; ++endpoint)
            {
                for (const bool changeX : {true, false})
                {
                    Document document;
                    const PolygonId id = document.addPolygon(triangle());
                    const Document before = document;
                    const Polygon* polygon = document.findPolygon(id);
                    auto* vertex = const_cast<Vertex*>(document.findVertex(polygon->vertices.at(endpoint)));
                    const Vertex original = *vertex;
                    if (changeX) vertex->position.x = invalid;
                    else vertex->position.y = invalid;
                    require(!document.splitEdge(polygon->edges.front()), "Non-finite endpoint was accepted");
                    const double preserved = changeX ? vertex->position.x : vertex->position.y;
                    require(std::isnan(invalid) ? std::isnan(preserved) : preserved == invalid,
                        "Rejected split changed the non-finite coordinate");
                    *vertex = original;
                    requireSameState(document, before);
                    requireCountersPreserved(document, before);
                }
            }
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> splitEdgeTests()
    {
        static const std::array tests{
            TestCase{"first, middle and closing edges preserve topology and other polygons", splitsFirstMiddleAndClosingEdges},
            TestCase{"repeated splits of either half", repeatedlySplitsEitherHalf},
            TestCase{"unknown edge rejects without mutation or consumed IDs", rejectsUnknownEdgeWithoutMutationOrConsumedIds},
            TestCase{"missing start or end rejects without mutation", rejectsMissingEndpoint},
            TestCase{"edge without owning polygon rejects without mutation", rejectsEdgeWithoutOwningPolygon},
            TestCase{"incorrect edge connection rejects without mutation", rejectsIncorrectEdgeConnection},
            TestCase{"finite extreme coordinates produce finite midpoint", midpointDoesNotOverflowForFiniteCoordinates},
            TestCase{"zero-length edge splits at its only position", splitsZeroLengthEdge},
            TestCase{"non-finite endpoint coordinates reject without mutation", rejectsNonFiniteEndpointCoordinates}
        };
        return tests;
    }
}
