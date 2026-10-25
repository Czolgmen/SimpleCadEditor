#include <editor/EditorSession.h>
#include <editor/HitTester.h>
#include <cad_core/solver/ConstraintSolver.h>


#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace
{
    bool isFinite(const cad::Vec2& position)
    {
        return std::isfinite(position.x) && std::isfinite(position.y);
    }
}

namespace cad::editor
{
    EditorSession::EditorSession(Document document)
        : document_(std::move(document))
    {
    }

    const Document& EditorSession::document() const noexcept
    {
        return document_;
    }

    const Selection& EditorSession::selection() const noexcept
    {
        return selection_;
    }

    bool EditorSession::isDragging() const noexcept
    {
        return drag_.has_value();
    }

    bool EditorSession::selectAt(const Vec2& worldPosition, double tolerance)
    {
        if (!isFinite(worldPosition) || !std::isfinite(tolerance) || tolerance < 0.0)
            return false;

        pointerUp();
        cancelParallelSelection();
        selection_ = {};

        selection_.controlPoint = hitTestControlPoint(document_, worldPosition, tolerance);

        if (selection_.controlPoint)
            return true;

        selection_.vertex = hitTestVertex(document_, worldPosition, tolerance);
        if (!selection_.vertex)
        {
            selection_.edge = hitTestEdge(document_, worldPosition, tolerance);
            if (!selection_.edge)
                selection_.polygon = hitTestPolygon(document_, worldPosition);
        }
        return true;
    }

    bool EditorSession::pointerDown(const Vec2& worldPosition, double tolerance)
    {
        if (pendingParallelEdge_)
            return false;
        if (!selectAt(worldPosition, tolerance))
            return false;

        if (selection_.vertex)
        {
            const Vertex* vertex = document_.findVertex(*selection_.vertex);
            const Vec2 offset = vertex->position - worldPosition;
            if (!isFinite(offset))
                return false;
            drag_.emplace(DragTransaction{document_, DraggingVertex{vertex->id, offset}});
        }
        else if (selection_.polygon)
        {
            drag_.emplace(DragTransaction{document_, DraggingPolygon{*selection_.polygon, worldPosition}});
        }
        else if (selection_.controlPoint)
        {
            const auto control = *selection_.controlPoint;
            const auto* edge = document_.findEdge(control.edge);
            if (!edge)
                return false;

            const auto* bezier = std::get_if<CubicBezier>(&edge->geometry);
            if (!bezier)
                return false;

            const Vec2 position = 
                control.point == BezierControlPoint::First
                    ? bezier->control1
                    : bezier->control2;

            const Vec2 offset = position - worldPosition;
            if (!isFinite(offset))
                return false;

            drag_.emplace(DragTransaction{
                document_,
                DraggingControlPoint{
                    control, offset
                }
            });
        }
        return isDragging();
    }

    bool EditorSession::pointerMove(const Vec2& worldPosition)
    {
        if (!drag_ || !isFinite(worldPosition))
            return false;
    
        Document candidate =
            std::holds_alternative<DraggingPolygon>(drag_->state)
                ? drag_->initialDocument
                : document_;
    
        bool moved = false;
    
        if (const auto* vertex =
            std::get_if<DraggingVertex>(&drag_->state))
        {
            moved = candidate.moveVertex(
                vertex->vertex,
                worldPosition + vertex->grabOffset);
            
            if (moved)
            {
                const auto result = solver::solveConstraints(
                    candidate,
                    candidate.getFixedLengthConstraints(),
                    candidate.getParallelConstraints(),
                    vertex->vertex);
                
                moved = result.status == solver::SolveStatus::Converged;
            }
        }
        else if (const auto* control =
            std::get_if<DraggingControlPoint>(&drag_->state))
        {
            moved = candidate.moveControlPoint(
                control->control,
                worldPosition + control->grabOffset);

            if (moved) 
            {
                const auto result = solver::solveConstraints(
                    candidate,
                    candidate.getFixedLengthConstraints(),
                    candidate.getParallelConstraints(),
                    control->control
                );

                moved = result.status == solver::SolveStatus::Converged;
            }
        }
        else if (const auto* polygon =
            std::get_if<DraggingPolygon>(&drag_->state))
        {
            moved = candidate.movePolygon(
                polygon->polygon,
                worldPosition - polygon->pointerOrigin);
        }
    
        if (!moved)
            return false;
    
        document_ = std::move(candidate);
        return true;
    }

