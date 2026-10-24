#include <editor/EditorSession.h>

#include "../TestSupport.h"

#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
    using namespace cad;
    using cad::editor::EditorSession;
    using cad::tests::require;

    static_assert(std::is_same_v<decltype(std::declval<EditorSession&>().document()), const Document&>);
    static_assert(std::is_same_v<decltype(std::declval<EditorSession&>().selection()), const cad::editor::Selection&>);

    Document scene()
    {
        Document document;
        const std::array first{Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 20}, Vec2{0, 20}};
        const std::array second{Vec2{100, 100}, Vec2{120, 100}, Vec2{120, 120}, Vec2{100, 120}};
        document.addPolygon(first);
        document.addPolygon(second);
        return document;
    }

    struct Fixture
    {
        EditorSession session{scene()};
        PolygonId polygon = session.document().getPolygons().begin()->first;
        VertexId vertex = session.document().findPolygon(polygon)->vertices.front();
        EdgeId edge = session.document().findPolygon(polygon)->edges.front();
    };

    void requirePosition(const Document& document, VertexId id, const Vec2& expected)
    {
        const Vertex* vertex = document.findVertex(id);
        require(vertex && vertex->position.x == expected.x && vertex->position.y == expected.y,
            "Vertex position differs from the expected world position");
    }

    void requireDocument(const Document& actual, const Document& expected, PolygonId translated = {}, Vec2 delta = {})
    {
        require(actual.getPolygons().size() == expected.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : expected.getPolygons())
        {
            const Polygon* found = actual.findPolygon(id);
            require(found && found->id == id && found->vertices == polygon.vertices && found->edges == polygon.edges,
                "Document topology changed unexpectedly");
            for (const VertexId vertexId : polygon.vertices)
            {
                const Vec2 original = expected.findVertex(vertexId)->position;
                requirePosition(actual, vertexId, id == translated ? original + delta : original);
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const Edge* before = expected.findEdge(edgeId);
                const Edge* after = actual.findEdge(edgeId);
                require(after && after->id == before->id && after->start == before->start && after->end == before->end,
                    "Edge changed unexpectedly");
            }
        }
        require(actual.validate().empty(), "Session left an invalid document");
    }

    void requireEmptySelection(const EditorSession& session)
    {
        const auto& selection = session.selection();
        require(!selection.vertex && !selection.edge && !selection.polygon, "Selection is not empty");
    }

    void emptySessionHasNoGestureOrCommands()
    {
        EditorSession session;
        require(!session.pointerDown(Vec2{}), "Empty scene started a drag");
        require(!session.pointerMove(Vec2{5, 5}), "Idle session accepted a move");
        require(!session.isDragging() && !session.canSplitSelectedEdge() && !session.canDeleteSelectedVertex(),
            "Empty scene enabled an operation");
        require(!session.splitSelectedEdge() && !session.deleteSelectedVertex(), "Empty scene executed an operation");
        session.pointerUp();
        session.escape();
        requireEmptySelection(session);
    }

    void selectionPriorityAndClearing()
    {
        Fixture f;
        require(f.session.selectAt(Vec2{}), "Vertex selection failed");
        require(f.session.selection().vertex == f.vertex && !f.session.selection().edge && !f.session.selection().polygon,
            "Vertex must have priority over incident edges and interior");
        f.session.selectAt(Vec2{10, 0});
        require(f.session.selection().edge == f.edge && !f.session.selection().vertex && !f.session.selection().polygon,
            "Edge must have priority over polygon interior");
        f.session.selectAt(Vec2{10, 10});
        require(f.session.selection().polygon == f.polygon && !f.session.selection().vertex && !f.session.selection().edge,
            "Interior must select only the polygon");
        f.session.selectAt(Vec2{60, 60});
        requireEmptySelection(f.session);
    }

    void contextSelectionDoesNotStartDrag()
    {
        Fixture f;
        for (const Vec2 point : {Vec2{}, Vec2{10, 0}, Vec2{10, 10}})
        {
            require(f.session.selectAt(point), "Selection query failed");
            require(!f.session.isDragging(), "Context selection must not start dragging");
        }
    }

    void vertexDragPreservesGrabOffsetAndOtherVertices()
    {
        Fixture f;
        const Document before = f.session.document();
        require(f.session.pointerDown(Vec2{-2, 1}), "Vertex drag did not start");
        require(f.session.pointerMove(Vec2{8, 11}), "Vertex move failed");
        requirePosition(f.session.document(), f.vertex, Vec2{10, 10});
        for (const auto& [id, polygon] : before.getPolygons())
            for (const VertexId vertexId : polygon.vertices)
                if (vertexId != f.vertex)
                    requirePosition(f.session.document(), vertexId, before.findVertex(vertexId)->position);
        require(f.session.document().validate().empty(), "Vertex drag damaged topology");
    }

    void polygonDragUsesOriginalDocumentAndPointerOrigin()
    {
        Fixture f;
        const Document before = f.session.document();
        require(f.session.pointerDown(Vec2{10, 10}), "Polygon drag did not start");
        for (int step = 1; step <= 50; ++step)
            require(f.session.pointerMove(Vec2{10 + step * 0.1, 10 - step * 0.1}), "Intermediate move failed");
        require(f.session.pointerMove(Vec2{8.5, 7.25}), "Final polygon move failed");
        requireDocument(f.session.document(), before, f.polygon, Vec2{-1.5, -2.75});
    }

    void edgeSelectionDoesNotMoveGeometry()
    {
        Fixture f;
        const Document before = f.session.document();
        require(!f.session.pointerDown(Vec2{10, 0}), "An edge click started a drag");
        require(f.session.selection().edge == f.edge, "Edge click lost the selection");
        require(!f.session.pointerMove(Vec2{30, 40}), "An edge selection moved geometry");
        requireDocument(f.session.document(), before);
    }

    void pointerUpCommitsAndLateCancellationIsHarmless()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{5, 6});
        f.session.pointerUp();
        require(!f.session.isDragging() && f.session.selection().vertex == f.vertex,
            "Pointer up must stop the gesture and preserve selection");
        require(!f.session.cancelInteraction(), "Completed gesture was cancelled");
        require(!f.session.pointerMove(Vec2{10, 10}), "Completed gesture kept following the pointer");
        requirePosition(f.session.document(), f.vertex, Vec2{5, 6});
    }

    void cancelVertexDragRestoresInitialDocument()
    {
        Fixture f;
        const Document before = f.session.document();
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{8, 9});
        require(f.session.cancelInteraction(), "Active vertex gesture was not cancelled");
        requireDocument(f.session.document(), before);
        require(!f.session.isDragging() && f.session.selection().vertex == f.vertex, "Cancellation lost selection");
    }

    void cancelPolygonDragRestoresAllPositions()
    {
        Fixture f;
        const Document before = f.session.document();
        f.session.pointerDown(Vec2{10, 10});
        f.session.pointerMove(Vec2{20, 30});
        f.session.pointerMove(Vec2{-50, 80});
        require(f.session.cancelInteraction(), "Active polygon gesture was not cancelled");
        requireDocument(f.session.document(), before);
        require(f.session.selection().polygon == f.polygon, "Polygon cancellation lost selection");
    }

    void escapeCancelsThenClearsSelection()
    {
        Fixture f;
        const Document before = f.session.document();
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{15, 18});
        f.session.escape();
        requireDocument(f.session.document(), before);
        require(!f.session.isDragging() && f.session.selection().vertex == f.vertex, "First Escape must cancel the gesture");
        f.session.escape();
        requireEmptySelection(f.session);
    }

    void idleCancellationKeepsSelection()
    {
        Fixture f;
        f.session.selectAt(Vec2{10, 0});
        require(!f.session.cancelInteraction() && f.session.selection().edge == f.edge,
            "Capture loss after a committed interaction must not clear selection");
    }

    void invalidInputPreservesGestureAndLastValidPreview()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{5, 6});
        const Document preview = f.session.document();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double infinity = std::numeric_limits<double>::infinity();
        for (const Vec2 point : {Vec2{nan, 0}, Vec2{0, nan}, Vec2{infinity, 0}, Vec2{0, -infinity}})
        {
            require(!f.session.selectAt(point) && !f.session.pointerDown(point) && !f.session.pointerMove(point),
                "Non-finite pointer input was accepted");
        }
        for (const double tolerance : {-1.0, nan, infinity})
            require(!f.session.selectAt(Vec2{}, tolerance) && !f.session.pointerDown(Vec2{}, tolerance),
                "Invalid tolerance was accepted");
        requireDocument(f.session.document(), preview);
        require(f.session.isDragging() && f.session.selection().vertex == f.vertex,
            "Rejected input terminated the valid gesture");
    }

    void splitCommandSelectsNewVertex()
    {
        Fixture f;
        f.session.selectAt(Vec2{10, 0});
        require(f.session.canSplitSelectedEdge() && !f.session.canDeleteSelectedVertex(), "Wrong edge command availability");
        require(f.session.splitSelectedEdge(), "Session split command failed");
        require(f.session.selection().vertex && !f.session.selection().edge && !f.session.selection().polygon,
            "Split must select only the new vertex");
        requirePosition(f.session.document(), *f.session.selection().vertex, Vec2{10, 0});
        require(f.session.document().findPolygon(f.polygon)->vertices.size() == 5 && !f.session.isDragging(),
            "Split left an incorrect vertex count or active gesture");
        require(!f.session.canSplitSelectedEdge() && f.session.canDeleteSelectedVertex(), "Wrong midpoint command availability");
    }

    void deleteCommandClearsSelection()
    {
        Fixture f;
        f.session.selectAt(Vec2{});
        require(f.session.canDeleteSelectedVertex() && !f.session.canSplitSelectedEdge(), "Wrong vertex command availability");
        require(f.session.deleteSelectedVertex(), "Session deletion failed");
        require(!f.session.document().findVertex(f.vertex) &&
                f.session.document().findPolygon(f.polygon)->vertices.size() == 3,
            "Session deletion did not update the document");
        requireEmptySelection(f.session);
        require(!f.session.isDragging() && !f.session.canDeleteSelectedVertex(), "Deletion left an active operation");
    }

    void triangleDeletionAndMismatchedCommandsAreRejected()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{20, 0}, Vec2{0, 20}};
        document.addPolygon(positions);
        EditorSession session{document};
        session.selectAt(Vec2{});
        require(!session.canDeleteSelectedVertex() && !session.deleteSelectedVertex() && !session.splitSelectedEdge(),
            "Triangle or mismatched command was accepted");
        requireDocument(session.document(), document);
        session.selectAt(Vec2{10, 0});
        require(!session.deleteSelectedVertex(), "Edge selection allowed vertex deletion");
        requireDocument(session.document(), document);
    }

    void deletionDuringDragCommitsOperationAndEndsGesture()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{5, 6});
        require(f.session.deleteSelectedVertex(), "Deletion during dragging failed");
        require(!f.session.isDragging() && !f.session.cancelInteraction() && !f.session.document().findVertex(f.vertex),
            "Late cancellation restored a deleted vertex");
        requireEmptySelection(f.session);
        require(f.session.document().validate().empty(), "Deletion during dragging damaged the document");
    }

    void selectingNewElementCommitsExistingPreview()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{3, 4});
        f.session.selectAt(Vec2{100, 100});
        require(!f.session.isDragging() && !f.session.cancelInteraction(), "Context selection left a gesture active");
        requirePosition(f.session.document(), f.vertex, Vec2{3, 4});
        require(f.session.selection().vertex && f.session.selection().vertex != f.vertex, "New element was not selected");
    }

    void validMotionCanResumeAfterRejectedInput()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{5, 6});
        require(!f.session.pointerMove(Vec2{std::numeric_limits<double>::infinity(), 0}), "Invalid move was accepted");
        require(f.session.pointerMove(Vec2{7, 8}), "A rejected move prevented continued dragging");
        f.session.pointerUp();
        requirePosition(f.session.document(), f.vertex, Vec2{7, 8});
    }

    void eachGestureHasIndependentSnapshot()
    {
        Fixture f;
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{5, 6});
        f.session.pointerUp();
        const Document committed = f.session.document();
        require(f.session.pointerDown(Vec2{5, 6}), "Second gesture failed to start");
        f.session.pointerMove(Vec2{10, 11});
        f.session.cancelInteraction();
        requireDocument(f.session.document(), committed);
    }

    void failedCommandLeavesGestureCancellable()
    {
        Fixture f;
        const Document before = f.session.document();
        f.session.pointerDown(Vec2{});
        f.session.pointerMove(Vec2{3, 4});
        require(!f.session.splitSelectedEdge() && f.session.isDragging(), "Rejected command ended the gesture");
        require(f.session.cancelInteraction(), "Rejected command destroyed the snapshot");
        requireDocument(f.session.document(), before);
    }

    void cancelledMotionPreservesCommittedTopology()
    {
        Fixture f;
        f.session.selectAt(Vec2{10, 0});
        f.session.splitSelectedEdge();
        const Document split = f.session.document();
        require(f.session.pointerDown(Vec2{10, 0}), "Inserted vertex could not be dragged");
        f.session.pointerMove(Vec2{50, 60});
        f.session.cancelInteraction();
        requireDocument(f.session.document(), split);
    }
}

