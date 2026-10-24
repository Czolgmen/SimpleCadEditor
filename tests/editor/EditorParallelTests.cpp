#include <editor/EditorSession.h>

#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad;
    using editor::EditorSession;
    using cad::tests::require;

    struct Scene
    {
        Document document;
        Polygon first;
        Polygon second;

        Scene()
        {
            const std::array a{Vec2{0, 0}, Vec2{20, 0}, Vec2{20, 20}, Vec2{0, 20}};
            const std::array b{Vec2{100, 100}, Vec2{120, 110}, Vec2{120, 130}, Vec2{100, 130}};
            first = *document.findPolygon(document.addPolygon(a));
            second = *document.findPolygon(document.addPolygon(b));
        }
    };

    void requirePosition(const Document& document, VertexId vertex, Vec2 expected)
    {
        const auto* actual = document.findVertex(vertex);
        require(actual && actual->position.x == expected.x && actual->position.y == expected.y, "Pinned or unrelated point moved");
    }

    void requireStorage(const Document& actual, const Document& expected)
    {
        const auto a = actual.getFixedLengthConstraints();
        const auto b = expected.getFixedLengthConstraints();
        require(a.size() == b.size(), "Length relation count changed");
        for (std::size_t i = 0; i < a.size(); ++i)
            require(a[i].edge == b[i].edge && a[i].length == b[i].length, "Length relation changed");
        const auto p = actual.getParallelConstraints();
        const auto q = expected.getParallelConstraints();
        require(p.size() == q.size(), "Parallel relation count changed");
        for (std::size_t i = 0; i < p.size(); ++i)
            require(p[i].first == q[i].first && p[i].second == q[i].second, "Parallel pair changed");
    }

    void requireDocument(const Document& actual, const Document& expected)
    {
        requireStorage(actual, expected);
        require(actual.getPolygons().size() == expected.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, before] : expected.getPolygons())
        {
            const auto* after = actual.findPolygon(id);
            require(after && after->id == before.id && after->vertices == before.vertices && after->edges == before.edges,
                "Topology changed unexpectedly");
            for (const VertexId vertex : before.vertices)
                requirePosition(actual, vertex, expected.findVertex(vertex)->position);
            for (const EdgeId edge : before.edges)
            {
                const auto* a = actual.findEdge(edge);
                const auto* b = expected.findEdge(edge);
                require(a && a->id == b->id && a->start == b->start && a->end == b->end, "Edge changed unexpectedly");
            }
        }
        require(actual.validate().empty(), "Invalid document after selection operation");
    }

    void requireSatisfied(const Document& document)
    {
        require(document.validate().empty(), "Invalid topology after adding parallelism");
        for (const auto& pair : document.getParallelConstraints())
        {
            const auto error = constraint::parallelError(document, pair);
            require(error && std::abs(*error) <= 1e-6, "Parallel pair was not solved");
        }
        for (const auto& length : document.getFixedLengthConstraints())
        {
            const auto error = constraint::fixedLengthError(document, length);
            require(error && std::abs(*error) <= 1e-6, "Adding parallelism broke a length");
        }
    }

    void beginRequiresNonzeroEdgeSelection()
    {
        EditorSession empty;
        require(!empty.beginParallelSelection() && !empty.completeParallelSelection(EdgeId{1}), "Empty session enabled parallelism");
        Scene scene;
        EditorSession session{scene.document};
        for (const Vec2 position : {Vec2{0, 0}, Vec2{10, 10}, Vec2{50, 50}})
        {
            session.selectAt(position, 0);
            require(!session.beginParallelSelection() && !session.pendingParallelEdge(), "Non-edge selection enabled parallelism");
        }
        require(session.selectAt({10, 0}, 0), "Edge selection failed");
        // Simulate geometry becoming degenerate after selection, without exposing mutable API.
        auto* end = const_cast<Vertex*>(session.document().findVertex(scene.first.vertices[1]));
        const Vec2 original = end->position;
        for (const Vec2 invalid : {Vec2{0, 0}, Vec2{std::numeric_limits<double>::quiet_NaN(), 0}})
        {
            end->position = invalid;
            require(!session.beginParallelSelection() && !session.pendingParallelEdge(), "Degenerate first edge accepted");
        }
        end->position = original;
        requireDocument(session.document(), scene.document);
    }

    void pendingSelectionStartsNoDragAndSurvivesPointerUp()
    {
        Scene scene;
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && session.pendingParallelEdge() == scene.first.edges[0], "First edge not remembered");
        require(!session.isDragging() && !session.pointerDown({0, 0}, 0) && !session.pointerMove({5, 10}),
            "Choosing a second edge started a drag");
        session.pointerUp();
        require(session.pendingParallelEdge() == scene.first.edges[0], "Mouse release cancelled choosing the second edge");
        requireDocument(session.document(), scene.document);
    }

    void cancellationPathsPreserveDocument()
    {
        for (int method = 0; method < 4; ++method)
        {
            Scene scene;
            EditorSession session{scene.document};
            session.selectAt({10, 0}, 0);
            require(session.beginParallelSelection(), "Could not begin selection");
            if (method == 0) session.cancelParallelSelection();
            if (method == 1) session.escape();
            if (method == 2) require(session.cancelInteraction(), "Focus/cancel-mode cancellation failed");
            if (method == 3) session.selectAt({110, 105}, 0);
            require(!session.pendingParallelEdge() && !session.isDragging(), "Cancellation left an active selection mode");
            require(!session.completeParallelSelection(scene.second.edges[0]), "Cancelled mode accepted a late completion");
            require(!session.cancelInteraction(), "Idle cancellation reported an active interaction");
            requireDocument(session.document(), scene.document);
        }
    }

    void addsAndSolvesAcrossPolygonsWithExactFirstPin()
    {
        Scene scene;
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && session.completeParallelSelection(scene.second.edges[0]), "Cross-polygon relation failed");
        require(!session.pendingParallelEdge() && !session.isDragging(), "Completed pair left an interaction active");
        require(session.selection().edge == scene.second.edges[0] && !session.selection().vertex && !session.selection().polygon,
            "Completion did not select only the second edge");
        require(session.document().getParallelConstraints().size() == 1, "Completed pair was not stored");
        requirePosition(session.document(), scene.first.vertices[0], {0, 0});
        for (const auto vertex : {scene.first.vertices[2], scene.first.vertices[3], scene.second.vertices[2], scene.second.vertices[3]})
            requirePosition(session.document(), vertex, scene.document.findVertex(vertex)->position);
        requireSatisfied(session.document());
    }

    void alignedAndOppositeEdgesDoNotMove()
    {
        for (const bool opposite : {false, true})
        {
            Scene scene;
            if (!opposite)
                require(scene.document.moveVertex(scene.second.vertices[1], {120, 100}), "Aligned fixture failed");
            EditorSession session{scene.document};
            session.selectAt({10, 0}, 0);
            const EdgeId second = opposite ? scene.first.edges[2] : scene.second.edges[0];
            require(session.beginParallelSelection() && session.completeParallelSelection(second), "Aligned pair rejected");
            Document expected = scene.document;
            require(expected.addParallel(scene.first.edges[0], second), "Expected pair failed");
            requireDocument(session.document(), expected);
        }
    }

    void addedParallelismPreservesAllLengthTargets()
    {
        Scene scene;
        for (const EdgeId edge : {scene.first.edges[0], scene.first.edges[1], scene.second.edges[0]})
            require(scene.document.setFixedLength(edge, 20), "Length fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && session.completeParallelSelection(scene.second.edges[0]), "Mixed pair addition failed");
        requireSatisfied(session.document());
        requirePosition(session.document(), scene.first.vertices[0], {0, 0});
        const auto lengths = session.document().getFixedLengthConstraints();
        require(lengths.size() == 3, "Adding parallelism changed length storage");
        for (const auto& length : lengths) require(length.length == 20, "Adding parallelism replaced a length target");
    }

    void invalidSecondEdgePreservesPendingStateAndCanRetry()
    {
        Scene scene;
        // The second polygon's right edge is unusable, but the first polygon's top remains a valid retry.
        require(scene.document.moveVertex(scene.second.vertices[2], {120, 110}), "Degenerate fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection(), "Could not begin selection");
        for (const EdgeId invalid : {EdgeId{}, EdgeId{999999}, scene.first.edges[0], scene.second.edges[1]})
        {
            require(!session.completeParallelSelection(invalid), "Invalid second edge accepted");
            require(session.pendingParallelEdge() == scene.first.edges[0] && session.selection().edge == scene.first.edges[0],
                "Rejected pair lost the source or selection");
            requireDocument(session.document(), scene.document);
        }
        require(session.completeParallelSelection(scene.first.edges[2]), "Could not retry a valid second edge");
        requireSatisfied(session.document());
    }

    void duplicatePairInEitherOrderDoesNotChangeDocument()
    {
        for (const bool reverse : {false, true})
        {
            Scene scene;
            require(scene.document.addParallel(scene.first.edges[0], scene.first.edges[2]), "Duplicate fixture failed");
            EditorSession session{scene.document};
            session.selectAt(reverse ? Vec2{10, 20} : Vec2{10, 0}, 0);
            const EdgeId first = scene.first.edges[reverse ? 2 : 0];
            const EdgeId second = scene.first.edges[reverse ? 0 : 2];
            require(session.beginParallelSelection() && !session.completeParallelSelection(second), "Duplicate pair accepted");
            require(session.pendingParallelEdge() == first && session.selection().edge == first, "Duplicate rejection lost source");
            requireDocument(session.document(), scene.document);
        }
    }

    void conflictingParallelismRollsBackSolverAndMetadata()
    {
        Document document;
        const std::array positions{Vec2{0, 0}, Vec2{20, 0}, Vec2{0, 20}};
        const auto polygon = *document.findPolygon(document.addPolygon(positions));
        const std::array targets{20.0, std::sqrt(800.0), 20.0};
        for (std::size_t i = 0; i < targets.size(); ++i)
            require(document.setFixedLength(polygon.edges[i], targets[i]), "Rigid triangle fixture failed");
        EditorSession session{document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && !session.completeParallelSelection(polygon.edges[2]), "Impossible parallel pair accepted");
        requireDocument(session.document(), document);
        require(session.pendingParallelEdge() == polygon.edges[0] && session.selection().edge == polygon.edges[0],
            "Solver rejection changed pending selection");
    }

    void topologyAndOtherSuccessfulCommandsCancelPendingSource()
    {
        for (const bool split : {false, true})
        {
            Scene scene;
            EditorSession session{scene.document};
            session.selectAt({10, 0}, 0);
            require(session.beginParallelSelection(), "Could not begin selection");
            require(split ? session.splitSelectedEdge() : session.setSelectedEdgeLength(25), "Editing command failed");
            require(!session.pendingParallelEdge(), "Editing command left a stale pending source");
            const Document committed = session.document();
            require(!session.completeParallelSelection(scene.second.edges[0]), "Stale source allowed late completion");
            requireDocument(session.document(), committed);
        }
    }

    void invalidSelectionInputKeepsPendingSource()
    {
        Scene scene;
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection(), "Could not begin selection");
        require(!session.selectAt({std::numeric_limits<double>::quiet_NaN(), 0}) && !session.selectAt({0, 0}, -1),
            "Invalid input accepted");
        require(session.pendingParallelEdge() == scene.first.edges[0], "Invalid input cancelled the pending source");
        requireDocument(session.document(), scene.document);
    }

    void removesOnlyChosenPairFromEitherArgumentPosition()
    {
        for (const bool reverse : {false, true})
        for (std::size_t removed = 0; removed < 3; ++removed)
        {
            Scene scene;
            const EdgeId selected = scene.first.edges[0];
            const std::array others{scene.first.edges[2], scene.second.edges[0], scene.second.edges[2]};
            for (const EdgeId other : others)
                require(reverse ? scene.document.addParallel(other, selected) : scene.document.addParallel(selected, other),
                    "Removal pair fixture failed");
            require(scene.document.addParallel(scene.first.edges[1], scene.second.edges[1]), "Unrelated pair fixture failed");
            require(scene.document.setFixedLength(selected, 20), "Removal length fixture failed");
            Document expected = scene.document;
            require(expected.removeParallel(selected, others[removed]), "Expected pair removal failed");
            EditorSession session{scene.document};
            require(session.selectAt({10, 0}, 0) && session.selection().edge == selected, "Removal selection failed");
            require(session.removeSelectedEdgeParallel(others[removed]), "Chosen pair was not removed");
            requireDocument(session.document(), expected);
            require(session.selection().edge == selected && !session.selection().vertex && !session.selection().polygon,
                "Pair removal changed edge selection");
            require(!session.removeSelectedEdgeParallel(others[removed]), "Repeated pair removal succeeded");
            requireDocument(session.document(), expected);
        }
    }

    void rejectedRemovalPreservesSelectionGestureAndPendingSource()
    {
        Scene scene;
        require(scene.document.addParallel(scene.first.edges[0], scene.first.edges[2]) &&
            scene.document.addParallel(scene.second.edges[0], scene.second.edges[2]), "Removal guard fixture failed");
        EditorSession session{scene.document};
        require(!session.removeSelectedEdgeParallel(scene.first.edges[2]), "Empty selection removed a pair");
        require(session.pointerDown({0, 0}, 0), "Guard vertex gesture failed");
        require(!session.removeSelectedEdgeParallel(scene.first.edges[2]) && session.isDragging(), "Rejected removal ended a drag");
        require(session.selection().vertex == scene.first.vertices[0], "Rejected removal changed vertex selection");
        require(session.cancelInteraction(), "Rejected removal lost drag snapshot");
        session.selectAt({10, 10}, 0);
        require(!session.removeSelectedEdgeParallel(scene.first.edges[2]) && session.selection().polygon == scene.first.id,
            "Polygon selection removed a pair or changed selection");
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection(), "Could not prepare pending removal guard");
        for (const EdgeId invalid : {EdgeId{}, EdgeId{999999}, scene.first.edges[0], scene.second.edges[0]})
        {
            require(!session.removeSelectedEdgeParallel(invalid), "Unrelated or missing pair was removed");
            require(session.pendingParallelEdge() == scene.first.edges[0] && session.selection().edge == scene.first.edges[0],
                "Rejected removal changed pending source or selection");
            requireDocument(session.document(), scene.document);
        }
    }

    void removingPairDoesNotValidateOrSolveDegenerateGeometry()
    {
        Scene scene;
        require(scene.document.addParallel(scene.first.edges[0], scene.first.edges[2]) &&
            scene.document.setFixedLength(scene.first.edges[0], 20), "Degenerate removal fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        // A stored relation must remain removable if its direction later degenerates.
        auto* end = const_cast<Vertex*>(session.document().findVertex(scene.first.vertices[1]));
        end->position = {0, 0};
        Document expected = session.document();
        require(expected.removeParallel(scene.first.edges[0], scene.first.edges[2]), "Expected degenerate removal failed");
        require(session.removeSelectedEdgeParallel(scene.first.edges[2]), "Degenerate geometry prevented removing a pair");
        requireDocument(session.document(), expected);
        require(session.selectedFixedLength() == 20, "Removing parallelism removed the length target");
    }

    void successfulRemovalEndsPendingSelectionAndCannotBeRolledBack()
    {
        Scene scene;
        require(scene.document.addParallel(scene.first.edges[0], scene.first.edges[2]), "Pending removal fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && session.removeSelectedEdgeParallel(scene.first.edges[2]),
            "Removal during second-edge selection failed");
        require(!session.pendingParallelEdge() && !session.isDragging(), "Removal left an active interaction");
        Document expected = scene.document;
        require(expected.removeParallel(scene.first.edges[0], scene.first.edges[2]), "Expected pending removal failed");
        require(!session.completeParallelSelection(scene.second.edges[0]) && !session.cancelInteraction(),
            "Removal retained a stale source or cancellable snapshot");
        session.pointerUp();
        session.escape();
        requireDocument(session.document(), expected);
    }

    void removingOnePairFreesItsDragAndKeepsOtherRelations()
    {
        Scene scene;
        require(scene.document.moveVertex(scene.second.vertices[1], {120, 100}), "Aligned removal fixture failed");
        require(scene.document.addParallel(scene.first.edges[0], scene.second.edges[0]) &&
            scene.document.addParallel(scene.first.edges[1], scene.second.edges[1]) &&
            scene.document.setFixedLength(scene.first.edges[1], 20), "Retained relation fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.removeSelectedEdgeParallel(scene.second.edges[0]), "Cross-polygon removal failed");
        const Document afterRemoval = session.document();
        require(session.pointerDown({0, 0}, 0) && session.pointerMove({5, 10}), "Freed vertex could not move");
        requirePosition(session.document(), scene.first.vertices[0], {5, 10});
        for (const auto& [id, polygon] : scene.document.getPolygons())
        {
            (void)id;
            for (const VertexId vertex : polygon.vertices)
                if (vertex != scene.first.vertices[0])
                    requirePosition(session.document(), vertex, scene.document.findVertex(vertex)->position);
        }
        requireSatisfied(session.document());
        requireStorage(session.document(), afterRemoval);
        const auto freedError = constraint::parallelError(session.document(), {scene.first.edges[0], scene.second.edges[0]});
        require(freedError && std::abs(*freedError) > 0.1, "Removed parallel pair still restricted the drag");
        require(session.cancelInteraction(), "Freed drag could not be cancelled");
        requireDocument(session.document(), afterRemoval);
    }

    void removedPairCanBeAddedAgain()
    {
        Scene scene;
        require(scene.document.addParallel(scene.first.edges[0], scene.first.edges[2]), "Re-add fixture failed");
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.removeSelectedEdgeParallel(scene.first.edges[2]) && session.beginParallelSelection() &&
            session.completeParallelSelection(scene.first.edges[2]), "Removed pair could not be added again");
        requireDocument(session.document(), scene.document);
        require(session.selection().edge == scene.first.edges[2], "Re-added pair lost completion selection");
    }

    void completionIsCommittedAndLaterDraggingUsesNewPair()
    {
        Scene scene;
        EditorSession session{scene.document};
        session.selectAt({10, 0}, 0);
        require(session.beginParallelSelection() && session.completeParallelSelection(scene.second.edges[0]), "Pair completion failed");
        const Document committed = session.document();
        session.pointerUp();
        require(!session.cancelInteraction(), "Completed pair retained a cancellable snapshot");
        session.escape();
        requireDocument(session.document(), committed);
        require(session.pointerDown({0, 0}, 0) && session.pointerMove({5, 10}), "Drag after pair creation failed");
        requireSatisfied(session.document());
        require(session.cancelInteraction(), "Later drag could not be cancelled");
        requireDocument(session.document(), committed);
    }
}

