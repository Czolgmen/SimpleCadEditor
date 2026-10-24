#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace
{
    using namespace cad;
    using cad::tests::require;

    std::array<Vec2, 4> square()
    {
        return {Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}};
    }

    void requireSameState(const Document& actual, const Document& expected, PolygonId skip = {})
    {
        require(actual.getPolygons().size() == expected.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : expected.getPolygons())
        {
            const Polygon* found = actual.findPolygon(id);
            require(found && found->id == id, "Polygon disappeared or changed ID");
            if (id == skip) continue;
            require(found->vertices == polygon.vertices && found->edges == polygon.edges,
                "Polygon topology changed unexpectedly");
            for (const VertexId vertexId : polygon.vertices)
            {
                const Vertex* before = expected.findVertex(vertexId);
                const Vertex* after = actual.findVertex(vertexId);
                require(before && after && before->id == after->id &&
                        before->position.x == after->position.x && before->position.y == after->position.y,
                    "Vertex disappeared or changed unexpectedly");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const Edge* before = expected.findEdge(edgeId);
                const Edge* after = actual.findEdge(edgeId);
                require(before && after && before->id == after->id &&
                        before->start == after->start && before->end == after->end,
                    "Edge disappeared or changed unexpectedly");
            }
        }
    }

    void requireCountersPreserved(Document& actual, Document expected)
    {
        actual.addPolygon(square());
        expected.addPolygon(square());
        requireSameState(actual, expected);
    }

    void checkDeletion(Document& document, PolygonId id, std::size_t index)
    {
        const Document before = document;
        const Polygon original = *before.findPolygon(id);
        const VertexId removed = original.vertices.at(index);
        const std::size_t previousIndex = index == 0 ? original.vertices.size() - 1 : index - 1;
        const VertexId previousId = original.vertices[previousIndex];
        const VertexId nextId = original.vertices[(index + 1) % original.vertices.size()];
        require(document.deleteVertex(removed), "Valid vertex deletion failed");
        require(document.findVertex(removed) == nullptr, "Deleted vertex is still stored");
        require(document.findEdge(original.edges[previousIndex]) == nullptr &&
                document.findEdge(original.edges[index]) == nullptr,
            "Incident edges must both be removed from storage");

        const Polygon* polygon = document.findPolygon(id);
        auto expectedVertices = original.vertices;
        expectedVertices.erase(expectedVertices.begin() + static_cast<std::ptrdiff_t>(index));
        require(polygon && polygon->vertices == expectedVertices &&
                polygon->edges.size() == expectedVertices.size(),
            "Deletion changed vertex order or left the wrong edge count");
        for (std::size_t i = 0; i < polygon->vertices.size(); ++i)
        {
            const VertexId vertexId = polygon->vertices[i];
            const Vertex* vertex = document.findVertex(vertexId);
            const Vertex* oldVertex = before.findVertex(vertexId);
            require(vertex && vertex->id == vertexId &&
                    vertex->position.x == oldVertex->position.x && vertex->position.y == oldVertex->position.y,
                "Deletion changed a surviving vertex");
            const Edge* edge = document.findEdge(polygon->edges[i]);
            require(edge && edge->id == polygon->edges[i] && edge->start == vertexId &&
                    edge->end == polygon->vertices[(i + 1) % polygon->vertices.size()],
                "Deletion left incorrect edge connections");
            if (vertexId == previousId)
            {
                require(edge->end == nextId &&
                        std::find(original.edges.begin(), original.edges.end(), edge->id) == original.edges.end(),
                    "Neighbours must be joined by a fresh edge ID");
            }
            else
            {
                const auto position = std::find(original.vertices.begin(), original.vertices.end(), vertexId);
                const auto oldIndex = static_cast<std::size_t>(position - original.vertices.begin());
                require(edge->id == original.edges[oldIndex], "Deletion replaced an unrelated edge");
            }
        }
        requireSameState(document, before, id);
        require(document.validate().empty(), "Deletion left invalid topology");
    }

    void deletesEverySquareVertexAndPreservesOtherPolygons()
    {
        for (std::size_t index = 0; index < 4; ++index)
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const PolygonId otherId = document.addPolygon(square());
            const Polygon other = *document.findPolygon(otherId);
            checkDeletion(document, id, index);
            const PolygonId nextId = document.addPolygon(square());
            const Polygon* next = document.findPolygon(nextId);
            require(nextId.value == otherId.value + 1 &&
                    next->vertices.front().value == other.vertices.back().value + 1 &&
                    next->edges.front().value == other.edges.back().value + 2,
                "Deletion must consume one edge ID and no vertex or polygon IDs");
        }
    }

    void repeatedDeletionStopsAtTriangle()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{10, 0}, Vec2{15, 5}, Vec2{10, 10}, Vec2{0, 10}};
        const PolygonId id = document.addPolygon(positions);
        checkDeletion(document, id, 2);
        checkDeletion(document, id, 0);
        const Document before = document;
        require(!document.deleteVertex(document.findPolygon(id)->vertices.front()), "Triangle lost a vertex");
        requireSameState(document, before);
    }

    void rejectsUnknownAndAlreadyDeletedIds()
    {
        Document empty;
        require(!empty.deleteVertex(VertexId{1}), "Empty document accepted deletion");
        requireCountersPreserved(empty, Document{});
        Document document;
        const PolygonId id = document.addPolygon(square());
        const VertexId removed = document.findPolygon(id)->vertices[1];
        checkDeletion(document, id, 1);
        const Document before = document;
        for (const VertexId missing : {VertexId{}, removed, VertexId{5},
                 VertexId{std::numeric_limits<std::uint64_t>::max()}})
            require(!document.deleteVertex(missing), "Missing vertex deletion was accepted");
        requireSameState(document, before);
        requireCountersPreserved(document, before);
    }

    void rejectsEveryTriangleVertex()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
        const PolygonId id = document.addPolygon(positions);
        const Document before = document;
        const auto ids = document.findPolygon(id)->vertices;
        for (const VertexId vertexId : ids)
            require(!document.deleteVertex(vertexId), "A triangle vertex was deleted");
        requireSameState(document, before);
        requireCountersPreserved(document, before);
    }

    void deletingSplitVertexRestoresOriginalGeometry()
    {
        Document document;
        const PolygonId id = document.addPolygon(square());
        const auto originalVertices = document.findPolygon(id)->vertices;
        const auto midpoint = document.splitEdge(document.findPolygon(id)->edges.back());
        require(midpoint.has_value(), "Fixture split failed");
        checkDeletion(document, id, 4);
        require(document.findPolygon(id)->vertices == originalVertices && !document.findVertex(*midpoint),
            "Deleting the inserted midpoint did not restore the original vertex cycle");
    }

    void rejectsMissingNeighbours()
    {
        for (const std::size_t neighbour : {std::size_t{0}, std::size_t{2}})
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const Document before = document;
            auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
            const auto originalVertices = polygon->vertices;
            const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
            polygon->vertices[neighbour] = missing;
            require(!document.deleteVertex(originalVertices[1]), "Deletion with a missing neighbour was accepted");
            require(polygon->vertices[neighbour] == missing, "Rejected deletion changed a broken reference");
            polygon->vertices = originalVertices;
            requireSameState(document, before);
            requireCountersPreserved(document, before);
        }
    }

    void rejectsMissingIncidentEdges()
    {
        for (const std::size_t edgeIndex : {std::size_t{0}, std::size_t{1}})
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const Document before = document;
            auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
            const auto originalEdges = polygon->edges;
            const EdgeId missing{std::numeric_limits<std::uint64_t>::max()};
            polygon->edges[edgeIndex] = missing;
            require(!document.deleteVertex(polygon->vertices[1]), "Deletion with a missing incident edge was accepted");
            require(polygon->edges[edgeIndex] == missing, "Rejected deletion changed a broken edge reference");
            polygon->edges = originalEdges;
            requireSameState(document, before);
            requireCountersPreserved(document, before);
        }
    }

    void rejectsIncorrectIncidentConnections()
    {
        for (const std::size_t edgeIndex : {std::size_t{0}, std::size_t{1}})
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const Document before = document;
            const Polygon* polygon = document.findPolygon(id);
            auto* edge = const_cast<Edge*>(document.findEdge(polygon->edges[edgeIndex]));
            const Edge original = *edge;
            if (edgeIndex == 0) edge->end = polygon->vertices[3];
            else edge->start = polygon->vertices[3];
            require(!document.deleteVertex(polygon->vertices[1]), "Incorrect incident connection was accepted");
            require(edge->start != original.start || edge->end != original.end, "Rejected deletion repaired an edge");
            *edge = original;
            requireSameState(document, before);
            requireCountersPreserved(document, before);
        }
    }

    void rejectsBrokenOrAmbiguousOwnership()
    {
        for (int mode = 0; mode < 4; ++mode)
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const PolygonId otherId = document.addPolygon(square());
            const Document before = document;
            auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
            auto* other = const_cast<Polygon*>(document.findPolygon(otherId));
            const Polygon original = *polygon;
            const Polygon originalOther = *other;
            const VertexId target = polygon->vertices[1];
            if (mode == 0) polygon->edges.pop_back();
            if (mode == 1) polygon->vertices.erase(polygon->vertices.begin() + 1);
            if (mode == 2) polygon->vertices[3] = target;
            if (mode == 3) other->vertices[0] = target;
            require(!document.deleteVertex(target), "Broken or ambiguous vertex ownership was accepted");
            *polygon = original;
            *other = originalOther;
            requireSameState(document, before);
            requireCountersPreserved(document, before);
        }
    }

    void rejectsNonFiniteLocalPositions()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            for (std::size_t index = 0; index < 3; ++index)
            {
                for (const bool changeX : {true, false})
                {
                    Document document;
                    const PolygonId id = document.addPolygon(square());
                    const Document before = document;
                    const Polygon* polygon = document.findPolygon(id);
                    auto* vertex = const_cast<Vertex*>(document.findVertex(polygon->vertices[index]));
                    const Vertex original = *vertex;
                    if (changeX) vertex->position.x = invalid;
                    else vertex->position.y = invalid;
                    require(!document.deleteVertex(polygon->vertices[1]), "Non-finite local position was accepted");
                    const double preserved = changeX ? vertex->position.x : vertex->position.y;
                    require(std::isnan(invalid) ? std::isnan(preserved) : preserved == invalid,
                        "Rejected deletion changed a non-finite coordinate");
                    *vertex = original;
                    requireSameState(document, before);
                    requireCountersPreserved(document, before);
                }
            }
        }
    }

    void allowsCoincidentPositions()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}, Vec2{10, 10}};
        const PolygonId id = document.addPolygon(positions);
        checkDeletion(document, id, 1);
    }
}

