#include <editor/HitTester.h>
#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <array>

namespace
{
    using namespace cad;
    using cad::editor::hitTestVertex;
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
            TestCase{"hit testing follows a moved vertex", hitTestingUsesMovedPosition}
        };
        return tests;
    }
}
