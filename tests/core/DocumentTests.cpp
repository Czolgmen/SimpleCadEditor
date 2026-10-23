#include <cad_core/model/Document.h>
#include <cad_core/demo/DemoScene.hpp>

#include "../TestSupport.h"

#include <array>
#include <limits>
#include <map>
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
    static_assert(std::is_same_v<
        decltype(std::declval<const Document&>().getPolygons()),
        const std::map<PolygonId, Polygon>&>);

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

    void movesOnlyRequestedVertexAndPreservesTopology()
    {
        Document document;
        auto expected = triangle();
        const PolygonId firstId = document.addPolygon(expected);
        const Polygon first = checkPolygon(document, firstId, expected);
        const std::array otherPositions{
            Vec2{100, 100}, Vec2{120, 100}, Vec2{100, 120}};
        const PolygonId otherId = document.addPolygon(otherPositions);
        const Polygon other = checkPolygon(document, otherId, otherPositions);

        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            expected[i] = Vec2{-40.5 + static_cast<double>(i), 25.25};
            require(document.moveVertex(first.vertices[i], expected[i]),
                "Moving an existing vertex must succeed");

            const Polygon& moved = checkPolygon(document, firstId, expected);
            require(moved.vertices == first.vertices && moved.edges == first.edges,
                "Moving a vertex changed topology or IDs");
            const Polygon& preserved = checkPolygon(document, otherId, otherPositions);
            require(preserved.vertices == other.vertices && preserved.edges == other.edges,
                "Moving a vertex changed another polygon's IDs");
            require(document.getPolygons().size() == 2, "Moving changed the polygon count");
            require(document.validate().empty(), "Moving a vertex made the document invalid");
        }
    }

    void repeatedMovesCopyPositionAndAcceptSamePosition()
    {
        Document document;
        auto expected = triangle();
        const PolygonId polygonId = document.addPolygon(expected);
        const VertexId vertexId = checkPolygon(document, polygonId, expected).vertices[1];
        Vec2 input{-15.5, 20.25};
        expected[1] = input;
        require(document.moveVertex(vertexId, input), "First move failed");
        input = Vec2{999, 999};
        checkPolygon(document, polygonId, expected);

        require(document.moveVertex(vertexId, expected[1]), "An unchanged position must be accepted");
        expected[1] = Vec2{0, 0};
        require(document.moveVertex(vertexId, expected[1]), "A repeated move failed");
        checkPolygon(document, polygonId, expected);
    }

    void rejectsMoveInEmptyDocument()
    {
        Document document;
        require(!document.moveVertex(VertexId{1}, Vec2{10, 20}),
            "Moving a missing vertex in an empty document must fail");
        require(document.findVertex(VertexId{1}) == nullptr, "Move inserted a missing vertex");
        require(document.getPolygons().empty() && document.validate().empty(),
            "Rejected move changed an empty document");
    }

    void rejectsUnknownMoveWithoutMutation()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId polygonId = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, polygonId, positions);
        for (const VertexId id : {VertexId{}, VertexId{original.vertices.back().value + 1},
                 VertexId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!document.moveVertex(id, Vec2{50, -20}), "Unknown vertex move was accepted");
            require(document.findVertex(id) == nullptr, "Rejected move inserted a vertex");
            const Polygon& preserved = checkPolygon(document, polygonId, positions);
            require(preserved.vertices == original.vertices && preserved.edges == original.edges,
                "Rejected move changed IDs");
            require(document.getPolygons().size() == 1 && document.validate().empty(),
                "Rejected move changed the document");
        }
    }

    void checkRejectedMoveCoordinate(double invalidValue)
    {
        Document document;
        const auto positions = triangle();
        const PolygonId polygonId = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, polygonId, positions);
        for (const VertexId id : original.vertices)
        {
            for (const Vec2 invalid : {Vec2{invalidValue, 42}, Vec2{42, invalidValue}})
            {
                require(!document.moveVertex(id, invalid), "Non-finite move was accepted");
                const Polygon& preserved = checkPolygon(document, polygonId, positions);
                require(preserved.vertices == original.vertices && preserved.edges == original.edges,
                    "Non-finite move changed IDs");
                require(document.getPolygons().size() == 1 && document.validate().empty(),
                    "Non-finite move changed the document");
            }
        }
    }

    void rejectsNaNMove()
    {
        checkRejectedMoveCoordinate(std::numeric_limits<double>::quiet_NaN());
    }

    void rejectsPositiveInfinityMove()
    {
        checkRejectedMoveCoordinate(std::numeric_limits<double>::infinity());
    }

    void rejectsNegativeInfinityMove()
    {
        checkRejectedMoveCoordinate(-std::numeric_limits<double>::infinity());
    }

    void movesWholePolygonAndPreservesOtherPolygons()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId id = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, id, positions);
        const std::array otherPositions{
            Vec2{100, 100}, Vec2{120, 100}, Vec2{100, 120}};
        const PolygonId otherId = document.addPolygon(otherPositions);
        const Polygon other = checkPolygon(document, otherId, otherPositions);

        require(document.movePolygon(id, Vec2{4.5, -2.25}), "Polygon move failed");
        const std::array expected{Vec2{2, -5.5}, Vec2{15.25, -1.75}, Vec2{5.5, 5.875}};
        const Polygon& moved = checkPolygon(document, id, expected);
        require(moved.vertices == original.vertices && moved.edges == original.edges,
            "Polygon move changed IDs or topology");
        const Polygon& preserved = checkPolygon(document, otherId, otherPositions);
        require(preserved.vertices == other.vertices && preserved.edges == other.edges,
            "Polygon move changed another polygon's IDs");
        require(document.getPolygons().size() == 2 && document.validate().empty(),
            "Polygon move changed the polygon count or invalidated the document");

        const PolygonId nextId = document.addPolygon(positions);
        const Polygon& next = checkPolygon(document, nextId, positions);
        require(nextId.value == otherId.value + 1 &&
                next.vertices.front().value == other.vertices.back().value + 1 &&
                next.edges.front().value == other.edges.back().value + 1,
            "Polygon move consumed entity IDs");
    }

    void polygonMovesAccumulateAndAcceptZero()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId id = document.addPolygon(positions);
        require(document.movePolygon(id, Vec2{}), "Zero displacement must be accepted");
        checkPolygon(document, id, positions);

        require(document.movePolygon(id, Vec2{4, -8}), "First polygon move failed");
        const std::array first{Vec2{1.5, -11.25}, Vec2{14.75, -7.5}, Vec2{5, 0.125}};
        checkPolygon(document, id, first);
        require(document.movePolygon(id, Vec2{-1.5, 2}), "Second polygon move failed");
        const std::array second{Vec2{0, -9.25}, Vec2{13.25, -5.5}, Vec2{3.5, 2.125}};
        checkPolygon(document, id, second);
        require(document.movePolygon(id, Vec2{-2.5, 6}), "Inverse polygon move failed");
        checkPolygon(document, id, positions);
    }

    void rejectsPolygonMoveInEmptyDocument()
    {
        Document document;
        require(!document.movePolygon(PolygonId{1}, Vec2{10, 20}),
            "Missing polygon move must fail in an empty document");
        require(document.getPolygons().empty() && document.findPolygon(PolygonId{1}) == nullptr,
            "Rejected polygon move inserted a polygon");
    }

    void rejectsUnknownPolygonMoveWithoutMutation()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId id = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, id, positions);
        for (const PolygonId missing : {PolygonId{}, PolygonId{id.value + 1},
                 PolygonId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!document.movePolygon(missing, Vec2{5, -10}), "Unknown polygon move was accepted");
            require(document.findPolygon(missing) == nullptr, "Rejected move inserted a polygon");
            const Polygon& preserved = checkPolygon(document, id, positions);
            require(preserved.vertices == original.vertices && preserved.edges == original.edges,
                "Rejected polygon move changed IDs");
            require(document.getPolygons().size() == 1 && document.validate().empty(),
                "Rejected polygon move changed the document");
        }
    }

    void checkRejectedPolygonDelta(double invalidValue)
    {
        Document document;
        const auto positions = triangle();
        const PolygonId id = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, id, positions);
        for (const Vec2 delta : {Vec2{invalidValue, 5}, Vec2{5, invalidValue}})
        {
            require(!document.movePolygon(id, delta), "Non-finite polygon displacement was accepted");
            const Polygon& preserved = checkPolygon(document, id, positions);
            require(preserved.vertices == original.vertices && preserved.edges == original.edges,
                "Rejected polygon displacement changed IDs");
            require(document.getPolygons().size() == 1 && document.validate().empty(),
                "Rejected polygon displacement changed the document");
        }
    }

    void rejectsNaNPolygonDelta()
    {
        checkRejectedPolygonDelta(std::numeric_limits<double>::quiet_NaN());
    }

    void rejectsPositiveInfinityPolygonDelta()
    {
        checkRejectedPolygonDelta(std::numeric_limits<double>::infinity());
    }

    void rejectsNegativeInfinityPolygonDelta()
    {
        checkRejectedPolygonDelta(-std::numeric_limits<double>::infinity());
    }

    void polygonOverflowDoesNotPartiallyMoveVertices()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const double large : {maximum, -maximum})
        {
            for (const bool moveX : {true, false})
            {
                Document document;
                std::array positions{Vec2{0, 0}, Vec2{1, 0}, Vec2{1, 1}, Vec2{0, 1}};
                if (moveX)
                    positions.back().x = large;
                else
                    positions.back().y = large;
                const PolygonId id = document.addPolygon(positions);
                const Polygon original = checkPolygon(document, id, positions);

                // Only the LAST vertex overflows: earlier candidates must remain unchanged.
                const Vec2 delta = moveX ? Vec2{large, 0} : Vec2{0, large};
                require(!document.movePolygon(id, delta), "Overflowing polygon move was accepted");
                const Polygon& preserved = checkPolygon(document, id, positions);
                require(preserved.vertices == original.vertices && preserved.edges == original.edges,
                    "Overflowing polygon move changed IDs");
                require(document.validate().empty(), "Rejected overflow left invalid positions");
            }
        }
    }

    void missingPolygonVertexDoesNotPartiallyMoveVertices()
    {
        Document document;
        const auto positions = triangle();
        const PolygonId id = document.addPolygon(positions);
        const Polygon original = checkPolygon(document, id, positions);
        // As in ValidationTests: the document is non-const; corruption is test-only.
        auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
        const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
        polygon->vertices.back() = missing;
        const auto corruptedIds = polygon->vertices;

        require(!document.movePolygon(id, Vec2{5, 10}), "Move with a missing vertex was accepted");
        require(polygon->vertices == corruptedIds && polygon->edges == original.edges,
            "Rejected move changed the corrupted topology");
        require(document.findVertex(missing) == nullptr, "Rejected move inserted the missing vertex");
        // Restore only the deliberately broken reference, then inspect ALL original positions.
        polygon->vertices.back() = original.vertices.back();
        checkPolygon(document, id, positions);
        require(document.validate().empty(), "Rejected move damaged the original model");
    }

    void demoSceneIsValidAndTraversable()
    {
        const Document scene = createDemoScene();
        require(scene.getPolygons().size() >= 2, "Demo scene must contain at least two polygons");
        require(scene.validate().empty(), "Demo scene must pass document validation");

        std::set<VertexId> vertexIds;
        std::set<EdgeId> edgeIds;
        for (const auto& [id, polygon] : scene.getPolygons())
        {
            require(polygon.id == id, "Polygon collection key differs from its ID");
            require(scene.findPolygon(id) == &polygon, "Polygon collection must expose stored entities");
            for (const VertexId vertexId : polygon.vertices)
            {
                require(scene.findVertex(vertexId) != nullptr, "Demo vertex cannot be found");
                require(vertexIds.insert(vertexId).second, "Demo polygons share a vertex ID");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                require(scene.findEdge(edgeId) != nullptr, "Demo edge cannot be found");
                require(edgeIds.insert(edgeId).second, "Demo polygons share an edge ID");
            }
        }
    }

    void demoScenesAreIndependent()
    {
        Document first = createDemoScene();
        const Document second = createDemoScene();
        const std::size_t firstCount = first.getPolygons().size();
        const std::size_t secondCount = second.getPolygons().size();
        const auto positions = triangle();
        const PolygonId addedId = first.addPolygon(positions);

        require(first.getPolygons().size() == firstCount + 1, "New polygon missing from collection");
        require(second.getPolygons().size() == secondCount, "Editing one demo changed another");
        checkPolygon(first, addedId, positions);
        require(first.validate().empty() && second.validate().empty(),
            "Independent demo documents must remain valid");
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
            TestCase{"reject negative infinity without mutation", rejectsNegativeInfinity},
            TestCase{"move only requested vertex and preserve topology", movesOnlyRequestedVertexAndPreservesTopology},
            TestCase{"repeated moves copy position and accept unchanged position", repeatedMovesCopyPositionAndAcceptSamePosition},
            TestCase{"reject move in empty document", rejectsMoveInEmptyDocument},
            TestCase{"reject unknown vertex move without mutation", rejectsUnknownMoveWithoutMutation},
            TestCase{"reject NaN move without mutation", rejectsNaNMove},
            TestCase{"reject positive infinity move without mutation", rejectsPositiveInfinityMove},
            TestCase{"reject negative infinity move without mutation", rejectsNegativeInfinityMove},
            TestCase{"move whole polygon and preserve other polygons and IDs", movesWholePolygonAndPreservesOtherPolygons},
            TestCase{"polygon displacements accumulate and accept zero", polygonMovesAccumulateAndAcceptZero},
            TestCase{"reject polygon move in empty document", rejectsPolygonMoveInEmptyDocument},
            TestCase{"reject unknown polygon move without mutation", rejectsUnknownPolygonMoveWithoutMutation},
            TestCase{"reject NaN polygon displacement", rejectsNaNPolygonDelta},
            TestCase{"reject positive infinity polygon displacement", rejectsPositiveInfinityPolygonDelta},
            TestCase{"reject negative infinity polygon displacement", rejectsNegativeInfinityPolygonDelta},
            TestCase{"last vertex overflow causes no partial polygon move", polygonOverflowDoesNotPartiallyMoveVertices},
            TestCase{"missing last vertex causes no partial polygon move", missingPolygonVertexDoesNotPartiallyMoveVertices},
            TestCase{"demo scene is valid and traversable", demoSceneIsValidAndTraversable},
            TestCase{"demo scenes own independent documents", demoScenesAreIndependent}
        };
        return tests;
    }
}
