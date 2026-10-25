#include "../CurveTestSupport.h"

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;

    void movesOnlyRequestedHandleToAbsolutePosition()
    {
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        {
            CurveTestScene scene;
            const Document before = scene.document;
            const auto control = scene.control(which);
            for (const Vec2 target : {Vec2{-4.5, 7.25}, Vec2{12, -8}, Vec2{12, -8}})
            {
                require(scene.document.moveControlPoint(control, target), "Valid absolute handle move rejected");
                requireSameCurveDocument(scene.document, withMovedCurveControl(before, control, target));
            }
        }
    }

    void invalidReferencesAndEnumsPreserveDocument()
    {
        CurveTestScene scene;
        const Document before = scene.document;
        for (const auto invalid : std::array{
            ControlPointRef{EdgeId{}, BezierControlPoint::First},
            ControlPointRef{EdgeId{999999}, BezierControlPoint::Second},
            ControlPointRef{scene.first.edges[1], BezierControlPoint::First},
            ControlPointRef{scene.first.edges[0], static_cast<BezierControlPoint>(-1)},
            ControlPointRef{scene.first.edges[0], static_cast<BezierControlPoint>(99)}})
        {
            require(!scene.document.moveControlPoint(invalid, {5, 10}), "Invalid handle reference accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }

    void nonFiniteTargetsPreserveDocument()
    {
        CurveTestScene scene;
        const Document before = scene.document;
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            require(!scene.document.moveControlPoint(scene.control(which), x ? Vec2{value, 5} : Vec2{5, value}),
                "Non-finite target accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }

    void handleTargetMayAliasAnotherStoredPoint()
    {
        CurveTestScene scene;
        const Document before = scene.document;
        const auto& curve = std::get<CubicBezier>(scene.document.findEdge(scene.first.edges[0])->geometry);
        const Vec2 expected = curve.control2;
        require(scene.document.moveControlPoint(scene.control(), curve.control2), "Aliased handle target rejected");
        requireSameCurveDocument(scene.document, withMovedCurveControl(before, scene.control(), expected));
    }

    void polygonTranslationMovesAllItsVerticesAndHandles()
    {
        CurveTestScene scene;
        const Document before = scene.document;
        require(scene.document.movePolygon(scene.first.id, {7, -11}), "Curved polygon translation rejected");
        requireSameCurveDocument(scene.document, before, scene.first.id, {7, -11});
        require(scene.document.movePolygon(scene.first.id, {-2, 5}) && scene.document.movePolygon(scene.first.id, {}),
            "Repeated or zero translation rejected");
        requireSameCurveDocument(scene.document, before, scene.first.id, {5, -6});
    }

    void translationMayAliasVertexOrHandle()
    {
        for (const bool handle : {false, true})
        {
            CurveTestScene scene;
            const Document before = scene.document;
            const auto& curve = std::get<CubicBezier>(scene.document.findEdge(scene.first.edges[0])->geometry);
            const Vec2& delta = handle ? curve.control1 : scene.document.findVertex(scene.first.vertices[1])->position;
            const Vec2 copiedDelta = delta;
            require(scene.document.movePolygon(scene.first.id, delta), "Aliased translation rejected");
            requireSameCurveDocument(scene.document, before, scene.first.id, copiedDelta);
        }
    }

    void missingEntityRejectsWholeCurvedTranslation()
    {
        for (const bool edge : {false, true})
        {
            CurveTestScene scene;
            auto* polygon = const_cast<Polygon*>(scene.document.findPolygon(scene.first.id));
            if (edge) polygon->edges.back() = EdgeId{999999};
            else polygon->vertices.back() = VertexId{999999};
            const Document before = scene.document;
            require(!scene.document.movePolygon(scene.first.id, {7, 8}), "Incomplete curved polygon translation accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }

    void lateHandleOverflowRejectsBeforeAnyChanges()
    {
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        for (const bool x : {false, true})
        for (const double sign : {-1.0, 1.0})
        {
            CurveTestScene scene;
            const double value = sign * std::numeric_limits<double>::max() * 0.75;
            auto& curve = std::get<CubicBezier>(const_cast<Edge*>(scene.document.findEdge(scene.first.edges[2]))->geometry);
            (which == BezierControlPoint::First ? curve.control1 : curve.control2) = x ? Vec2{value, 0} : Vec2{0, value};
            const Document before = scene.document;
            const Vec2 delta = x ? Vec2{value, 0} : Vec2{0, value};
            require(!scene.document.movePolygon(scene.first.id, delta), "Overflow in a later handle was accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }

    void malformedHandleRejectsWholeTranslation()
    {
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            CurveTestScene scene;
            auto& curve = std::get<CubicBezier>(const_cast<Edge*>(scene.document.findEdge(scene.first.edges[2]))->geometry);
            auto& control = which == BezierControlPoint::First ? curve.control1 : curve.control2;
            (x ? control.x : control.y) = value;
            const Document before = scene.document;
            require(!scene.document.movePolygon(scene.first.id, {1, 2}), "Malformed handle was moved");
            requireSameCurveDocument(scene.document, before);
        }
    }

    void invalidPolygonOrDeltaPreservesCurves()
    {
        CurveTestScene scene;
        const Document before = scene.document;
        for (const PolygonId missing : {PolygonId{}, PolygonId{999999}})
            require(!scene.document.movePolygon(missing, {1, 2}), "Missing polygon translation accepted");
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            require(!scene.document.movePolygon(scene.first.id, x ? Vec2{value, 0} : Vec2{0, value}), "Invalid translation accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> controlPointMovementTests()
    {
        static const std::array tests{
            TestCase{"move only requested handle to absolute position", movesOnlyRequestedHandleToAbsolutePosition},
            TestCase{"invalid handle references and enums preserve document", invalidReferencesAndEnumsPreserveDocument},
            TestCase{"non-finite handle targets preserve document", nonFiniteTargetsPreserveDocument},
            TestCase{"handle target may alias another stored point", handleTargetMayAliasAnotherStoredPoint},
            TestCase{"polygon translation moves all vertices and handles", polygonTranslationMovesAllItsVerticesAndHandles},
            TestCase{"translation may alias vertex or handle", translationMayAliasVertexOrHandle},
            TestCase{"missing entity rejects whole curved translation", missingEntityRejectsWholeCurvedTranslation},
            TestCase{"late handle overflow rejects before any changes", lateHandleOverflowRejectsBeforeAnyChanges},
            TestCase{"malformed handle rejects whole translation", malformedHandleRejectsWholeTranslation},
            TestCase{"invalid polygon or delta preserves curves", invalidPolygonOrDeltaPreservesCurves}
        };
        return tests;
    }
}
