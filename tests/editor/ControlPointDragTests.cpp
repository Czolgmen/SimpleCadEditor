#include <editor/EditorSession.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include "../CurveTestSupport.h"

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;
    using editor::EditorSession;

    void requireControlSelection(const EditorSession& session, ControlPointRef control)
    {
        const auto& selection = session.selection();
        require(selection.controlPoint == control && !selection.vertex && !selection.edge && !selection.polygon,
            "Control selection is missing or not exclusive");
    }

    void g1HandleDragPinsCursorAndCanCancelOrCommit()
    {
        CurveTestScene scene;
        require(scene.document.moveControlPoint(scene.control(), {0, -10}) &&
            scene.document.setContinuity(scene.first.vertices[0], Continuity::G1), "G1 handle fixture failed");
        EditorSession session{scene.document};
        const Vec2 target{10, -20};
        for (const bool cancel : {true, false})
        {
            require(session.pointerDown({2, -9}, 3) && session.pointerMove({12, -19}), "G1 handle drag failed");
            requireControlSelection(session, scene.control());
            requireSameCurvePoint(curveHandlePosition(session.document(), scene.control()), target);
            const auto joins = collectContinuityJoins(session.document());
            require(joins && joins->size() == 1, "Wrong active G1 join count");
            const auto errors = constraint::continuityErrors(session.document(), joins->front());
            require(errors && errors->size() == 1 && std::abs(errors->front()) <= 1e-6,
                "Handle drag accepted unsatisfied G1");
            if (cancel)
            {
                require(session.cancelInteraction(), "G1 drag did not retain a snapshot");
                requireSameCurveDocument(session.document(), scene.document);
            }
            else
            {
                const Document committed = session.document();
                session.pointerUp();
                require(!session.cancelInteraction(), "Committed G1 drag retained a snapshot");
                requireSameCurveDocument(session.document(), committed);
            }
        }
    }

    Document adjacentControlDragScene(bool incomingLine)
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{30, 0}, Vec2{30, 30}, Vec2{0, 30}};
        const Polygon polygon = *document.findPolygon(document.addPolygon(positions));
        if (!incomingLine)
        {
            require(document.convertEdgeToBezier(polygon.edges[0]) &&
                document.moveControlPoint({polygon.edges[0], BezierControlPoint::Second}, {30, -10}),
                "Incoming curve fixture failed");
        }
        require(document.convertEdgeToBezier(polygon.edges[1]) &&
            document.moveControlPoint({polygon.edges[1], BezierControlPoint::First},
                incomingLine ? Vec2{40, 0} : Vec2{30, 10}), "Outgoing curve fixture failed");
        for (const auto vertex : polygon.vertices)
            require(document.setContinuity(vertex, Continuity::G0), "Continuity reset failed");
        require(document.setContinuity(polygon.vertices[1], Continuity::C1), "C1 fixture failed");
        if (incomingLine)
            require(document.setFixedLength(polygon.edges[0], 30) &&
                document.addParallel(polygon.edges[0], polygon.edges[2]), "Mixed drag fixture failed");
        return document;
    }

    void requireDragConstraints(const Document& document)
    {
        const auto joins = collectContinuityJoins(document);
        require(joins && joins->size() == 1, "Wrong controlled joint count");
        const auto errors = constraint::continuityErrors(document, joins->front());
        require(errors.has_value(), "Unusable tangents after handle drag");
        for (const double error : *errors)
            require(std::isfinite(error) && std::abs(error) <= 1e-6, "Unsatisfied drag continuity");
        for (const auto& relation : document.getFixedLengthConstraints())
        {
            const auto error = constraint::fixedLengthError(document, relation);
            require(error && std::abs(*error) <= 1e-6, "Unsatisfied drag length");
        }
        for (const auto& relation : document.getParallelConstraints())
        {
            const auto error = constraint::parallelError(document, relation);
            require(error && std::abs(*error) <= 1e-6, "Unsatisfied drag parallel relation");
        }
    }

    void bezierJointHandleDragKeepsVerticesAndCanCancelOrCommit()
    {
        const Document initial = adjacentControlDragScene(false);
        const Polygon polygon = initial.getPolygons().begin()->second;
        const ControlPointRef control{polygon.edges[1], BezierControlPoint::First};
        EditorSession session{initial};
        for (const bool cancel : {true, false})
        {
            require(session.pointerDown({31, 12}, 3) && session.pointerMove({46, 17}), "C1 handle drag failed");
            requireControlSelection(session, control);
            requireSameCurvePoint(curveHandlePosition(session.document(), control), {45, 15});
            for (const auto vertex : polygon.vertices)
                requireSameCurvePoint(session.document().findVertex(vertex)->position, initial.findVertex(vertex)->position);
            requireDragConstraints(session.document());
            if (cancel)
            {
                require(session.cancelInteraction(), "Preferred handle drag lost snapshot");
                requireSameCurveDocument(session.document(), initial);
            }
            else
            {
                const Document committed = session.document();
                session.pointerUp();
                require(!session.cancelInteraction(), "Preferred handle drag retained a committed snapshot");
                requireSameCurveDocument(session.document(), committed);
            }
        }
    }

    void requiredEndpointMovementKeepsCursorAndCanCancel()
    {
        const Document initial = adjacentControlDragScene(true);
        const Polygon polygon = initial.getPolygons().begin()->second;
        const ControlPointRef control{polygon.edges[1], BezierControlPoint::First};
        EditorSession session{initial};
        require(session.pointerDown({40, 0}, 0) && session.pointerMove({45, 0}), "Fallback handle drag failed");
        requireControlSelection(session, control);
        requireSameCurvePoint(curveHandlePosition(session.document(), control), {45, 0});
        require(std::abs(session.document().findVertex(polygon.vertices[1])->position.x - 30) > 1,
            "Required endpoint movement did not occur");
        requireDragConstraints(session.document());
        require(session.isDragging() && session.cancelInteraction(), "Fallback drag lost its snapshot");
        requireSameCurveDocument(session.document(), initial);
    }

    void handleSelectionHasPriorityAndClearsOnBackground()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        {
            const auto control = scene.control(which);
            require(session.selectAt(curveHandlePosition(scene.document, control), 0), "Handle selection rejected");
            requireControlSelection(session, control);
        }
        for (const Vec2 position : {Vec2{0, 0}, Vec2{30, 15}, Vec2{15, 15}})
        {
            CurveTestScene overlap;
            require(overlap.document.moveControlPoint(overlap.control(), position), "Overlap fixture failed");
            EditorSession selection{overlap.document};
            selection.selectAt(position, 0);
            requireControlSelection(selection, overlap.control());
        }
        session.selectAt({500, 500}, 0);
        const auto& selection = session.selection();
        require(!selection.controlPoint && !selection.vertex && !selection.edge && !selection.polygon,
            "Background did not clear the handle selection");
    }

    void bothHandlesDragWithGrabOffsetAndCommit()
    {
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        {
            CurveTestScene scene;
            EditorSession session{scene.document};
            const auto control = scene.control(which);
            const Vec2 pointer = curveHandlePosition(scene.document, control) + Vec2{1, -2};
            require(session.pointerDown(pointer, 3) && session.isDragging(), "Handle drag did not start");
            requireControlSelection(session, control);
            require(session.pointerMove({50, 60}), "Handle drag rejected a valid preview");
            const Document expected = withMovedCurveControl(scene.document, control, {49, 62});
            requireSameCurveDocument(session.document(), expected);
            session.pointerUp();
            require(!session.isDragging() && !session.cancelInteraction(), "Released handle gesture remained cancellable");
            requireSameCurveDocument(session.document(), expected);
        }
    }

    void successivePreviewsUseAbsoluteCursorPosition()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        const auto control = scene.control();
        require(session.pointerDown(curveHandlePosition(scene.document, control), 0), "Preview gesture did not start");
        for (const Vec2 target : {Vec2{50, 60}, Vec2{51, 62}, Vec2{49, 58}})
        {
            require(session.pointerMove(target), "Successive preview failed");
            requireSameCurveDocument(session.document(), withMovedCurveControl(scene.document, control, target));
        }
    }

    void cancelAndEscapeRestoreHandlesAndRelations()
    {
        for (const bool escape : {false, true})
        for (const auto which : {BezierControlPoint::First, BezierControlPoint::Second})
        {
            CurveTestScene scene;
            EditorSession session{scene.document};
            const auto control = scene.control(which);
            require(session.pointerDown(curveHandlePosition(scene.document, control), 0) &&
                session.pointerMove({50, 60}) && session.pointerMove({55, 65}), "Cancellation fixture gesture failed");
            if (escape) session.escape();
            else require(session.cancelInteraction(), "Handle cancellation failed");
            require(!session.isDragging(), "Cancelled handle drag remained active");
            requireSameCurveDocument(session.document(), scene.document);
            requireControlSelection(session, control);
        }
    }

    void rejectedInputPreservesLastPreviewAndCanResume()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        require(session.pointerDown({10, -10}, 0) && session.pointerMove({50, 60}), "Rejection fixture gesture failed");
        const Document accepted = session.document();
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            require(!session.pointerMove(x ? Vec2{value, 0} : Vec2{0, value}), "Invalid handle preview was accepted");
            require(session.isDragging(), "Rejected preview lost the gesture");
            requireControlSelection(session, scene.control());
            requireSameCurveDocument(session.document(), accepted);
        }
        require(session.pointerMove({51, 61}), "Handle could not resume after rejection");
        requireSameCurveDocument(session.document(), withMovedCurveControl(scene.document, scene.control(), {51, 61}));
    }

    void releaseAfterRejectionCommitsLastPreview()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        require(session.pointerDown({10, -10}, 0) && session.pointerMove({50, 60}), "Release fixture gesture failed");
        const Document accepted = session.document();
        require(!session.pointerMove({std::numeric_limits<double>::infinity(), 0}), "Invalid final preview accepted");
        session.pointerUp();
        require(!session.cancelInteraction(), "Release after rejection retained a snapshot");
        session.escape();
        requireSameCurveDocument(session.document(), accepted);
        require(!session.selection().controlPoint, "Idle Escape did not clear handle selection");
    }

    void polygonDragMovesHandlesByTotalGestureDelta()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        require(session.pointerDown({15, 15}, 0) && session.selection().polygon == scene.first.id, "Curved polygon drag did not start");
        require(session.pointerMove({22, 26}), "First polygon preview failed");
        requireSameCurveDocument(session.document(), scene.document, scene.first.id, {7, 11});
        require(session.pointerMove({17, 19}), "Second polygon preview failed");
        requireSameCurveDocument(session.document(), scene.document, scene.first.id, {2, 4});
        const Document accepted = session.document();
        session.pointerUp();
        require(!session.cancelInteraction(), "Released curved polygon drag remained cancellable");
        requireSameCurveDocument(session.document(), accepted);
    }

    void polygonCancellationRestoresEveryHandle()
    {
        for (const bool escape : {false, true})
        {
            CurveTestScene scene;
            EditorSession session{scene.document};
            require(session.pointerDown({15, 15}, 0) && session.pointerMove({25, 35}) && session.pointerMove({12, 9}),
                "Polygon cancellation fixture failed");
            if (escape) session.escape();
            else require(session.cancelInteraction(), "Polygon cancellation failed");
            require(!session.isDragging(), "Cancelled polygon drag remained active");
            requireSameCurveDocument(session.document(), scene.document);
        }
    }

    void laterGestureCancellationKeepsEarlierCommit()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        require(session.pointerDown({10, -10}, 0) && session.pointerMove({50, 60}), "First gesture failed");
        session.pointerUp();
        const Document committed = session.document();
        require(session.pointerDown({20, -10}, 0) && session.pointerMove({-5, -8}), "Second gesture failed");
        require(session.cancelInteraction(), "Second gesture cancellation failed");
        requireSameCurveDocument(session.document(), committed);
    }

    void linkedCurveHandleDragConvergesAndCancels()
    {
        LinkedCurveTestScene scene;
        const std::array controls{
            ControlPointRef{scene.left.edges[2], BezierControlPoint::First},
            ControlPointRef{scene.right.edges[2], BezierControlPoint::First},
            ControlPointRef{scene.right.edges[2], BezierControlPoint::Second}};
        for (const auto control : controls)
        {
            EditorSession session{scene.document};
            const Vec2 start = curveHandlePosition(scene.document, control);
            const Vec2 target = start + Vec2{40, 20};
            require(session.pointerDown(start, 0) && session.pointerMove(target), "Linked curve handle drag failed");
            requireControlSelection(session, control);
            requireSameCurvePoint(curveHandlePosition(session.document(), control), target);
            for (const auto& length : session.document().getFixedLengthConstraints())
            {
                const auto error = constraint::fixedLengthError(session.document(), length);
                require(error && std::abs(*error) <= 1e-6, "Linked drag violated length");
            }
            for (const auto& parallel : session.document().getParallelConstraints())
            {
                const auto error = constraint::parallelError(session.document(), parallel);
                require(error && std::abs(*error) <= 1e-6, "Linked drag violated parallelism");
            }
            const auto joins = collectContinuityJoins(session.document());
            require(joins.has_value(), "Linked drag invalidated continuity topology");
            for (const auto& join : *joins)
            {
                const auto errors = constraint::continuityErrors(session.document(), join);
                require(errors.has_value(), "Linked drag lost tangent");
                for (const double error : *errors)
                    require(std::abs(error) <= 1e-6, "Linked drag violated continuity");
            }
            require(session.cancelInteraction(), "Linked drag lost its initial snapshot");
            requireSameCurveDocument(session.document(), scene.document);
        }
    }

    void unrelatedCommandsPreserveActiveHandleGesture()
    {
        CurveTestScene scene;
        EditorSession session{scene.document};
        require(session.pointerDown({10, -10}, 0), "Command fixture gesture failed");
        require(!session.setSelectedEdgeLength(30) && !session.convertSelectedEdgeToBezier() &&
            !session.convertSelectedEdgeToSegment() && !session.splitSelectedEdge() && !session.deleteSelectedVertex(),
            "Handle selection enabled an unrelated command");
        require(session.isDragging(), "Rejected command ended the handle gesture");
        requireControlSelection(session, scene.control());
        require(session.pointerMove({50, 60}) && session.cancelInteraction(), "Rejected command destroyed the drag snapshot");
        requireSameCurveDocument(session.document(), scene.document);
    }
}