namespace cad::tests
{
    std::span<const TestCase> deleteVertexTests()
    {
        static const std::array tests{
            TestCase{"first, middle and last vertices preserve topology, other polygons and IDs", deletesEverySquareVertexAndPreservesOtherPolygons},
            TestCase{"repeated deletion stops at a triangle", repeatedDeletionStopsAtTriangle},
            TestCase{"unknown and already deleted IDs reject without mutation", rejectsUnknownAndAlreadyDeletedIds},
            TestCase{"every triangle vertex is protected", rejectsEveryTriangleVertex},
            TestCase{"deleting a split vertex restores the original vertex cycle", deletingSplitVertexRestoresOriginalGeometry},
            TestCase{"missing neighbours reject without mutation", rejectsMissingNeighbours},
            TestCase{"missing incident edges reject without mutation", rejectsMissingIncidentEdges},
            TestCase{"incorrect incident connections reject without mutation", rejectsIncorrectIncidentConnections},
            TestCase{"broken or ambiguous ownership rejects without mutation", rejectsBrokenOrAmbiguousOwnership},
            TestCase{"non-finite local positions reject without mutation", rejectsNonFiniteLocalPositions},
            TestCase{"coincident positions are allowed", allowsCoincidentPositions}
        };
        return tests;
    }
}
