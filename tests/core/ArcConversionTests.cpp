#include "../CurveTestSupport.h"
#include <cad_core/model/EdgeSampling.h>
#include <editor/EditorSession.h>

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;
    using cad::editor::EditorSession;

    struct Fixture : CurveTestScene
    {
        Edge& edge(EdgeId id) { return *const_cast<Edge*>(document.findEdge(id)); }
        Vertex& vertex(VertexId id) { return *const_cast<Vertex*>(document.findVertex(id)); }
    };

    void requireSameState(const Document& actual, const Document& expected)
    {
        requireSameCurveDocument(actual, expected);
        Document a = actual, b = expected;
        const std::array points{Vec2{200, 200}, Vec2{220, 200}, Vec2{200, 220}};
        require(a.addPolygon(points) == b.addPolygon(points), "Arc conversion consumed polygon IDs");
        requireSameCurveDocument(a, b);
    }

    Document expectedArc(Document document, EdgeId id, double bulge = 1)
    {
        document.removeFixedLength(id);
        document.removeParallelsForEdge(id);
        const_cast<Edge*>(document.findEdge(id))->geometry = CircularArc{bulge};
        return document;
    }

    void requireRejected(Fixture& f, EdgeId id)
    {
        const Document before = f.document;
        require(!f.document.convertEdgeToArc(id), "Invalid arc conversion accepted");
        requireSameState(f.document, before);
    }

    void selectEdge(EditorSession& editor, EdgeId id)
    {
        const auto* edge = editor.document().findEdge(id);
        const auto points = sampleEdge(edge->geometry, editor.document().findVertex(edge->start)->position,
            editor.document().findVertex(edge->end)->position, 100);
        const Vec2 point = points.size() == 2 ? (points.front() + points.back()) * 0.5 : points[points.size() / 2];
        require(editor.selectAt(point, 0) && editor.selection().edge == id,
            "Conversion fixture edge not selected");
    }

    void everySegmentAndBezierConvertsWithoutChangingMetadata()
    {
        for (std::size_t index = 0; index < 8; ++index)
        for (const auto source : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            const EdgeId id = index < 4 ? f.first.edges[index] : f.other.edges[index - 4];
            f.vertex(f.edge(id).start).continuity.source = source;
            f.vertex(f.edge(id).end).continuity.source = source;
            const Document expected = expectedArc(f.document, id);
            require(f.document.convertEdgeToArc(id), "Valid semicircle conversion rejected");
            requireSameState(f.document, expected);
            require(f.document.validate().empty(), "Arc conversion invalidated geometry");
        }
    }

    void removesOnlyConvertedEdgesRelations()
    {
        Fixture f;
        const auto id = f.first.edges[3];
        require(f.document.setFixedLength(id, 30) && f.document.addParallel(id, f.first.edges[1]) &&
            f.document.addParallel(f.other.edges[1], id) && f.document.addParallel(id, f.other.edges[3]),
            "Conversion relation fixture failed");
        const Document expected = expectedArc(f.document, id);
        require(f.document.convertEdgeToArc(id), "Constrained segment conversion rejected");
        requireSameState(f.document, expected);
    }

    void oneG1AtEitherEndIsPreserved()
    {
        for (const bool end : {false, true})
        for (const auto source : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            const auto id = f.first.edges[0];
            f.vertex(end ? f.edge(id).end : f.edge(id).start).continuity = {Continuity::G1, source};
            const Document expected = expectedArc(f.document, id);
            require(f.document.convertEdgeToArc(id), "One-G1 arc conversion rejected");
            requireSameState(f.document, expected);
        }
    }

    void forbiddenContinuityRejectsBeforeRemovingRelations()
    {
        for (int kind = 0; kind < 4; ++kind)
        for (const bool end : {false, true})
        for (const auto source : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            const auto id = f.first.edges[3];
            require(f.document.setFixedLength(id, 30) && f.document.addParallel(id, f.first.edges[1]), "Reject fixture failed");
            if (kind == 0)
            {
                f.vertex(f.edge(id).start).continuity = {Continuity::G1, source};
                f.vertex(f.edge(id).end).continuity = {Continuity::G1, source};
            }
            else
                f.vertex(end ? f.edge(id).end : f.edge(id).start).continuity = {
                    kind == 1 ? Continuity::C1 : static_cast<Continuity>(kind == 2 ? -1 : 3), source};
            requireRejected(f, id);
        }
    }

    void missingIdsAndEndpointsRejectWithoutMutation()
    {
        Fixture f;
        for (const EdgeId id : {EdgeId{}, EdgeId{99999}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
            requireRejected(f, id);
        for (const bool end : {false, true})
        {
            Fixture broken;
            auto& edge = broken.edge(broken.first.edges[3]);
            (end ? edge.end : edge.start) = VertexId{99999};
            requireRejected(broken, edge.id);
        }
    }

    void nonfiniteAndCoincidentEndpointsReject()
    {
        for (const double bad : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (const bool end : {false, true})
        for (const bool y : {false, true})
        {
            Fixture f;
            const auto id = f.first.edges[3];
            auto& position = f.vertex(end ? f.edge(id).end : f.edge(id).start).position;
            (y ? position.y : position.x) = bad;
            requireRejected(f, id);
        }
        Fixture f;
        const auto id = f.first.edges[3];
        f.vertex(f.edge(id).end).position = f.vertex(f.edge(id).start).position;
        requireRejected(f, id);
    }

    void unrepresentableCircleRejects()
    {
        Fixture f;
        const auto id = f.first.edges[3];
        const double maximum = std::numeric_limits<double>::max();
        f.vertex(f.edge(id).start).position = {-maximum, -maximum};
        f.vertex(f.edge(id).end).position = {maximum, maximum};
        requireRejected(f, id);
    }

    void repeatedConversionKeepsExistingSignedBulge()
    {
        for (const double bulge : {-2.0, -1.0, -0.5, 0.5, 1.0, 2.0})
        {
            Fixture f;
            const auto id = f.first.edges[0];
            f.edge(id).geometry = CircularArc{bulge};
            f.vertex(f.edge(id).end).continuity.value = Continuity::G1;
            const Document before = f.document;
            require(f.document.convertEdgeToArc(id) && f.document.convertEdgeToArc(id), "Repeated arc conversion rejected");
            requireSameState(f.document, before);
        }
    }

    void invalidExistingArcIsNotReportedAsSuccess()
    {
        for (const double bulge : {0.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), 0.5})
        {
            Fixture f;
            const auto id = f.first.edges[0];
            f.edge(id).geometry = CircularArc{bulge}; // G0/G0 also rejects a non-semicircle.
            requireRejected(f, id);
        }
    }

    void sessionConvertsBackWithoutRestoringRelations()
    {
        Fixture f;
        const auto id = f.first.edges[3];
        require(f.document.setFixedLength(id, 30) && f.document.addParallel(id, f.first.edges[1]), "Session fixture failed");
        EditorSession editor(f.document);
        selectEdge(editor, id);
        require(editor.convertSelectedEdgeToArc(), "Session arc conversion failed");
        requireSameState(editor.document(), expectedArc(f.document, id));
        require(!editor.selectedEdgeLength() && !editor.selectedFixedLength() && !editor.beginParallelSelection(),
            "Arc exposed segment relations");
        require(editor.convertSelectedEdgeToSegment(), "Arc to segment failed");
        Document expected = f.document;
        expected.removeFixedLength(id);
        expected.removeParallelsForEdge(id);
        requireSameState(editor.document(), expected);
        require(editor.selectedEdgeLength().has_value() && editor.selection().edge == id, "Restored segment lost selection");
    }

    void sessionSolvesG1WithStartPinned()
    {
        Fixture f;
        const auto id = f.first.edges[0];
        f.vertex(f.first.vertices[2]).position = {48, 24}; // Neighbor length stays 30, direction (18,24).
        f.document.removeParallelsForEdge(f.first.edges[1]);
        require(f.document.setContinuity(f.first.vertices[1], Continuity::G1), "Session G1 fixture failed");
        EditorSession editor(f.document);
        selectEdge(editor, id);
        require(editor.convertSelectedEdgeToArc(), "Session G1 conversion failed");
        const double bulge = std::get<CircularArc>(editor.document().findEdge(id)->geometry).bulge;
        require(std::abs(bulge - 0.5) <= 1e-12, "Session did not fit arc G1");
        requireSameState(editor.document(), expectedArc(f.document, id, bulge));
    }

    void solverRejectionRestoresRelationsAndPendingSelection()
    {
        Fixture f;
        const auto id = f.first.edges[3];
        std::get<CubicBezier>(f.edge(f.first.edges[0]).geometry).control1 = {0, -10};
        require(f.document.setContinuity(f.first.vertices[0], Continuity::G1) && f.document.setFixedLength(id, 30),
            "Unconstructable arc fixture failed");
        EditorSession editor(f.document);
        selectEdge(editor, id);
        require(editor.beginParallelSelection(), "Pending selection fixture failed");
        const Document before = editor.document();
        require(!editor.convertSelectedEdgeToArc(), "Parallel-tangent arc conversion accepted");
        requireSameState(editor.document(), before);
        require(editor.selection().edge == id && editor.pendingParallelEdge() == id,
            "Failed conversion lost selection or pending relation");
    }

    void wrongSelectionPreservesActiveGesture()
    {
        Fixture f;
        EditorSession editor(f.document);
        require(!editor.convertSelectedEdgeToArc(), "Empty selection converted");
        require(editor.pointerDown({10, -10}, 0) && editor.isDragging(), "Control gesture fixture failed");
        const Document before = editor.document();
        require(!editor.convertSelectedEdgeToArc() && editor.isDragging(), "Rejected conversion ended gesture");
        requireSameState(editor.document(), before);
        require(editor.cancelInteraction(), "Gesture could not be cancelled");
    }

    void sessionCanSwitchArcAndBezierWithExplicitG0()
    {
        Fixture f;
        const auto id = f.first.edges[3];
        require(f.document.setContinuity(f.edge(id).start, Continuity::G0) &&
            f.document.setContinuity(f.edge(id).end, Continuity::G0), "Explicit G0 fixture failed");
        EditorSession editor(f.document);
        selectEdge(editor, id);
        require(editor.convertSelectedEdgeToArc() && editor.convertSelectedEdgeToBezier(), "Arc to Bezier failed");
        require(std::holds_alternative<CubicBezier>(editor.document().findEdge(id)->geometry), "Expected Bezier");
        require(editor.convertSelectedEdgeToArc(), "Bezier to arc failed");
        requireSameState(editor.document(), expectedArc(f.document, id));
    }
}

namespace cad::tests
{
    std::span<const TestCase> arcConversionTests()
    {
        static const std::array tests{
            TestCase{"all segments and Beziers convert without metadata changes", everySegmentAndBezierConvertsWithoutChangingMetadata},
            TestCase{"only converted edge relations removed", removesOnlyConvertedEdgesRelations},
            TestCase{"single G1 at either end preserved", oneG1AtEitherEndIsPreserved},
            TestCase{"forbidden continuity rejects before removing relations", forbiddenContinuityRejectsBeforeRemovingRelations},
            TestCase{"missing IDs and endpoints reject without mutation", missingIdsAndEndpointsRejectWithoutMutation},
            TestCase{"nonfinite and coincident endpoints reject", nonfiniteAndCoincidentEndpointsReject},
            TestCase{"unrepresentable circle rejects", unrepresentableCircleRejects},
            TestCase{"repeated conversion keeps signed bulge", repeatedConversionKeepsExistingSignedBulge},
            TestCase{"invalid existing arc is not reported as success", invalidExistingArcIsNotReportedAsSuccess},
            TestCase{"session converts back without restoring relations", sessionConvertsBackWithoutRestoringRelations},
            TestCase{"session solves G1 with start pinned", sessionSolvesG1WithStartPinned},
            TestCase{"solver rejection restores relations and pending selection", solverRejectionRestoresRelationsAndPendingSelection},
            TestCase{"wrong selection preserves active gesture", wrongSelectionPreservesActiveGesture},
            TestCase{"session switches arc and Bezier with explicit G0", sessionCanSwitchArcAndBezierWithExplicitG0}
        };
        return tests;
    }
}
