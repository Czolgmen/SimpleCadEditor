#include <editor/HitTester.h>
#include <cad_core/model/Document.h>
#include <cad_core/model/EdgeSampling.h>

#include "../TestSupport.h"

#include <array>

namespace
{
    using namespace cad;
    using namespace cad::editor;
    using cad::tests::require;

    struct CurvedPolygon
    {
        PolygonId id;
        EdgeId curve;
    };

    CurvedPolygon addCurvedSquare(Document& document, double bend = -20, bool reverse = false,
        Vec2 offset = {}, double scale = 1)
    {
        auto positions = reverse
            ? std::array{Vec2{0, 0}, Vec2{0, 20}, Vec2{20, 20}, Vec2{20, 0}}
            : std::array{Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 20}, Vec2{0, 20}};
        for (auto& point : positions) point = point * scale + offset;
        const PolygonId id = document.addPolygon(positions);
        const EdgeId curve = document.findPolygon(id)->edges[reverse ? 3 : 0];
        require(document.convertEdgeToBezier(curve), "Curved square fixture conversion failed");
        auto* edge = const_cast<Edge*>(document.findEdge(curve));
        const Vec2 a = Vec2{0, bend} * scale + offset;
        const Vec2 b = Vec2{20, bend} * scale + offset;
        edge->geometry = reverse ? CubicBezier{b, a} : CubicBezier{a, b};
        return {id, curve};
    }

    std::vector<Vec2> samples(const Document& document, EdgeId id)
    {
        const auto* edge = document.findEdge(id);
        return sampleEdge(edge->geometry, document.findVertex(edge->start)->position, document.findVertex(edge->end)->position);
    }

    void edgeHitsKnownCurvePointsAwayFromChord()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        for (const Vec2 point : {Vec2{3.125, -11.25}, Vec2{10, -15}, Vec2{16.875, -11.25}})
            require(hitTestEdge(document, point, 1e-8) == polygon.curve, "Curve hit followed its chord instead of its shape");
    }

    void edgeHitsBetweenSamples()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        const auto points = samples(document, polygon.curve);
        const Vec2 between = (points[43] + points[44]) * 0.5;
        require(hitTestEdge(document, between, 1e-8) == polygon.curve, "Hit testing considered only sample vertices");
    }

    void edgeRejectsChordAndControlPolygon()
    {
        Document document;
        addCurvedSquare(document);
        require(!hitTestEdge(document, {10, 0}, 1), "Old straight chord remained selectable");
        require(!hitTestEdge(document, {10, -20}, 1), "Control polygon was mistaken for the curve");
    }

    void curveRespectsToleranceAndExactSampleHits()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        const auto points = samples(document, polygon.curve);
        require(hitTestEdge(document, points[43], 0) == polygon.curve, "Exact sample was rejected at zero tolerance");
        require(!hitTestEdge(document, points[43], -1), "Negative tolerance accepted a curve");
        require(hitTestEdge(document, {10, -15.5}, 0.51) == polygon.curve, "Nearby curve was rejected");
        require(!hitTestEdge(document, {10, -15.5}, 0.49), "Curve outside tolerance was selected");
    }

    void nearestCurveOrLineWinsAcrossPolygons()
    {
        for (const bool lineFirst : {false, true})
        for (const bool lineCloser : {false, true})
        {
            Document document;
            EdgeId line;
            CurvedPolygon curved{};
            const auto addLine = [&]
            {
                const double y = lineCloser ? -13.75 : -10;
                const std::array positions{Vec2{2, y}, Vec2{18, y}, Vec2{30, -40}};
                line = document.findPolygon(document.addPolygon(positions))->edges[0];
            };
            if (lineFirst) addLine();
            curved = addCurvedSquare(document);
            if (!lineFirst) addLine();
            require(hitTestEdge(document, {10, -14}, 5) == (lineCloser ? line : curved.curve),
                "Nearest comparison was reset between curves, fragments or polygons");
        }
    }

    void overlappingCurvesKeepFirstEdgeOnTie()
    {
        Document document;
        const auto first = addCurvedSquare(document);
        addCurvedSquare(document);
        require(hitTestEdge(document, {10, -15}, 1e-8) == first.curve, "Equal curve distances did not keep the first edge");
    }

    void outwardBulgeAddsSelectableInterior()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        for (const Vec2 point : {Vec2{10, -10}, Vec2{2, -2}, Vec2{10, 10}})
            require(hitTestPolygon(document, point) == polygon.id, "Outward bulge interior was ignored");
        for (const Vec2 point : {Vec2{10, -16}, Vec2{-1, -5}, Vec2{21, 10}})
            require(!hitTestPolygon(document, point), "Point outside the curved contour was accepted");
    }

    void inwardBulgeExcludesOldInterior()
    {
        Document document;
        const auto polygon = addCurvedSquare(document, 20);
        require(!hitTestPolygon(document, {10, 5}) && !hitTestPolygon(document, {10, 14}), "Inward curve retained the old chord interior");
        require(hitTestPolygon(document, {10, 16}) == polygon.id, "Remaining curved polygon interior was lost");
    }

    void orientationAndClosingCurvePreserveHits()
    {
        for (const bool reverse : {false, true})
        for (const double bend : {-20.0, 20.0})
        {
            Document document;
            const auto polygon = addCurvedSquare(document, bend, reverse);
            require(hitTestEdge(document, {10, bend * 0.75}, 1e-8) == polygon.curve, "Reversed or closing curve was not selectable");
            require(hitTestPolygon(document, {10, 17}) == polygon.id, "Orientation changed polygon interior");
            require(bool(hitTestPolygon(document, {10, -10})) == (bend < 0), "Closing curve changed parity incorrectly");
        }
    }

    void raysThroughSampleVerticesCountCrossingsOnce()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        const auto points = samples(document, polygon.curve);
        const double y = points[25].y;
        require(hitTestPolygon(document, {10, y}) == polygon.id, "Ray through sample vertices lost the interior");
        require(!hitTestPolygon(document, {-1, y}), "Ray through sample vertices double-counted crossings");
        require(!hitTestPolygon(document, {-1, points[50].y}), "Tangency at the curve minimum changed exterior parity");
    }

    void severalCurvesContributeToOneContour()
    {
        Document document;
        const auto polygon = addCurvedSquare(document);
        const EdgeId top = document.findPolygon(polygon.id)->edges[2];
        require(document.convertEdgeToBezier(top), "Second curve fixture failed");
        const_cast<Edge*>(document.findEdge(top))->geometry = CubicBezier{{20, 40}, {0, 40}};
        require(hitTestPolygon(document, {10, -10}) == polygon.id && hitTestPolygon(document, {10, 30}) == polygon.id,
            "One of the curved boundaries was ignored");
        require(!hitTestPolygon(document, {10, -16}) && !hitTestPolygon(document, {10, 36}), "Contour parity was reset per edge");
    }

    void twiceTracedCurvedContourHasEvenParity()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 20}, Vec2{0, 20},
            Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 20}, Vec2{0, 20}};
        const auto polygon = *document.findPolygon(document.addPolygon(positions));
        for (const auto index : {0, 4})
        {
            require(document.convertEdgeToBezier(polygon.edges[index]), "Repeated contour fixture failed");
            const_cast<Edge*>(document.findEdge(polygon.edges[index]))->geometry = CubicBezier{{0, -20}, {20, -20}};
        }
        require(!hitTestPolygon(document, {10, -10}) && !hitTestPolygon(document, {10, 10}), "Repeated curved contour used nonzero winding instead of parity");
        require(hitTestEdge(document, {10, -15}, 1e-8) == polygon.edges[0], "Repeated contour changed first-edge tie handling");
    }

    void brokenCurveEndpointsDoNotPreventOtherHits()
    {
        for (const bool start : {false, true})
        {
            Document document;
            const auto broken = addCurvedSquare(document);
            const auto good = addCurvedSquare(document, -20, false, {50, 0});
            auto* edge = const_cast<Edge*>(document.findEdge(broken.curve));
            (start ? edge->start : edge->end) = VertexId{999999};
            require(!hitTestEdge(document, {10, -15}, 1) && !hitTestPolygon(document, {10, 10}), "Broken curve remained selectable");
            require(hitTestEdge(document, {60, -15}, 1e-8) == good.curve && hitTestPolygon(document, {60, -10}) == good.id,
                "Broken curve blocked another valid polygon");
        }
    }

    void overlappingPolygonsChooseLastContainingCurvedRegion()
    {
        Document document;
        const auto first = addCurvedSquare(document);
        const auto last = addCurvedSquare(document);
        require(hitTestPolygon(document, {10, -10}) == last.id, "Overlap did not select the last drawn curved polygon");
        const_cast<Edge*>(document.findEdge(last.curve))->geometry = CubicBezier{{0, 20}, {20, 20}};
        require(hitTestPolygon(document, {10, -10}) == first.id, "Old chord interior masked the polygon underneath");
        require(hitTestPolygon(document, {10, 17}) == last.id, "Top polygon lost its remaining interior");
    }

    void scaledAndTranslatedCurvesRemainSelectable()
    {
        const Vec2 offset{-40.25, 30.5};
        for (const double scale : {0.01, 1.0, 1000.0})
        {
            Document document;
            const auto polygon = addCurvedSquare(document, -20, false, offset, scale);
            require(hitTestEdge(document, Vec2{10, -15} * scale + offset, 1e-7 * scale) == polygon.curve,
                "Scaled or translated curve hit failed");
            require(hitTestPolygon(document, Vec2{10, -10} * scale + offset) == polygon.id,
                "Scaled or translated curved interior failed");
            require(!hitTestPolygon(document, Vec2{10, -16} * scale + offset), "Scaled curved exterior was accepted");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> curveHitTesterTests()
    {
        static const std::array tests{
            TestCase{"known curve points away from the chord", edgeHitsKnownCurvePointsAwayFromChord},
            TestCase{"hits between sampled points", edgeHitsBetweenSamples},
            TestCase{"chord and control polygon are not the edge", edgeRejectsChordAndControlPolygon},
            TestCase{"curve tolerance and exact sample hits", curveRespectsToleranceAndExactSampleHits},
            TestCase{"nearest curve or line across polygons", nearestCurveOrLineWinsAcrossPolygons},
            TestCase{"overlapping curves retain first edge on a tie", overlappingCurvesKeepFirstEdgeOnTie},
            TestCase{"outward bulge adds interior", outwardBulgeAddsSelectableInterior},
            TestCase{"inward bulge excludes the old interior", inwardBulgeExcludesOldInterior},
            TestCase{"orientation and closing curve preserve hits", orientationAndClosingCurvePreserveHits},
            TestCase{"rays through sample vertices count crossings once", raysThroughSampleVerticesCountCrossingsOnce},
            TestCase{"several curves form one contour", severalCurvesContributeToOneContour},
            TestCase{"twice traced curved contour has even parity", twiceTracedCurvedContourHasEvenParity},
            TestCase{"broken curve endpoints permit unrelated hits", brokenCurveEndpointsDoNotPreventOtherHits},
            TestCase{"overlap selects last containing curved polygon", overlappingPolygonsChooseLastContainingCurvedRegion},
            TestCase{"scaled and translated curves remain selectable", scaledAndTranslatedCurvesRemainSelectable}
        };
        return tests;
    }
}
