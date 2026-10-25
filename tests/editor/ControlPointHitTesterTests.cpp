#include <editor/HitTester.h>
#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad;
    using cad::editor::hitTestControlPoint;
    using cad::tests::require;

    void setControls(Document& document, EdgeId edge, Vec2 first, Vec2 second)
    {
        require(document.convertEdgeToBezier(edge), "Control point fixture conversion failed");
        const_cast<Edge*>(document.findEdge(edge))->geometry = CubicBezier{first, second};
    }

    struct Fixture
    {
        Document document;
        Polygon polygon;
        EdgeId edge;
        const Vec2 first{10, -10};
        const Vec2 second{40, -10};

        Fixture()
        {
            const std::array positions{Vec2{0, 0}, Vec2{30, 0}, Vec2{30, 30}, Vec2{0, 30}};
            polygon = *document.findPolygon(document.addPolygon(positions));
            edge = polygon.edges[0];
            setControls(document, edge, first, second);
        }
    };

    EdgeId addOtherCurve(Document& document, Vec2 first, Vec2 second)
    {
        const std::array positions{Vec2{100, 100}, Vec2{130, 100}, Vec2{100, 130}};
        const EdgeId edge = document.findPolygon(document.addPolygon(positions))->edges[0];
        setControls(document, edge, first, second);
        return edge;
    }

    void emptyDocumentAndSegmentsHaveNoHandles()
    {
        Document document;
        require(!hitTestControlPoint(document, {0, 0}, 1000), "Empty document returned a handle");
        const std::array positions{Vec2{0, 0}, Vec2{30, 0}, Vec2{0, 30}};
        document.addPolygon(positions);
        require(!hitTestControlPoint(document, {0, 0}, 1000), "Segment endpoints were treated as handles");
    }

    void exactHitsIdentifyBothHandlesAtZeroTolerance()
    {
        Fixture f;
        const Vec2 first{-12.5, 7.25}, second{101.125, -90.75};
        setControls(f.document, f.edge, first, second);
        require(hitTestControlPoint(f.document, first, 0) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Exact first handle was not identified at zero tolerance");
        require(hitTestControlPoint(f.document, second, -0.0) == ControlPointRef{f.edge, BezierControlPoint::Second},
            "Exact second handle was not identified at signed zero tolerance");
    }

    void distanceIsMeasuredFromCursorInsteadOfOrigin()
    {
        Fixture f;
        setControls(f.document, f.edge, {0.25, 0.25}, {100, 200});
        require(hitTestControlPoint(f.document, {100, 200}, 1) == ControlPointRef{f.edge, BezierControlPoint::Second},
            "Distance was measured from the coordinate origin instead of the cursor");
        require(!hitTestControlPoint(f.document, {500, 500}, 1), "Handle near the origin matched a distant cursor");
    }

    void zeroToleranceRejectsNearbyCursor()
    {
        Fixture f;
        require(!hitTestControlPoint(f.document, f.first + Vec2{0.001, 0}, 0), "Zero tolerance accepted a nearby cursor");
    }

    void nearestHandleWinsWithinOneCurve()
    {
        Fixture f;
        require(hitTestControlPoint(f.document, {39, -10}, 50) == ControlPointRef{f.edge, BezierControlPoint::Second},
            "Closer second handle did not replace the first");
        require(hitTestControlPoint(f.document, {11, -10}, 50) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Farther second handle replaced the nearest first handle");
    }

    void nearestHandleWinsAcrossEdgesAndPolygons()
    {
        for (const bool otherPolygon : {false, true})
        {
            Fixture f;
            EdgeId other;
            if (otherPolygon)
                other = addOtherCurve(f.document, {10.5, -10}, {100, 100});
            else
            {
                other = f.polygon.edges[1];
                setControls(f.document, other, {10.5, -10}, {100, 100});
            }
            require(hitTestControlPoint(f.document, {10.4, -10}, 5) == ControlPointRef{other, BezierControlPoint::First},
                "Closer handle from a later edge or polygon was ignored");
            require(hitTestControlPoint(f.document, {10.1, -10}, 5) == ControlPointRef{f.edge, BezierControlPoint::First},
                "Nearest distance was reset while visiting later edges or polygons");
        }
    }

    void equalAndCoincidentHandlesKeepFirst()
    {
        Fixture f;
        setControls(f.document, f.edge, {10, -10}, {20, -10});
        require(hitTestControlPoint(f.document, {15, -10}, 5) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Equal distances must keep the first handle");
        setControls(f.document, f.edge, f.first, f.first);
        require(hitTestControlPoint(f.document, f.first, 0) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Coincident handles must keep the first handle");
    }

    void tiesAcrossCurvesKeepEarlierHandleReference()
    {
        Fixture f;
        setControls(f.document, f.edge, {200, 200}, {10, -10});
        addOtherCurve(f.document, {10, -10}, {100, 100});
        require(hitTestControlPoint(f.document, {10, -10}, 0) == ControlPointRef{f.edge, BezierControlPoint::Second},
            "Later coincident curve replaced an earlier handle reference");
    }

    void toleranceIsCircularAndIncludesBoundary()
    {
        Fixture f;
        const Vec2 point{13, -6}; // Offset (3, 4) from control1: distance exactly 5.
        require(hitTestControlPoint(f.document, point, 5) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Exact tolerance boundary was excluded");
        require(!hitTestControlPoint(f.document, point, std::nextafter(5.0, 0.0)), "Point outside tolerance was accepted");
        require(!hitTestControlPoint(f.document, {14, -6}, 5), "Tolerance used a square instead of a circle");
    }

    void invalidToleranceIsRejected()
    {
        Fixture f;
        for (const double tolerance : {-1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
            require(!hitTestControlPoint(f.document, f.first, tolerance), "Invalid tolerance returned a handle");
    }

    void nonFiniteCursorHasNoHit()
    {
        Fixture f;
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            const Vec2 cursor = x ? Vec2{value, f.first.y} : Vec2{f.first.x, value};
            require(!hitTestControlPoint(f.document, cursor, 6), "Non-finite cursor returned a handle");
        }
    }

    void missingEdgeReferenceIsSkipped()
    {
        Fixture f;
        const auto other = addOtherCurve(f.document, {80, 90}, {100, 110});
        const_cast<Polygon*>(f.document.findPolygon(f.polygon.id))->edges[0] = EdgeId{999999};
        require(!hitTestControlPoint(f.document, f.first, 0), "Unreferenced curve remained selectable");
        require(hitTestControlPoint(f.document, {80, 90}, 0) == ControlPointRef{other, BezierControlPoint::First},
            "Missing edge reference blocked a valid curve");
    }

    void missingEndpointsAreSkipped()
    {
        for (const bool start : {false, true})
        {
            Fixture f;
            const auto other = addOtherCurve(f.document, {80, 90}, {100, 110});
            auto* edge = const_cast<Edge*>(f.document.findEdge(f.edge));
            (start ? edge->start : edge->end) = VertexId{999999};
            require(!hitTestControlPoint(f.document, f.first, 0), "Curve with a missing endpoint remained selectable");
            require(hitTestControlPoint(f.document, {80, 90}, 0) == ControlPointRef{other, BezierControlPoint::First},
                "Missing endpoint blocked a valid curve");
        }
    }

    void nonFiniteHandleDoesNotHideTheOtherHandle()
    {
        for (const bool first : {false, true})
        for (const bool x : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            auto& curve = std::get<CubicBezier>(const_cast<Edge*>(f.document.findEdge(f.edge))->geometry);
            auto& damaged = first ? curve.control1 : curve.control2;
            (x ? damaged.x : damaged.y) = invalid;
            const Vec2 valid = first ? f.second : f.first;
            const auto expected = first ? BezierControlPoint::Second : BezierControlPoint::First;
            require(hitTestControlPoint(f.document, valid, 0) == ControlPointRef{f.edge, expected},
                "Non-finite control prevented hitting the other valid handle");
        }
    }

    void handlesMayCoincideWithVertices()
    {
        Fixture f;
        const Vec2 start = f.document.findVertex(f.polygon.vertices[0])->position;
        const Vec2 end = f.document.findVertex(f.polygon.vertices[1])->position;
        setControls(f.document, f.edge, start, end);
        require(hitTestControlPoint(f.document, start, 0) == ControlPointRef{f.edge, BezierControlPoint::First},
            "First control at the start vertex was lost");
        require(hitTestControlPoint(f.document, end, 0) == ControlPointRef{f.edge, BezierControlPoint::Second},
            "Second control at the end vertex was lost");
    }

    void hitsFollowUpdatedControlsAndGeometryType()
    {
        Fixture f;
        const Vec2 moved = f.first + Vec2{3, 4};
        setControls(f.document, f.edge, moved, f.second);
        require(!hitTestControlPoint(f.document, f.first, 0), "Old handle position remained selectable");
        require(hitTestControlPoint(f.document, moved, 0) == ControlPointRef{f.edge, BezierControlPoint::First},
            "Updated handle position was not used");
        require(f.document.convertEdgeToSegment(f.edge), "Segment conversion fixture failed");
        require(!hitTestControlPoint(f.document, moved, 100), "Converted segment retained selectable handles");
    }
}

namespace cad::tests
{
    std::span<const TestCase> controlPointHitTesterTests()
    {
        static const std::array tests{
            TestCase{"empty document and segments have no handles", emptyDocumentAndSegmentsHaveNoHandles},
            TestCase{"exact hits identify both handles at zero tolerance", exactHitsIdentifyBothHandlesAtZeroTolerance},
            TestCase{"distance is measured from cursor instead of origin", distanceIsMeasuredFromCursorInsteadOfOrigin},
            TestCase{"zero tolerance rejects nearby cursor", zeroToleranceRejectsNearbyCursor},
            TestCase{"nearest handle within one curve", nearestHandleWinsWithinOneCurve},
            TestCase{"nearest handle across edges and polygons", nearestHandleWinsAcrossEdgesAndPolygons},
            TestCase{"equal and coincident handles keep first", equalAndCoincidentHandlesKeepFirst},
            TestCase{"ties across curves keep earlier reference", tiesAcrossCurvesKeepEarlierHandleReference},
            TestCase{"circular tolerance includes exact boundary", toleranceIsCircularAndIncludesBoundary},
            TestCase{"invalid tolerance is rejected", invalidToleranceIsRejected},
            TestCase{"non-finite cursor has no hit", nonFiniteCursorHasNoHit},
            TestCase{"missing edge reference is skipped", missingEdgeReferenceIsSkipped},
            TestCase{"missing endpoints are skipped", missingEndpointsAreSkipped},
            TestCase{"non-finite handle does not hide valid handle", nonFiniteHandleDoesNotHideTheOtherHandle},
            TestCase{"handles may coincide with vertices", handlesMayCoincideWithVertices},
            TestCase{"hits follow updated controls and geometry type", hitsFollowUpdatedControlsAndGeometryType}
        };
        return tests;
    }
}
