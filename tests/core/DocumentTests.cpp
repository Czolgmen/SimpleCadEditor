#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <array>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
    using namespace cad;
    using cad::tests::require;

    // Lookup through a const document must not allow modifying its entities.
    static_assert(std::is_same_v<
        decltype(std::declval<const Document&>().findVertex(VertexId{})), const Vertex*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Document&>().findEdge(EdgeId{})), const Edge*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Document&>().findPolygon(PolygonId{})), const Polygon*>);

    std::array<Vec2, 3> triangle()
    {
        return {Vec2{-2.5, -3.25}, Vec2{10.75, 0.5}, Vec2{1.0, 8.125}};
    }

    const Polygon& checkPolygon(
        const Document& document, PolygonId id, std::span<const Vec2> expectedPositions)
    {
        const Polygon* polygon = document.findPolygon(id);
        require(polygon != nullptr, "Created polygon cannot be found");
        require(id.value != 0 && polygon->id == id, "Polygon has an invalid or mismatched ID");
        require(polygon->vertices.size() == expectedPositions.size(), "Wrong vertex count");
        require(polygon->edges.size() == expectedPositions.size(), "Wrong edge count");

        const std::set<VertexId> vertexIds(polygon->vertices.begin(), polygon->vertices.end());
        const std::set<EdgeId> edgeIds(polygon->edges.begin(), polygon->edges.end());
        require(vertexIds.size() == polygon->vertices.size(), "Vertex IDs are not unique");
        require(edgeIds.size() == polygon->edges.size(), "Edge IDs are not unique");

        for (std::size_t i = 0; i < expectedPositions.size(); ++i)
        {
            const VertexId vertexId = polygon->vertices[i];
            const Vertex* vertex = document.findVertex(vertexId);
            require(vertex != nullptr, "Polygon references a missing vertex");
            require(vertexId.value != 0 && vertex->id == vertexId, "Vertex ID mismatch");
            require(vertex->position.x == expectedPositions[i].x &&
                    vertex->position.y == expectedPositions[i].y,
                "Vertex position or vertex order changed");

            const EdgeId edgeId = polygon->edges[i];
            const Edge* edge = document.findEdge(edgeId);
            require(edge != nullptr, "Polygon references a missing edge");
            require(edgeId.value != 0 && edge->id == edgeId, "Edge ID mismatch");
            require(edge->start == vertexId, "Edge starts at the wrong vertex");

            const VertexId expectedEnd = i + 1 == expectedPositions.size()
                ? polygon->vertices.front()
                : polygon->vertices[i + 1];
            require(edge->end == expectedEnd, "Edge does not connect to the next vertex");
        }

        return *polygon;
    }

    void emptyDocumentReturnsNull()
    {
        const Document document;
        for (const std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1},
                 std::numeric_limits<std::uint64_t>::max()})
        {
            require(document.findVertex(VertexId{value}) == nullptr, "Unknown vertex found");
            require(document.findEdge(EdgeId{value}) == nullptr, "Unknown edge found");
            require(document.findPolygon(PolygonId{value}) == nullptr, "Unknown polygon found");
        }
    }

    void createsTriangle()
    {
        Document document;
        const auto positions = triangle();
        checkPolygon(document, document.addPolygon(positions), positions);
    }

    void createsQuadrilateral()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 10}, Vec2{0, 10}};
        checkPolygon(document, document.addPolygon(positions), positions);
    }

    void acceptsConcavePolygon()
    {
        Document document;
        const std::array positions{
            Vec2{0, 0}, Vec2{10, 0}, Vec2{4, 4}, Vec2{10, 10}, Vec2{0, 10}};
        checkPolygon(document, document.addPolygon(positions), positions);
    }

    void acceptsSelfIntersection()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{10, 10}, Vec2{0, 10}, Vec2{10, 0}};
        checkPolygon(document, document.addPolygon(positions), positions);
    }

    void keepsPolygonsIndependent()
    {
        Document document;
        const auto firstPositions = triangle();
        const PolygonId firstId = document.addPolygon(firstPositions);
        const Polygon first = checkPolygon(document, firstId, firstPositions);

        const std::array secondPositions{
            Vec2{100, 100}, Vec2{120, 100}, Vec2{120, 115}, Vec2{100, 115}};
        const PolygonId secondId = document.addPolygon(secondPositions);
        const Polygon& second = checkPolygon(document, secondId, secondPositions);
        require(firstId != secondId, "Polygon ID was reused");

        const std::set<VertexId> firstVertices(first.vertices.begin(), first.vertices.end());
        const std::set<EdgeId> firstEdges(first.edges.begin(), first.edges.end());
        for (const VertexId id : second.vertices)
            require(!firstVertices.contains(id), "Polygons share a vertex ID");
        for (const EdgeId id : second.edges)
            require(!firstEdges.contains(id), "Polygons share an edge ID");

        const Polygon& preserved = checkPolygon(document, firstId, firstPositions);
        require(preserved.vertices == first.vertices && preserved.edges == first.edges,
            "Adding another polygon changed the first polygon's IDs");
    }

    void copiesInputPositions()
    {
        Document document;
        auto positions = triangle();
        const auto original = positions;
        const PolygonId id = document.addPolygon(positions);

        positions[0] = Vec2{999, -999};
        positions[1] = Vec2{-123, 456};
        checkPolygon(document, id, original);
    }

    void populatedDocumentReturnsNullForUnknownIds()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId polygonId = document.addPolygon(positions);
        const Polygon& polygon = checkPolygon(document, polygonId, positions);

        for (const VertexId id : {VertexId{}, VertexId{polygon.vertices.back().value + 1},
                 VertexId{std::numeric_limits<std::uint64_t>::max()}})
            require(document.findVertex(id) == nullptr, "Unknown vertex found");

        for (const EdgeId id : {EdgeId{}, EdgeId{polygon.edges.back().value + 1},
                 EdgeId{std::numeric_limits<std::uint64_t>::max()}})
            require(document.findEdge(id) == nullptr, "Unknown edge found");

        for (const PolygonId id : {PolygonId{}, PolygonId{polygonId.value + 1},
                 PolygonId{std::numeric_limits<std::uint64_t>::max()}})
            require(document.findPolygon(id) == nullptr, "Unknown polygon found");

        checkPolygon(document, polygonId, positions);
    }

    void checkRejectedInput(std::span<const Vec2> invalidPositions)
    {
        Document document;
        const auto positions = triangle();
        const PolygonId firstId = document.addPolygon(positions);
        const Polygon first = checkPolygon(document, firstId, positions);

        bool rejected = false;
        try
        {
            document.addPolygon(invalidPositions);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "Invalid polygon was accepted");

        const Polygon& preserved = checkPolygon(document, firstId, positions);
        require(preserved.vertices == first.vertices && preserved.edges == first.edges,
            "Rejected input changed existing IDs");

        const VertexId nextVertex{first.vertices.back().value + 1};
        const EdgeId nextEdge{first.edges.back().value + 1};
        const PolygonId nextPolygon{firstId.value + 1};
        require(document.findPolygon(nextPolygon) == nullptr, "Rejected polygon leaked into document");
        for (std::size_t i = 0; i < invalidPositions.size(); ++i)
        {
            const auto offset = static_cast<std::uint64_t>(i);
            require(document.findVertex(VertexId{nextVertex.value + offset}) == nullptr,
                "Rejected input left orphan vertices");
            require(document.findEdge(EdgeId{nextEdge.value + offset}) == nullptr,
                "Rejected input left orphan edges");
        }

        const PolygonId followingId = document.addPolygon(positions);
        const Polygon& following = checkPolygon(document, followingId, positions);
        require(followingId == nextPolygon, "Rejected input consumed a polygon ID");
        require(following.vertices.front() == nextVertex, "Rejected input consumed vertex IDs");
        require(following.edges.front() == nextEdge, "Rejected input consumed edge IDs");
        checkPolygon(document, firstId, positions);
    }

    void rejectsEmptyPolygon()
    {
        checkRejectedInput(std::span<const Vec2>{});
    }

    void rejectsSingleVertex()
    {
        const std::array positions{Vec2{0, 0}};
        checkRejectedInput(positions);
    }

    void rejectsTwoVertices()
    {
        const std::array positions{Vec2{0, 0}, Vec2{10, 10}};
        checkRejectedInput(positions);
    }

    void checkNonFiniteCoordinates(double value)
    {
        // Include invalid coordinates at the end to catch partial registration.
        for (std::size_t i = 0; i < 3; ++i)
        {
            auto positions = triangle();
            positions[i].x = value;
            checkRejectedInput(positions);

            positions = triangle();
            positions[i].y = value;
            checkRejectedInput(positions);
        }
    }

    void rejectsNaN()
    {
        checkNonFiniteCoordinates(std::numeric_limits<double>::quiet_NaN());
    }

    void rejectsPositiveInfinity()
    {
        checkNonFiniteCoordinates(std::numeric_limits<double>::infinity());
    }

    void rejectsNegativeInfinity()
    {
        checkNonFiniteCoordinates(-std::numeric_limits<double>::infinity());
    }
}

namespace cad::tests
{
    std::span<const TestCase> documentTests()
    {
        static const std::array tests{
            TestCase{"empty document lookup", emptyDocumentReturnsNull},
            TestCase{"triangle vertices, positions and closed edges", createsTriangle},
            TestCase{"quadrilateral vertices and closed edges", createsQuadrilateral},
            TestCase{"concave polygon", acceptsConcavePolygon},
            TestCase{"self-intersecting polygon", acceptsSelfIntersection},
            TestCase{"multiple independent polygons and stable IDs", keepsPolygonsIndependent},
            TestCase{"document owns a copy of input positions", copiesInputPositions},
            TestCase{"unknown IDs in populated document", populatedDocumentReturnsNullForUnknownIds},
            TestCase{"reject empty input without mutation", rejectsEmptyPolygon},
            TestCase{"reject one vertex without mutation", rejectsSingleVertex},
            TestCase{"reject two vertices without mutation", rejectsTwoVertices},
            TestCase{"reject NaN in either coordinate without mutation", rejectsNaN},
            TestCase{"reject positive infinity without mutation", rejectsPositiveInfinity},
            TestCase{"reject negative infinity without mutation", rejectsNegativeInfinity}
        };
        return tests;
    }
}
