#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using namespace cad;
    using cad::tests::require;

    static_assert(std::is_same_v<
        decltype(std::declval<const Document&>().validate()), std::vector<ValidationIssue>>);

    std::array<Vec2, 3> triangle()
    {
        return {Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
    }

    // Public creation operations keep the model valid. These fixtures deliberately
    // corrupt entities to exercise diagnostics. The original Document is non-const,
    // so removing const from its lookup results is legal and limited to this test file.
    struct Fixture
    {
        Document document;
        PolygonId polygonId;

        Fixture()
        {
            const auto positions = triangle();
            polygonId = document.addPolygon(positions);
        }

        Polygon& polygon()
        {
            const Polygon* found = document.findPolygon(polygonId);
            require(found != nullptr, "Fixture polygon is missing");
            return *const_cast<Polygon*>(found);
        }

        Vertex& vertex(std::size_t index)
        {
            const Vertex* found = document.findVertex(polygon().vertices.at(index));
            require(found != nullptr, "Fixture vertex is missing");
            return *const_cast<Vertex*>(found);
        }

        Edge& edge(std::size_t index)
        {
            const Edge* found = document.findEdge(polygon().edges.at(index));
            require(found != nullptr, "Fixture edge is missing");
            return *const_cast<Edge*>(found);
        }
    };

    void requireIssue(std::span<const ValidationIssue> issues, ValidationError code,
        PolygonId polygon, VertexId vertex = {}, EdgeId edge = {})
    {
        const auto found = std::find_if(issues.begin(), issues.end(),
            [&](const ValidationIssue& issue)
            {
                return issue.code == code && issue.polygon == polygon &&
                    issue.vertex == vertex && issue.edge == edge;
            });
        require(found != issues.end(), "Expected validation issue and entity IDs were not reported");
    }

    void acceptsEmptyDocument()
    {
        const Document document;
        require(document.validate().empty(), "An empty scene must be valid");
    }

    void acceptsValidScenes()
    {
        Document document;
        const auto first = triangle();
        const std::array concave{
            Vec2{20, 0}, Vec2{30, 0}, Vec2{24, 4}, Vec2{30, 10}, Vec2{20, 10}};
        const std::array crossing{Vec2{40, 0}, Vec2{50, 10}, Vec2{40, 10}, Vec2{50, 0}};
        document.addPolygon(first);
        document.addPolygon(concave);
        document.addPolygon(crossing);
        require(document.validate().empty(), "Valid concave or self-intersecting scene was rejected");
    }

    void reportsTooFewVertices()
    {
        Fixture fixture;
        fixture.polygon().vertices.resize(2);
        fixture.polygon().edges.resize(2);
        fixture.edge(1).end = fixture.polygon().vertices.front();

        const auto issues = fixture.document.validate();
        require(issues.size() == 1, "Only the minimum vertex count should fail");
        requireIssue(issues, ValidationError::TooFewVertices, fixture.polygonId);
    }

    void handlesEmptyVertexListWithEdges()
    {
        Fixture fixture;
        fixture.polygon().vertices.clear();
        const auto issues = fixture.document.validate();
        require(issues.size() == 2, "An empty cycle should report its size and edge count");
        requireIssue(issues, ValidationError::TooFewVertices, fixture.polygonId);
        requireIssue(issues, ValidationError::EdgeCountMismatch, fixture.polygonId);
    }

    void reportsTooFewEdges()
    {
        Fixture fixture;
        fixture.polygon().edges.pop_back();
        const auto issues = fixture.document.validate();
        require(issues.size() == 1, "A missing cycle slot should report a count mismatch");
        requireIssue(issues, ValidationError::EdgeCountMismatch, fixture.polygonId);
    }

    void handlesExtraEdges()
    {
        Fixture fixture;
        fixture.polygon().edges.push_back(fixture.polygon().edges.back());
        const auto issues = fixture.document.validate();
        require(issues.size() == 1, "An extra cycle slot should report a count mismatch");
        requireIssue(issues, ValidationError::EdgeCountMismatch, fixture.polygonId);
    }

    void reportsMissingListedVertex()
    {
        Fixture fixture;
        const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
        fixture.polygon().vertices[1] = missing;
        const auto issues = fixture.document.validate();
        requireIssue(issues, ValidationError::MissingVertex, fixture.polygonId, missing);
    }

    void reportsMissingListedEdge()
    {
        Fixture fixture;
        const EdgeId missing{std::numeric_limits<std::uint64_t>::max()};
        fixture.polygon().edges[1] = missing;
        const auto issues = fixture.document.validate();
        require(issues.size() == 1, "A missing edge should be reported without dereferencing it");
        requireIssue(issues, ValidationError::MissingEdge, fixture.polygonId, {}, missing);
    }

    void reportsMissingStartVertex()
    {
        Fixture fixture;
        const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
        Edge& edge = fixture.edge(0);
        edge.start = missing;
        const auto issues = fixture.document.validate();
        requireIssue(issues, ValidationError::MissingVertex, fixture.polygonId, missing, edge.id);
        requireIssue(issues, ValidationError::InvalidEdgeConnection, fixture.polygonId, {}, edge.id);
    }

    void reportsMissingEndVertex()
    {
        Fixture fixture;
        const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
        Edge& edge = fixture.edge(0);
        edge.end = missing;
        const auto issues = fixture.document.validate();
        requireIssue(issues, ValidationError::MissingVertex, fixture.polygonId, missing, edge.id);
        requireIssue(issues, ValidationError::InvalidEdgeConnection, fixture.polygonId, {}, edge.id);
    }

    void reportsIncorrectConnections()
    {
        for (int variant = 0; variant < 3; ++variant)
        {
            Fixture fixture;
            const std::size_t edgeIndex = variant == 2 ? 2 : 0;
            Edge& edge = fixture.edge(edgeIndex);
            if (variant == 0)
                edge.start = fixture.polygon().vertices[1];
            else if (variant == 1)
                edge.end = fixture.polygon().vertices[2];
            else
                edge.end = fixture.polygon().vertices[1];

            const auto issues = fixture.document.validate();
            require(issues.size() == 1, "An existing but misplaced endpoint should report a connection error");
            requireIssue(issues, ValidationError::InvalidEdgeConnection, fixture.polygonId, {}, edge.id);
        }
    }

    void reportsNonFinitePositions()
    {
        const std::array invalidValues{
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity()};

        for (const double value : invalidValues)
        {
            for (std::size_t index = 0; index < 3; ++index)
            {
                for (int axis = 0; axis < 2; ++axis)
                {
                    Fixture fixture;
                    Vertex& vertex = fixture.vertex(index);
                    if (axis == 0)
                        vertex.position.x = value;
                    else
                        vertex.position.y = value;

                    const auto issues = fixture.document.validate();
                    require(issues.size() == 1, "A non-finite position should report its vertex");
                    requireIssue(issues, ValidationError::NonFinitePosition, fixture.polygonId, vertex.id);
                }
            }
        }
    }

    void collectsErrorsAcrossPolygons()
    {
        Fixture fixture;
        const auto positions = triangle();
        const PolygonId secondId = fixture.document.addPolygon(positions);
        const Polygon* second = fixture.document.findPolygon(secondId);
        require(second != nullptr, "Second fixture polygon is missing");
        const VertexId secondVertexId = second->vertices.front();
        Vertex* secondVertex = const_cast<Vertex*>(fixture.document.findVertex(secondVertexId));
        require(secondVertex != nullptr, "Second fixture vertex is missing");
        secondVertex->position.y = std::numeric_limits<double>::quiet_NaN();

        const EdgeId missing{std::numeric_limits<std::uint64_t>::max()};
        fixture.polygon().vertices.clear();
        fixture.polygon().edges.front() = missing;

        const auto issues = fixture.document.validate();
        require(issues.size() == 4, "Validation should collect errors across the whole scene");
        requireIssue(issues, ValidationError::TooFewVertices, fixture.polygonId);
        requireIssue(issues, ValidationError::EdgeCountMismatch, fixture.polygonId);
        requireIssue(issues, ValidationError::MissingEdge, fixture.polygonId, {}, missing);
        requireIssue(issues, ValidationError::NonFinitePosition, secondId, secondVertexId);
    }

    void validationDoesNotModifyDocument()
    {
        Fixture fixture;
        const Polygon originalPolygon = fixture.polygon();
        Edge& edge = fixture.edge(0);
        edge.end = edge.start;
        const Edge brokenEdge = edge;

        const auto first = fixture.document.validate();
        const auto second = fixture.document.validate();
        require(first.size() == 1 && second.size() == 1, "Repeated validation changed the result");
        requireIssue(first, ValidationError::InvalidEdgeConnection, fixture.polygonId, {}, edge.id);
        requireIssue(second, ValidationError::InvalidEdgeConnection, fixture.polygonId, {}, edge.id);
        require(edge.id == brokenEdge.id && edge.start == brokenEdge.start && edge.end == brokenEdge.end,
            "Validation must not repair an edge implicitly");
        require(fixture.polygon().vertices == originalPolygon.vertices &&
                fixture.polygon().edges == originalPolygon.edges,
            "Validation changed polygon topology");

        const auto positions = triangle();
        const PolygonId nextId = fixture.document.addPolygon(positions);
        const Polygon* next = fixture.document.findPolygon(nextId);
        require(next != nullptr && nextId.value == fixture.polygonId.value + 1,
            "Validation consumed a polygon ID");
        require(next->vertices.front().value == originalPolygon.vertices.back().value + 1,
            "Validation consumed vertex IDs");
        require(next->edges.front().value == originalPolygon.edges.back().value + 1,
            "Validation consumed edge IDs");
    }
}

namespace cad::tests
{
    std::span<const TestCase> validationTests()
    {
        static const std::array tests{
            TestCase{"empty document", acceptsEmptyDocument},
            TestCase{"valid scene including concavity and self-intersection", acceptsValidScenes},
            TestCase{"too few vertices", reportsTooFewVertices},
            TestCase{"empty vertex list with existing edges", handlesEmptyVertexListWithEdges},
            TestCase{"too few edges", reportsTooFewEdges},
            TestCase{"extra edges without out-of-range access", handlesExtraEdges},
            TestCase{"missing listed vertex", reportsMissingListedVertex},
            TestCase{"missing listed edge", reportsMissingListedEdge},
            TestCase{"missing edge start vertex", reportsMissingStartVertex},
            TestCase{"missing edge end vertex", reportsMissingEndVertex},
            TestCase{"incorrect starts, ends and closing edge", reportsIncorrectConnections},
            TestCase{"NaN and infinity in either coordinate", reportsNonFinitePositions},
            TestCase{"collect all errors across polygons", collectsErrorsAcrossPolygons},
            TestCase{"validation preserves topology, errors and ID counters", validationDoesNotModifyDocument}
        };
        return tests;
    }
}
