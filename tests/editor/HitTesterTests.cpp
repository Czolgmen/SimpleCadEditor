#include <editor/HitTester.h>
#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <limits>

namespace
{
    using namespace cad;
    using cad::editor::hitTestVertex;
    using cad::editor::hitTestPolygon;
    using cad::editor::hitTestEdge;
    using cad::tests::require;

    struct Fixture
    {
        Document document;
        std::array<VertexId, 3> vertices;

        Fixture()
        {
            const std::array positions{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 30}};
            const PolygonId id = document.addPolygon(positions);
            const auto& ids = document.findPolygon(id)->vertices;
            vertices = {ids[0], ids[1], ids[2]};
        }
    };

    void emptyDocumentHasNoHit()
    {
        const Document document;
        require(!hitTestVertex(document, Vec2{}, 6), "Empty document produced a hit");
    }

    void zeroToleranceFindsExactPositions()
    {
        const Fixture fixture;
        for (const VertexId id : fixture.vertices)
        {
            const Vec2 position = fixture.document.findVertex(id)->position;
            require(hitTestVertex(fixture.document, position, 0) == id,
                "Zero tolerance must accept an exact vertex position");
        }
    }

    void zeroToleranceRejectsNearbyPosition()
    {
        const Fixture fixture;
        require(!hitTestVertex(fixture.document, Vec2{0.25, 0}, 0),
            "Zero tolerance accepted a nearby position");
    }

    void selectsCloserLaterVertex()
    {
        const Fixture fixture;
        require(hitTestVertex(fixture.document, Vec2{8, 1}, 12) == fixture.vertices[1],
            "Must choose the nearest candidate, not the first within tolerance");
    }

    void keepsCloserEarlierVertex()
    {
        const Fixture fixture;
        require(hitTestVertex(fixture.document, Vec2{1, 1}, 20) == fixture.vertices[0],
            "A farther candidate must not replace a closer vertex");
    }

    void selectsNearestAcrossPolygons()
    {
        Fixture fixture;
        const std::array positions{Vec2{4, 0}, Vec2{40, 0}, Vec2{4, 40}};
        const PolygonId id = fixture.document.addPolygon(positions);
        const VertexId expected = fixture.document.findPolygon(id)->vertices[0];
        require(hitTestVertex(fixture.document, Vec2{3, 0}, 10) == expected,
            "Must compare candidates from all polygons");
    }

    void acceptsDistanceExactlyAtTolerance()
    {
        const Fixture fixture;
        // Distance from (3, 4) to the origin is exactly 5.
        require(hitTestVertex(fixture.document, Vec2{3, 4}, 5) == fixture.vertices[0],
            "A vertex exactly on the tolerance boundary must be accepted");
    }

    void rejectsPointOutsideCircularTolerance()
    {
        const Fixture fixture;
        require(!hitTestVertex(fixture.document, Vec2{4, 4}, 5),
            "Tolerance must describe a circle, not a square around a vertex");
    }

    void returnsNoHitWhenAllVerticesAreTooFar()
    {
        const Fixture fixture;
        require(!hitTestVertex(fixture.document, Vec2{50, 50}, 6),
            "Vertices outside tolerance must not produce a hit");
    }

    void rejectsNegativeToleranceEvenAtExactPosition()
    {
        const Fixture fixture;
        require(!hitTestVertex(fixture.document, Vec2{}, -6),
            "Negative tolerance must not become positive after squaring");
    }

    void equalDistancesKeepFirstVertex()
    {
        const Fixture fixture;
        require(hitTestVertex(fixture.document, Vec2{5, 0}, 5) == fixture.vertices[0],
            "Equally distant candidates must preserve the first encountered vertex");
    }

    void overlappingPolygonsKeepFirstVertex()
    {
        Fixture fixture;
        const std::array positions{Vec2{0, 0}, Vec2{-10, 0}, Vec2{0, -10}};
        fixture.document.addPolygon(positions);
        require(hitTestVertex(fixture.document, Vec2{}, 0) == fixture.vertices[0],
            "Coincident vertices must preserve the first encountered candidate");
    }

    void supportsNegativeAndFractionalWorldCoordinates()
    {
        Document document;
        const std::array positions{
            Vec2{-3.5, -2.25}, Vec2{-20, -2}, Vec2{-3, -20}};
        const PolygonId id = document.addPolygon(positions);
        const VertexId expected = document.findPolygon(id)->vertices[0];
        require(hitTestVertex(document, Vec2{-3, -2.25}, 0.5) == expected,
            "Hit testing must support negative and fractional world coordinates");
    }

    void hitTestingUsesMovedPosition()
    {
        Fixture fixture;
        const VertexId id = fixture.vertices[0];
        require(fixture.document.moveVertex(id, Vec2{100, 80}), "Fixture move failed");
        require(!hitTestVertex(fixture.document, Vec2{}, 1),
            "The old position must stop producing a hit after a move");
        require(hitTestVertex(fixture.document, Vec2{100, 80}, 1) == id,
            "The moved vertex must be found at its new position");
    }
    std::array<Vec2, 4> square()
    {
        return {Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}};
    }

    struct EdgeFixture
    {
        Document document;
        PolygonId polygonId;
        std::array<EdgeId, 4> edges;

        EdgeFixture()
        {
            polygonId = document.addPolygon(square());
            const auto& ids = document.findPolygon(polygonId)->edges;
            edges = {ids.at(0), ids.at(1), ids.at(2), ids.at(3)};
        }
    };

    void emptyDocumentHasNoPolygonHit()
    {
        const Document document;
        require(!hitTestPolygon(document, Vec2{5, 5}), "Empty document produced a polygon hit");
    }

    void findsOnlyPolygonAndRejectsTriangleExterior()
    {
        const Fixture fixture;
        const PolygonId id = fixture.document.getPolygons().begin()->first;
        require(hitTestPolygon(fixture.document, Vec2{2, 3}) == id,
            "The only polygon must be selectable");
        require(!hitTestPolygon(fixture.document, Vec2{8, 10}),
            "A point in the bounding box but outside the triangle must miss");
        require(!hitTestPolygon(fixture.document, Vec2{-1, 3}),
            "The closing edge must participate in intersection counting");
    }

    void concavePolygonExcludesItsNotch()
    {
        Document document;
        const std::array positions{
            Vec2{0, 0}, Vec2{8, 0}, Vec2{8, 2}, Vec2{2, 2}, Vec2{2, 8}, Vec2{0, 8}};
        const PolygonId id = document.addPolygon(positions);
        require(hitTestPolygon(document, Vec2{1, 6}) == id, "Vertical arm of L shape was missed");
        require(hitTestPolygon(document, Vec2{6, 1}) == id, "Horizontal arm of L shape was missed");
        require(!hitTestPolygon(document, Vec2{6, 6}), "Concave notch must be outside");
    }

    void polygonOrientationDoesNotChangeHits()
    {
        auto positions = square();
        for (int orientation = 0; orientation < 2; ++orientation)
        {
            Document document;
            const PolygonId id = document.addPolygon(positions);
            require(hitTestPolygon(document, Vec2{3, 4}) == id,
                "Polygon orientation changed an interior hit");
            require(!hitTestPolygon(document, Vec2{-2, 4}),
                "Polygon orientation changed an exterior miss");
            std::reverse(positions.begin(), positions.end());
        }
    }

    void rayThroughVertexCountsCrossingOnce()
    {
        Document document;
        const std::array diamond{Vec2{0, 5}, Vec2{5, 0}, Vec2{10, 5}, Vec2{5, 10}};
        const PolygonId id = document.addPolygon(diamond);
        // Query points are off the boundary, but their horizontal rays pass through vertices.
        require(hitTestPolygon(document, Vec2{5, 5}) == id,
            "A ray through a vertex must count the crossing exactly once");
        require(!hitTestPolygon(document, Vec2{-1, 5}), "Two vertex crossings must leave a point outside");
        require(!hitTestPolygon(document, Vec2{11, 5}), "Point to the right of the diamond must miss");
    }

    void selfIntersectingPolygonUsesEvenOddInterior()
    {
        Document document;
        const std::array bowTie{Vec2{0, 0}, Vec2{10, 10}, Vec2{0, 10}, Vec2{10, 0}};
        const PolygonId id = document.addPolygon(bowTie);
        require(hitTestPolygon(document, Vec2{5, 2}) == id, "Lower lobe of bow tie was missed");
        require(hitTestPolygon(document, Vec2{5, 8}) == id, "Upper lobe of bow tie was missed");
        require(!hitTestPolygon(document, Vec2{1, 5}), "Point outside the left side of bow tie must miss");
        require(!hitTestPolygon(document, Vec2{9, 5}), "Point outside the right side of bow tie must miss");
    }

    void twiceTracedLoopHasEvenParity()
    {
        Document document;
        // Trace the same square twice in the same direction: even-odd differs from winding.
        const std::array positions{
            Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10},
            Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}};
        document.addPolygon(positions);
        require(!hitTestPolygon(document, Vec2{5, 5}),
            "Two complete loops must cancel under the even-odd rule");
    }

    void overlappingPolygonsSelectLastDrawnHit()
    {
        Document document;
        const PolygonId bottom = document.addPolygon(square());
        const std::array topPositions{Vec2{5, 5}, Vec2{15, 5}, Vec2{15, 15}, Vec2{5, 15}};
        const PolygonId top = document.addPolygon(topPositions);
        require(hitTestPolygon(document, Vec2{7, 7}) == top,
            "Overlapping polygons must select the last drawn hit");
        require(hitTestPolygon(document, Vec2{2, 2}) == bottom,
            "A miss in the top polygon must allow selecting the bottom polygon");
        require(hitTestPolygon(document, Vec2{13, 13}) == top, "Top-only region was missed");
        require(!hitTestPolygon(document, Vec2{30, 30}), "Point outside every polygon must miss");
    }

    void disjointPolygonsAreAllSelectable()
    {
        Document document;
        std::array<PolygonId, 3> ids;
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            auto positions = square();
            const double offset = 20 * static_cast<double>(i);
            for (Vec2& position : positions)
                position.x += offset;
            ids[i] = document.addPolygon(positions);
        }
        for (std::size_t i = 0; i < ids.size(); ++i)
            require(hitTestPolygon(document, Vec2{5 + 20 * static_cast<double>(i), 5}) == ids[i],
                "Reverse traversal must visit the first, middle and last polygon");
    }

    void polygonHitsSupportNegativeFractionalCoordinates()
    {
        Document document;
        const std::array positions{
            Vec2{-4.5, -3.25}, Vec2{-1.5, -3.25}, Vec2{-1.5, -0.25}, Vec2{-4.5, -0.25}};
        const PolygonId id = document.addPolygon(positions);
        require(hitTestPolygon(document, Vec2{-3, -2}) == id,
            "Negative fractional polygon interior was missed");
        require(!hitTestPolygon(document, Vec2{-5, -2}), "Point outside negative-coordinate polygon must miss");
    }

    void polygonHitFollowsWholePolygonMove()
    {
        Document document;
        const PolygonId id = document.addPolygon(square());
        require(hitTestPolygon(document, Vec2{5, 5}) == id, "Initial polygon hit failed");
        require(document.movePolygon(id, Vec2{100, -40}), "Fixture polygon move failed");
        require(!hitTestPolygon(document, Vec2{5, 5}), "Old polygon position must stop producing a hit");
        require(hitTestPolygon(document, Vec2{105, -35}) == id, "New polygon position was missed");
    }

    void missingEdgeRejectsPolygonHit()
    {
        Document document;
        const PolygonId id = document.addPolygon(square());
        // Deliberate corruption of an originally non-const document, limited to tests.
        auto* polygon = const_cast<Polygon*>(document.findPolygon(id));
        polygon->edges.back() = EdgeId{std::numeric_limits<std::uint64_t>::max()};
        require(!hitTestPolygon(document, Vec2{5, 5}), "A polygon with a missing edge must not be hit");
    }

    void missingEdgeEndpointRejectsPolygonHit()
    {
        for (const bool missingStart : {true, false})
        {
            Document document;
            const PolygonId id = document.addPolygon(square());
            const Polygon* polygon = document.findPolygon(id);
            auto* edge = const_cast<Edge*>(document.findEdge(polygon->edges.back()));
            const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
            if (missingStart)
                edge->start = missing;
            else
                edge->end = missing;
            require(!hitTestPolygon(document, Vec2{5, 5}),
                "A polygon with a missing edge endpoint must not be hit");
        }
    }
    void emptyDocumentHasNoEdgeHit()
    {
        const Document document;
        require(!hitTestEdge(document, Vec2{}, 6), "Empty document produced an edge hit");
    }

    void zeroToleranceAcceptsPointsOnEveryEdge()
    {
        const EdgeFixture fixture;
        const std::array midpoints{Vec2{5, 0}, Vec2{10, 5}, Vec2{5, 10}, Vec2{0, 5}};
        for (std::size_t i = 0; i < midpoints.size(); ++i)
            require(hitTestEdge(fixture.document, midpoints[i], 0) == fixture.edges[i],
                "Zero tolerance must accept a point on an edge, including the closing edge");
        require(hitTestEdge(fixture.document, Vec2{}, 0) == fixture.edges[0],
            "Zero tolerance must accept an endpoint and preserve the first candidate");
    }

    void zeroToleranceRejectsPointOffEdge()
    {
        const EdgeFixture fixture;
        require(!hitTestEdge(fixture.document, Vec2{5, -0.25}, 0),
            "Zero tolerance accepted a point off every edge");
    }

    void negativeToleranceRejectsExactEdgeHit()
    {
        const EdgeFixture fixture;
        require(!hitTestEdge(fixture.document, Vec2{5, 0}, -6),
            "Negative tolerance must reject even an exact edge hit");
    }

    void edgeToleranceIncludesExactBoundary()
    {
        const EdgeFixture fixture;
        require(hitTestEdge(fixture.document, Vec2{5, -4}, 4) == fixture.edges[0],
            "Distance exactly equal to tolerance must be accepted using squared units");
        require(!hitTestEdge(fixture.document, Vec2{5, -4.25}, 4),
            "An edge just outside tolerance must not be hit");
    }

    void closerLaterEdgeReplacesEarlierCandidate()
    {
        const EdgeFixture fixture;
        require(hitTestEdge(fixture.document, Vec2{5, 9}, 10) == fixture.edges[2],
            "A closer later edge must replace the first candidate");
    }

    void fartherLaterEdgeDoesNotReplaceNearest()
    {
        const EdgeFixture fixture;
        require(hitTestEdge(fixture.document, Vec2{5, 1}, 10) == fixture.edges[0],
            "A farther later edge must not replace the nearest candidate");
    }

    void nearestEdgeIsSelectedAcrossPolygons()
    {
        EdgeFixture fixture;
        const std::array positions{Vec2{0, 20}, Vec2{10, 20}, Vec2{10, 30}, Vec2{0, 30}};
        const PolygonId otherId = fixture.document.addPolygon(positions);
        const EdgeId otherBottom = fixture.document.findPolygon(otherId)->edges.front();
        require(hitTestEdge(fixture.document, Vec2{5, 18}, 10) == otherBottom,
            "A closer edge in a later polygon must win");
        require(hitTestEdge(fixture.document, Vec2{5, 12}, 10) == fixture.edges[2],
            "A closer edge in an earlier polygon must remain selected");
    }

    void equalEdgeDistancesKeepFirstCandidate()
    {
        EdgeFixture fixture;
        require(hitTestEdge(fixture.document, Vec2{5, 5}, 5) == fixture.edges[0],
            "Equidistant edges must preserve the first encountered candidate");
        fixture.document.addPolygon(square());
        require(hitTestEdge(fixture.document, Vec2{5, 0}, 0) == fixture.edges[0],
            "Coincident edges from different polygons must preserve the first candidate");
    }

    void edgeProjectionIsClampedToBothEndpoints()
    {
        Document document;
        const std::array positions{Vec2{10, 0}, Vec2{20, 0}, Vec2{15, 30}};
        const PolygonId id = document.addPolygon(positions);
        const EdgeId bottom = document.findPolygon(id)->edges.front();
        // Infinite-line distance is 4; the nearest endpoint is 5 units away.
        for (const Vec2 point : {Vec2{7, -4}, Vec2{23, -4}})
        {
            require(!hitTestEdge(document, point, 4.5),
                "Projection outside the segment must use the endpoint distance");
            require(hitTestEdge(document, point, 5) == bottom,
                "The nearest endpoint must be accepted on the tolerance boundary");
        }
    }

    void diagonalEdgesSupportFractionalCoordinatesAndBothDirections()
    {
        std::array positions{Vec2{-3.5, -2.5}, Vec2{6.5, 7.5}, Vec2{-30, 30}};
        for (int orientation = 0; orientation < 2; ++orientation)
        {
            Document document;
            const PolygonId id = document.addPolygon(positions);
            const EdgeId diagonal = document.findPolygon(id)->edges.at(orientation == 0 ? 0 : 1);
            // Squared distance to the diagonal is 2, so 1.25 misses and 1.5 hits.
            require(!hitTestEdge(document, Vec2{-1.5, -2.5}, 1.25),
                "Diagonal edge outside tolerance must be rejected");
            require(hitTestEdge(document, Vec2{-1.5, -2.5}, 1.5) == diagonal,
                "Diagonal hit must support fractional coordinates and reversed endpoints");
            std::reverse(positions.begin(), positions.end());
        }
    }

    void zeroLengthEdgeIsTreatedAsPoint()
    {
        Document document;
        const std::array positions{Vec2{2, 3}, Vec2{2, 3}, Vec2{20, 30}};
        const PolygonId id = document.addPolygon(positions);
        const EdgeId collapsed = document.findPolygon(id)->edges.front();
        require(hitTestEdge(document, Vec2{2, 3}, 0) == collapsed,
            "Collapsed edge must support an exact hit without division by zero");
        require(hitTestEdge(document, Vec2{-1, -1}, 5) == collapsed,
            "Collapsed edge must use distance to its only point");
        require(!hitTestEdge(document, Vec2{-1, -1}, 4.5),
            "Collapsed edge outside tolerance must be rejected");
    }

    void edgeHitsFollowVertexAndPolygonMoves()
    {
        EdgeFixture fixture;
        const auto vertices = fixture.document.findPolygon(fixture.polygonId)->vertices;
        require(fixture.document.moveVertex(vertices[0], Vec2{0, -10}), "First fixture vertex move failed");
        require(fixture.document.moveVertex(vertices[1], Vec2{10, -10}), "Second fixture vertex move failed");
        require(!hitTestEdge(fixture.document, Vec2{5, 0}, 0.5),
            "Old edge position must stop producing hits after moving its endpoints");
        require(hitTestEdge(fixture.document, Vec2{5, -10}, 0) == fixture.edges[0],
            "Edge must be found at its new endpoint positions");
        require(fixture.document.movePolygon(fixture.polygonId, Vec2{100, 0}), "Fixture polygon move failed");
        require(!hitTestEdge(fixture.document, Vec2{5, -10}, 0.5),
            "Old edge position must stop producing hits after moving the polygon");
        require(hitTestEdge(fixture.document, Vec2{105, -10}, 0) == fixture.edges[0],
            "Edge must follow a whole polygon move without changing its ID");
    }

    void edgeHitSkipsMissingEdgeReference()
    {
        EdgeFixture fixture;
        // Test-only corruption of an originally non-const document.
        auto* polygon = const_cast<Polygon*>(fixture.document.findPolygon(fixture.polygonId));
        polygon->edges[0] = EdgeId{std::numeric_limits<std::uint64_t>::max()};
        require(!hitTestEdge(fixture.document, Vec2{5, 0}, 0), "Missing edge must be skipped");
        require(hitTestEdge(fixture.document, Vec2{10, 5}, 0) == fixture.edges[1],
            "A missing edge must not prevent finding another valid edge");
    }

    void edgeHitSkipsMissingStartOrEnd()
    {
        for (const bool missingStart : {true, false})
        {
            EdgeFixture fixture;
            auto* edge = const_cast<Edge*>(fixture.document.findEdge(fixture.edges[0]));
            const VertexId missing{std::numeric_limits<std::uint64_t>::max()};
            if (missingStart)
                edge->start = missing;
            else
                edge->end = missing;
            require(!hitTestEdge(fixture.document, Vec2{5, 0}, 0),
                "Edge with a missing endpoint must be skipped");
            require(hitTestEdge(fixture.document, Vec2{10, 5}, 0) == fixture.edges[1],
                "A missing endpoint must not prevent finding another valid edge");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> hitTesterTests()
    {
        static const std::array tests{
            TestCase{"empty document has no hit", emptyDocumentHasNoHit},
            TestCase{"zero tolerance accepts exact positions", zeroToleranceFindsExactPositions},
            TestCase{"zero tolerance rejects nearby positions", zeroToleranceRejectsNearbyPosition},
            TestCase{"select nearest later candidate", selectsCloserLaterVertex},
            TestCase{"keep nearest earlier candidate", keepsCloserEarlierVertex},
            TestCase{"select nearest across polygons", selectsNearestAcrossPolygons},
            TestCase{"accept exact tolerance boundary", acceptsDistanceExactlyAtTolerance},
            TestCase{"use circular tolerance", rejectsPointOutsideCircularTolerance},
            TestCase{"no hit outside tolerance", returnsNoHitWhenAllVerticesAreTooFar},
            TestCase{"reject negative tolerance", rejectsNegativeToleranceEvenAtExactPosition},
            TestCase{"equal distances keep first vertex", equalDistancesKeepFirstVertex},
            TestCase{"coincident vertices keep first candidate", overlappingPolygonsKeepFirstVertex},
            TestCase{"negative and fractional world coordinates", supportsNegativeAndFractionalWorldCoordinates},
            TestCase{"hit testing follows a moved vertex", hitTestingUsesMovedPosition},
            TestCase{"empty document has no polygon hit", emptyDocumentHasNoPolygonHit},
            TestCase{"only polygon, triangle interior and exterior", findsOnlyPolygonAndRejectsTriangleExterior},
            TestCase{"concave polygon excludes its notch", concavePolygonExcludesItsNotch},
            TestCase{"polygon orientation does not affect hits", polygonOrientationDoesNotChangeHits},
            TestCase{"ray through vertex counts crossing once", rayThroughVertexCountsCrossingOnce},
            TestCase{"self-intersecting polygon interior", selfIntersectingPolygonUsesEvenOddInterior},
            TestCase{"twice traced loop has even parity", twiceTracedLoopHasEvenParity},
            TestCase{"overlapping polygons select last drawn hit", overlappingPolygonsSelectLastDrawnHit},
            TestCase{"all disjoint polygons are selectable", disjointPolygonsAreAllSelectable},
            TestCase{"polygon hits at negative fractional coordinates", polygonHitsSupportNegativeFractionalCoordinates},
            TestCase{"polygon hit follows whole polygon move", polygonHitFollowsWholePolygonMove},
            TestCase{"missing edge rejects polygon hit", missingEdgeRejectsPolygonHit},
            TestCase{"missing start or end rejects polygon hit", missingEdgeEndpointRejectsPolygonHit},
            TestCase{"empty document has no edge hit", emptyDocumentHasNoEdgeHit},
            TestCase{"zero tolerance accepts every edge and endpoints", zeroToleranceAcceptsPointsOnEveryEdge},
            TestCase{"zero tolerance rejects point off edge", zeroToleranceRejectsPointOffEdge},
            TestCase{"negative tolerance rejects exact edge hit", negativeToleranceRejectsExactEdgeHit},
            TestCase{"edge tolerance includes exact boundary", edgeToleranceIncludesExactBoundary},
            TestCase{"closer later edge replaces earlier candidate", closerLaterEdgeReplacesEarlierCandidate},
            TestCase{"farther later edge does not replace nearest", fartherLaterEdgeDoesNotReplaceNearest},
            TestCase{"nearest edge across polygons", nearestEdgeIsSelectedAcrossPolygons},
            TestCase{"equal edge distances keep first candidate", equalEdgeDistancesKeepFirstCandidate},
            TestCase{"edge projection clamps to both endpoints", edgeProjectionIsClampedToBothEndpoints},
            TestCase{"diagonal fractional edges in both directions", diagonalEdgesSupportFractionalCoordinatesAndBothDirections},
            TestCase{"zero-length edge is a point", zeroLengthEdgeIsTreatedAsPoint},
            TestCase{"edge hits follow vertex and polygon moves", edgeHitsFollowVertexAndPolygonMoves},
            TestCase{"edge hit skips missing edge reference", edgeHitSkipsMissingEdgeReference},
            TestCase{"edge hit skips missing start or end", edgeHitSkipsMissingStartOrEnd}
        };
        return tests;
    }
}