namespace cad::tests
{
    std::span<const TestCase> editorSessionTests()
    {
        static const std::array tests{
            TestCase{"empty session has no gesture or commands", emptySessionHasNoGestureOrCommands},
            TestCase{"selection priority, exclusivity and empty background", selectionPriorityAndClearing},
            TestCase{"context selection starts no drag", contextSelectionDoesNotStartDrag},
            TestCase{"vertex drag preserves grab offset and other vertices", vertexDragPreservesGrabOffsetAndOtherVertices},
            TestCase{"polygon drag uses original document and pointer origin", polygonDragUsesOriginalDocumentAndPointerOrigin},
            TestCase{"edge selection does not move geometry", edgeSelectionDoesNotMoveGeometry},
            TestCase{"pointer up commits and late cancellation is harmless", pointerUpCommitsAndLateCancellationIsHarmless},
            TestCase{"cancel vertex drag restores initial document", cancelVertexDragRestoresInitialDocument},
            TestCase{"cancel polygon drag restores all positions", cancelPolygonDragRestoresAllPositions},
            TestCase{"Escape cancels gesture then clears selection", escapeCancelsThenClearsSelection},
            TestCase{"idle cancellation preserves selection", idleCancellationKeepsSelection},
            TestCase{"invalid input preserves gesture and last valid preview", invalidInputPreservesGestureAndLastValidPreview},
            TestCase{"split command selects midpoint", splitCommandSelectsNewVertex},
            TestCase{"delete command clears selection", deleteCommandClearsSelection},
            TestCase{"triangle deletion and mismatched commands are rejected", triangleDeletionAndMismatchedCommandsAreRejected},
            TestCase{"deletion during dragging ends gesture without rollback", deletionDuringDragCommitsOperationAndEndsGesture},
            TestCase{"new selection commits existing preview", selectingNewElementCommitsExistingPreview},
            TestCase{"valid motion resumes after rejected input", validMotionCanResumeAfterRejectedInput},
            TestCase{"each gesture has an independent snapshot", eachGestureHasIndependentSnapshot},
            TestCase{"failed command preserves cancellable gesture", failedCommandLeavesGestureCancellable},
            TestCase{"cancelled motion preserves committed topology", cancelledMotionPreservesCommittedTopology}
        };
        return tests;
    }
}