    void EditorSession::pointerUp() noexcept
    {
        drag_.reset();
    }

    bool EditorSession::cancelInteraction()
    {
        const bool wasPicking = pendingParallelEdge_.has_value();
        cancelParallelSelection();
        if (!drag_)
            return wasPicking;
        document_ = std::move(drag_->initialDocument);
        drag_.reset();
        return true;
    }

    void EditorSession::escape()
    {
        if (!cancelInteraction())
            selection_ = {};
    }

    bool EditorSession::canSplitSelectedEdge() const
    {
        if (!selection_.edge)
            return false;
        const auto* edge = document_.findEdge(*selection_.edge);
        return edge &&
            (std::holds_alternative<LineSegment>(edge->geometry) ||
             std::holds_alternative<CubicBezier>(edge->geometry));
    }

    bool EditorSession::canDeleteSelectedVertex() const
    {
        if (!selection_.vertex || !document_.findVertex(*selection_.vertex))
            return false;
        for (const auto& [id, polygon] : document_.getPolygons())
        {
            if (std::find(polygon.vertices.begin(), polygon.vertices.end(), *selection_.vertex) !=
                polygon.vertices.end())
                return polygon.vertices.size() > 3;
        }
        return false;
    }

    bool EditorSession::splitSelectedEdge()
    {
        if (!canSplitSelectedEdge())
            return false;

        Document candidate = document_;
        const auto vertex = candidate.splitEdge(*selection_.edge);
        if (!vertex)
            return false;

        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            *vertex
        );

        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        selection_ = {};
        selection_.vertex = vertex;
        return true;
    }

    bool EditorSession::deleteSelectedVertex()
    {
        if (!canDeleteSelectedVertex())
            return false;

        const VertexId removed = *selection_.vertex;
        std::optional<VertexId> fixedVertex;
        for (const auto& [id, polygon] : document_.getPolygons())
        {
            const auto position = std::find(polygon.vertices.begin(), polygon.vertices.end(), removed);
            if (position == polygon.vertices.end())
                continue;

            // Save a surviving neighbour before the model removes the selected vertex.
            fixedVertex = position == polygon.vertices.begin() ? polygon.vertices.back() : *(position - 1);
            break;
        }
        if (!fixedVertex)
            return false;

        Document candidate = document_;
        if (!candidate.deleteVertex(removed))
            return false;
        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            *fixedVertex);
        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        selection_ = {};
        return true;
    }

    std::optional<double> EditorSession::selectedEdgeLength() const
    {
        if (!selection_.edge)
            return std::nullopt;
        const auto* edge = document_.findEdge(*selection_.edge);
        if (!edge || !std::holds_alternative<LineSegment>(edge->geometry))
            return std::nullopt;
        const auto* start = document_.findVertex(edge->start);
        const auto* end = document_.findVertex(edge->end);
        if (!start || !end)
            return std::nullopt;
        const double length = std::hypot(end->position.x - start->position.x, end->position.y - start->position.y);
        return std::isfinite(length) ? std::optional<double>{length} : std::nullopt;
    }

    std::optional<double> EditorSession::selectedFixedLength() const
    {
        if (!selection_.edge)
            return std::nullopt;
        for (const auto& relation : document_.getFixedLengthConstraints())
            if (relation.edge == *selection_.edge)
                return relation.length;
        return std::nullopt;
    }

    bool EditorSession::setSelectedEdgeLength(double length)
    {
        if (!selection_.edge)
            return false;
        const auto* edge = document_.findEdge(*selection_.edge);
        if (!edge)
            return false;
        const VertexId fixedVertex = edge->start;
        Document candidate = document_;
        if (!candidate.setFixedLength(*selection_.edge, length))
            return false;
        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            fixedVertex);
        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        return true;
    }

    bool EditorSession::removeSelectedEdgeLength()
    {
        if (!selection_.edge || !document_.removeFixedLength(*selection_.edge))
            return false;
        cancelParallelSelection();
        pointerUp();
        return true;
    }

    std::optional<EdgeId> EditorSession::pendingParallelEdge() const noexcept
    {
        return pendingParallelEdge_;
    }

    bool EditorSession::beginParallelSelection()
    {
        const auto length = selectedEdgeLength();
        if (!length || *length <= 0.0)
            return false;
        pointerUp();
        pendingParallelEdge_ = selection_.edge;
        return true;
    }

    bool EditorSession::completeParallelSelection(EdgeId second)
    {
        if (!pendingParallelEdge_)
            return false;
        const EdgeId first = *pendingParallelEdge_;
        const auto* edge = document_.findEdge(first);
        if (!edge)
            return false;
        const VertexId pinned = edge->start;
        Document candidate = document_;
        if (!candidate.addParallel(first, second))
            return false;
        const auto result = solver::solveConstraints(candidate, candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(), pinned);
        if (result.status != solver::SolveStatus::Converged)
            return false;
        document_ = std::move(candidate);
        cancelParallelSelection();
        selection_ = {};
        selection_.edge = second;
        return true;
    }

    void EditorSession::cancelParallelSelection() noexcept
    {
        pendingParallelEdge_.reset();
    }

    bool EditorSession::removeSelectedEdgeParallel(EdgeId other)
    {
        if (!selection_.edge || !document_.removeParallel(*selection_.edge, other))
            return false;
        cancelParallelSelection();
        pointerUp();
        return true;
    }

    bool EditorSession::convertSelectedEdgeToBezier()
    {
        if(!selection_.edge)
            return false;

        const auto* edge = document_.findEdge(*selection_.edge);

        if(!edge)
            return false;

        VertexId fixedVertex = edge->start;

        Document candidate = document_;

        if(!candidate.convertEdgeToBezier(*selection_.edge))
            return false;

        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            fixedVertex
        );

        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        return true;
    }

    bool EditorSession::convertSelectedEdgeToSegment()
    {
        if (!selection_.edge)
            return false;

        const auto* edge = document_.findEdge(*selection_.edge);
        if (!edge)
            return false;
        const VertexId fixedVertex = edge->start;

        Document candidate = document_;
        if (!candidate.convertEdgeToSegment(*selection_.edge))
            return false;

        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            fixedVertex);
        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        return true;
    }


    bool EditorSession::setSelectedVertexContinuity(Continuity continuity)
    {
        if (!selectedVertexContinuity())
            return false;

        const VertexId fixedVertex = *selection_.vertex;

        Document candidate = document_;
        if (!candidate.setContinuity(fixedVertex, continuity))
            return false;

        const auto result = solver::solveConstraints(
            candidate,
            candidate.getFixedLengthConstraints(),
            candidate.getParallelConstraints(),
            fixedVertex);
        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        cancelParallelSelection();
        pointerUp();
        return true;
    }
    std::optional<Continuity> EditorSession::selectedVertexContinuity() const
    {
        if (!selection_.vertex)
            return std::nullopt;
        
        const Vertex* vertex = document_.findVertex(*selection_.vertex);
        if (!vertex)
            return std::nullopt;

        bool hasAdjacentBezier = false;
        for (const auto& [polygonId, polygon] : document_.getPolygons())
        {
            for (const EdgeId id : polygon.edges)
            {
                const auto* edge = document_.findEdge(id);
                if (edge && (edge->start == *selection_.vertex || edge->end == *selection_.vertex) &&
                    std::holds_alternative<CubicBezier>(edge->geometry))
                {
                    hasAdjacentBezier = true;
                    break;
                }
            }
            if (hasAdjacentBezier)
                break;
        }
        if (!hasAdjacentBezier)
            return std::nullopt;

        return vertex->continuity.value;
    }
}
