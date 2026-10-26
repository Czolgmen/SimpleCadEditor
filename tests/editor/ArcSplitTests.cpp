#include "../CurveTestSupport.h"

#include <cad_core/constraints/ContinuityConstraint.h>
#include <cad_core/model/ArcContinuity.h>
#include <cad_core/model/EdgeSampling.h>
#include <editor/EditorSession.h>

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;
    using cad::editor::EditorSession;

    void requireSameState(const Document& actual, const Document& expected)
    {
        requireSameCurveDocument(actual, expected);
        Document a = actual, b = expected;
        const std::array probe{Vec2{200, 200}, Vec2{220, 200}, Vec2{200, 220}};
        require(a.addPolygon(probe) == b.addPolygon(probe), "Arc split consumed polygon IDs");
        requireSameCurveDocument(a, b);
    }

    void requireRelations(const Document& document)
    {
        require(document.validate().empty(), "Arc split left invalid model geometry");
        for (const auto& length : document.getFixedLengthConstraints())
        {
            const auto error = constraint::fixedLengthError(document, length);
            require(error && std::abs(*error) <= 1e-6, "Arc split changed constrained length");
        }
        for (const auto& parallel : document.getParallelConstraints())
        {
            const auto error = constraint::parallelError(document, parallel);
            require(error && std::abs(*error) <= 1e-6, "Arc split changed constrained parallelism");
        }
        const auto joins = collectContinuityJoins(document);
        require(joins.has_value(), "Arc split lost continuity topology");
        for (const auto& join : *joins)
        {
            const auto errors = constraint::continuityErrors(document, join);
            require(errors.has_value(), "Arc split left undefined tangents");
            for (const double error : *errors)
                require(std::abs(error) <= 1e-6, "Arc split left unsatisfied continuity");
        }
    }

    void selectArc(EditorSession& session, EdgeId id)
    {
        const auto* edge = session.document().findEdge(id);
        const auto points = sampleEdge(edge->geometry, session.document().findVertex(edge->start)->position,
            session.document().findVertex(edge->end)->position);
        require(session.selectAt(points[points.size() / 2], 0) && session.selection().edge == id &&
            session.canSplitSelectedEdge(), "Arc split command unavailable or wrong selection");
    }

    void requireCommitted(const EditorSession& session, VertexId id, Vec2 position)
    {
        const auto& selection = session.selection();
        require(selection.vertex == id && !selection.edge && !selection.polygon && !selection.controlPoint,
            "Arc split did not exclusively select the new vertex");
        require(!session.isDragging() && !session.pendingParallelEdge(), "Arc split retained an interaction");
        const auto* vertex = session.document().findVertex(id);
        require(vertex && vertex->position == position, "Arc split moved the pinned midpoint");
        require(vertex->continuity == ContinuitySetting{Continuity::G0, ContinuitySource::Default},
            "Arc split did not assign G0/Default");
        requireRelations(session.document());
    }

    void g0SplitWorksAtEveryPositionAndBothSides()
    {
        for (std::size_t index = 0; index < 4; ++index)
        for (const double bulge : {-1.0, 1.0})
        {
            CurveTestScene f;
            const auto id = f.first.edges[index];
            require(f.document.convertEdgeToArc(id) && f.document.setArcBulge(id, bulge) &&
                f.document.convertEdgeToArc(f.other.edges[2]), "Arc split fixture failed");
            const auto* edge = f.document.findEdge(id);
            require(f.document.setContinuity(edge->start, Continuity::G0), "G0 metadata fixture failed");
            const Vec2 start = f.document.findVertex(edge->start)->position;
            const Vec2 end = f.document.findVertex(edge->end)->position;
            const Vec2 chord = end - start;
            const Vec2 geometricMidpoint = (start + end) * 0.5 + Vec2{chord.y, -chord.x} * (bulge * 0.5);
            EditorSession session(f.document);
            selectArc(session, id);
            Document expected = f.document;
            const auto inserted = expected.splitEdge(id);
            require(inserted.has_value() && session.splitSelectedEdge(), "G0 arc session split failed");
            const Vec2 actualMidpoint = session.document().findVertex(*inserted)->position;
            require(std::hypot(actualMidpoint.x - geometricMidpoint.x, actualMidpoint.y - geometricMidpoint.y) < 1e-11,
                "Arc split used the chord midpoint");
            requireSameState(session.document(), expected);
            requireCommitted(session, *inserted, expected.findVertex(*inserted)->position);
            const auto& polygon = *session.document().findPolygon(f.first.id);
            require(std::holds_alternative<LineSegment>(session.document().findEdge(id)->geometry) &&
                std::holds_alternative<LineSegment>(session.document().findEdge(polygon.edges[index + 1])->geometry),
                "Arc split did not produce two segments");
            require(!session.cancelInteraction(), "Committed arc split remained cancellable");
        }
    }

    void g1SplitRepairsAdjacentBezierWithMidpointPinned()
    {
        for (const bool atStart : {false, true})
        {
            CurveTestScene f;
            const EdgeId id = f.first.edges[atStart ? 1 : 3];
            const VertexId shared = f.first.vertices[atStart ? 1 : 0];
            const ControlPointRef control{f.first.edges[0], atStart ? BezierControlPoint::Second : BezierControlPoint::First};
            require(f.document.convertEdgeToArc(id) && f.document.setContinuity(shared, Continuity::G1) &&
                updateArcBulges(f.document), "G1 arc split fixture failed");
            requireRelations(f.document);
            EditorSession session(f.document);
            selectArc(session, id);
            Document expected = f.document;
            const auto inserted = expected.splitEdge(id);
            require(inserted.has_value(), "Model G1 arc split failed");
            const auto joins = collectContinuityJoins(expected);
            require(joins && joins->size() == 1, "Raw G1 split lost its active join");
            const auto errors = constraint::continuityErrors(expected, joins->front());
            require(errors && std::abs(errors->front()) > 1e-3, "G1 fixture needs no correction after splitting");
            const Vec2 midpoint = expected.findVertex(*inserted)->position;
            require(session.splitSelectedEdge(), "G1 arc split did not converge");
            requireCommitted(session, *inserted, midpoint);
            // These are the only free points in the remaining G1 equation.
            const_cast<Vertex*>(expected.findVertex(shared))->position = session.document().findVertex(shared)->position;
            auto& curve = std::get<CubicBezier>(const_cast<Edge*>(expected.findEdge(control.edge))->geometry);
            (atStart ? curve.control2 : curve.control1) = curveHandlePosition(session.document(), control);
            requireSameState(session.document(), expected);
        }
    }

    void arcPairSplitRefitsSurvivingArcWithoutMovingVertices()
    {
        for (const std::size_t dividedIndex : {0u, 1u})
        {
            CurveTestScene f;
            require(f.document.convertEdgeToArc(f.first.edges[0]) && f.document.convertEdgeToArc(f.first.edges[1]) &&
                f.document.setContinuity(f.first.vertices[1], Continuity::G1) &&
                f.document.setArcBulge(f.first.edges[0], 0.5) && updateArcBulges(f.document), "Arc pair split fixture failed");
            const EdgeId divided = f.first.edges[dividedIndex], surviving = f.first.edges[1 - dividedIndex];
            const double oldBulge = std::get<CircularArc>(f.document.findEdge(surviving)->geometry).bulge;
            EditorSession session(f.document);
            selectArc(session, divided);
            Document expected = f.document;
            const auto inserted = expected.splitEdge(divided);
            require(inserted.has_value() && session.splitSelectedEdge(), "Arc pair split failed");
            requireCommitted(session, *inserted, expected.findVertex(*inserted)->position);
            const double newBulge = std::get<CircularArc>(session.document().findEdge(surviving)->geometry).bulge;
            require(std::abs(newBulge - oldBulge) > 1e-3, "Surviving arc was not refitted");
            require(expected.setArcBulge(surviving, newBulge), "Expected arc update failed");
            requireSameState(session.document(), expected);
        }
    }

    void invalidArcSplitPreservesSelectionAndCounters()
    {
        for (const double invalid : {0.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        {
            CurveTestScene f;
            const EdgeId id = f.first.edges[3];
            require(f.document.convertEdgeToArc(id), "Invalid arc session fixture failed");
            EditorSession session(f.document);
            selectArc(session, id);
            // Malformed data is introduced after selecting a valid, sampleable arc.
            std::get<CircularArc>(const_cast<Edge*>(session.document().findEdge(id))->geometry).bulge = invalid;
            const Document before = session.document();
            require(!session.splitSelectedEdge(), "Malformed arc split accepted");
            requireSameState(session.document(), before);
            require(session.selection().edge == id && session.canSplitSelectedEdge() && !session.isDragging(),
                "Rejected arc split changed interaction state");
        }
    }

    void solverFailureRollsBackArcTopologyAndIds()
    {
        CurveTestScene f;
        const EdgeId id = f.first.edges[3];
        require(f.document.convertEdgeToArc(id), "Failure arc split fixture failed");
        const std::array triangle{Vec2{200, 0}, Vec2{210, 0}, Vec2{205, 10}};
        const auto polygon = *f.document.findPolygon(f.document.addPolygon(triangle));
        for (std::size_t i = 0; i < 3; ++i)
            require(f.document.setFixedLength(polygon.edges[i], i == 2 ? 30 : 10), "Conflict arc split fixture failed");
        EditorSession session(f.document);
        selectArc(session, id);
        Document raw = f.document;
        require(raw.splitEdge(id).has_value(), "Conflict fixture failed before the solver");
        require(!session.splitSelectedEdge(), "Unsolvable arc split committed");
        requireSameState(session.document(), f.document);
        require(session.selection().edge == id && session.canSplitSelectedEdge() && !session.isDragging(),
            "Solver failure lost arc selection");
    }

    void wrongSelectionKeepsPreviewAndCancellation()
    {
        CurveTestScene f;
        require(f.document.convertEdgeToArc(f.first.edges[3]), "Gesture arc fixture failed");
        EditorSession session(f.document);
        const auto control = f.control();
        const Vec2 position = curveHandlePosition(f.document, control);
        require(session.pointerDown(position, 0) && session.pointerMove(position + Vec2{2, -2}), "Handle preview failed");
        const Document preview = session.document();
        require(!session.canSplitSelectedEdge() && !session.splitSelectedEdge() && session.isDragging() &&
            session.selection().controlPoint == control, "Wrong split command destroyed active preview");
        requireSameState(session.document(), preview);
        require(session.cancelInteraction(), "Gesture snapshot lost after rejected split");
        requireSameState(session.document(), f.document);
    }

    void insertedG0VertexCanBeDraggedAndCancelled()
    {
        CurveTestScene f;
        const EdgeId id = f.first.edges[0];
        require(f.document.convertEdgeToArc(id), "Inserted vertex fixture failed");
        EditorSession session(f.document);
        selectArc(session, id);
        require(session.splitSelectedEdge() && session.selection().vertex.has_value(), "Inserted vertex split failed");
        const VertexId inserted = *session.selection().vertex;
        const Document committed = session.document();
        const Vec2 point = committed.findVertex(inserted)->position;
        require(session.pointerDown(point, 0) && session.pointerMove(point + Vec2{3, -2}), "Inserted G0 vertex drag failed");
        requireSameCurvePoint(session.document().findVertex(inserted)->position, point + Vec2{3, -2});
        session.escape();
        require(!session.isDragging() && session.selection().vertex == inserted, "Escape lost inserted vertex selection");
        requireSameState(session.document(), committed);
    }
}

namespace cad::tests
{
    std::span<const TestCase> arcSplitTests()
    {
        static const std::array tests{
            TestCase{"G0 arc splits across cycle and both sides", g0SplitWorksAtEveryPositionAndBothSides},
            TestCase{"G1 arc split repairs adjacent Bezier with midpoint pinned", g1SplitRepairsAdjacentBezierWithMidpointPinned},
            TestCase{"arc pair split refits surviving arc without moving vertices", arcPairSplitRefitsSurvivingArcWithoutMovingVertices},
            TestCase{"invalid arc split preserves selection and counters", invalidArcSplitPreservesSelectionAndCounters},
            TestCase{"solver failure rolls back arc topology and IDs", solverFailureRollsBackArcTopologyAndIds},
            TestCase{"wrong selection keeps preview and cancellation", wrongSelectionKeepsPreviewAndCancellation},
            TestCase{"inserted G0 vertex can be dragged and cancelled", insertedG0VertexCanBeDraggedAndCancelled}
        };
        return tests;
    }
}