namespace cad::tests
{
    std::span<const TestCase> editorParallelTests()
    {
        static const std::array tests{
            TestCase{"begin requires a nonzero edge selection", beginRequiresNonzeroEdgeSelection},
            TestCase{"pending selection starts no drag and survives mouse release", pendingSelectionStartsNoDragAndSurvivesPointerUp},
            TestCase{"cancel, Escape, focus loss and reselection preserve document", cancellationPathsPreserveDocument},
            TestCase{"add and solve across polygons with exact first pin", addsAndSolvesAcrossPolygonsWithExactFirstPin},
            TestCase{"aligned and opposite edges remain in place", alignedAndOppositeEdgesDoNotMove},
            TestCase{"adding parallelism preserves all length targets", addedParallelismPreservesAllLengthTargets},
            TestCase{"invalid second edge preserves pending state and permits retry", invalidSecondEdgePreservesPendingStateAndCanRetry},
            TestCase{"duplicates in either order preserve document", duplicatePairInEitherOrderDoesNotChangeDocument},
            TestCase{"conflicting pair rolls back solver and relation storage", conflictingParallelismRollsBackSolverAndMetadata},
            TestCase{"successful editing commands cancel pending source", topologyAndOtherSuccessfulCommandsCancelPendingSource},
            TestCase{"invalid selection input preserves pending source", invalidSelectionInputKeepsPendingSource},
            TestCase{"completion commits and later dragging uses the new pair", completionIsCommittedAndLaterDraggingUsesNewPair},
            TestCase{"remove only chosen pair from either argument position", removesOnlyChosenPairFromEitherArgumentPosition},
            TestCase{"rejected removal preserves gesture and pending source", rejectedRemovalPreservesSelectionGestureAndPendingSource},
            TestCase{"removal works without solving degenerate geometry", removingPairDoesNotValidateOrSolveDegenerateGeometry},
            TestCase{"removal ends pending selection and commits", successfulRemovalEndsPendingSelectionAndCannotBeRolledBack},
            TestCase{"removing one pair frees its drag and keeps other relations", removingOnePairFreesItsDragAndKeepsOtherRelations},
            TestCase{"removed pair can be added again", removedPairCanBeAddedAgain}
        };
        return tests;
    }
}
