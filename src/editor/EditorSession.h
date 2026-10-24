#pragma once

#include <cad_core/model/Document.h>
#include <editor/DragState.h>
#include <editor/Selection.h>

#include <optional>

namespace cad::editor
{
    class EditorSession
    {
    public:
        explicit EditorSession(Document document = {});

        const Document& document() const noexcept;
        const Selection& selection() const noexcept;
        bool isDragging() const noexcept;

        // Positions and tolerance are in world units. Selecting commits an existing drag.
        bool selectAt(const Vec2& worldPosition, double tolerance = 6.0);
        // Returns whether a vertex or polygon drag was started.
        bool pointerDown(const Vec2& worldPosition, double tolerance = 6.0);
        bool pointerMove(const Vec2& worldPosition);
        void pointerUp() noexcept;
        bool cancelInteraction();
        void escape();

        bool canSplitSelectedEdge() const;
        bool canDeleteSelectedVertex() const;
        bool splitSelectedEdge();
        bool deleteSelectedVertex();
        std::optional<double> selectedEdgeLength() const;
        std::optional<double> selectedFixedLength() const;
        // Sets/updates the relation on a candidate, solving with the edge's start pinned.
        bool setSelectedEdgeLength(double length);
        bool removeSelectedEdgeLength();

        std::optional<EdgeId> pendingParallelEdge() const noexcept;
        bool beginParallelSelection();
        // Failure keeps the document, selection and pending first edge unchanged.
        bool completeParallelSelection(EdgeId second);
        void cancelParallelSelection() noexcept;

    private:
        struct DragTransaction
        {
            Document initialDocument;
            DragState state;
        };

        Document document_;
        Selection selection_;
        std::optional<DragTransaction> drag_;
        std::optional<EdgeId> pendingParallelEdge_;
    };
}