namespace cad::tests
{
    std::span<const TestCase> controlPointDragTests()
    {
        static const std::array tests{
            TestCase{"Bezier joint handle drag keeps vertices and cancels or commits", bezierJointHandleDragKeepsVerticesAndCanCancelOrCommit},
            TestCase{"required endpoint movement keeps cursor and can cancel", requiredEndpointMovementKeepsCursorAndCanCancel},
            TestCase{"G1 handle drag pins cursor and cancels or commits", g1HandleDragPinsCursorAndCanCancelOrCommit},
            TestCase{"exclusive handle selection has priority and clears", handleSelectionHasPriorityAndClearsOnBackground},
            TestCase{"both handles drag with offset and commit", bothHandlesDragWithGrabOffsetAndCommit},
            TestCase{"successive previews use absolute cursor position", successivePreviewsUseAbsoluteCursorPosition},
            TestCase{"cancel and Escape restore handles and relations", cancelAndEscapeRestoreHandlesAndRelations},
            TestCase{"rejected input preserves preview and permits resuming", rejectedInputPreservesLastPreviewAndCanResume},
            TestCase{"release after rejection commits last preview", releaseAfterRejectionCommitsLastPreview},
            TestCase{"polygon drag moves handles by total gesture delta", polygonDragMovesHandlesByTotalGestureDelta},
            TestCase{"polygon cancellation restores every handle", polygonCancellationRestoresEveryHandle},
            TestCase{"later cancellation keeps earlier committed gesture", laterGestureCancellationKeepsEarlierCommit},
            TestCase{"unrelated commands preserve active handle gesture", unrelatedCommandsPreserveActiveHandleGesture},
            TestCase{"linked curve handle drag converges and cancels", linkedCurveHandleDragConvergesAndCancels}
        };
        return tests;
    }
}
