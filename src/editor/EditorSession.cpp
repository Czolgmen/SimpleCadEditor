#include <editor/EditorSession.h>
#include <editor/HitTester.h>
#include <cad_core/solver/ConstraintSolver.h>


#include <algorithm>
#include <cmath>
#include <utility>

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
        selection_ = {};
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
        return isDragging();
    }

    bool EditorSession::pointerMove(const Vec2& worldPosition)
    {
        if (!drag_ || !isFinite(worldPosition))
            return false;

        // Vertex solving continues from the last accepted preview; polygon translation
        // remains absolute relative to the initial snapshot.
        Document candidate = std::holds_alternative<DraggingVertex>(drag_->state)
            ? document_ : drag_->initialDocument;
        bool moved = false;
        if (const auto* vertex = std::get_if<DraggingVertex>(&drag_->state))
        {
            moved = candidate.moveVertex(vertex->vertex, worldPosition + vertex->grabOffset);
            if (moved)
            {
                const auto result = solver::solveFixedLengths(
                    candidate,
                    candidate.getFixedLengthConstraints(),
                    vertex->vertex);

                moved = result.status == solver::SolveStatus::Converged;
            }
        }
        else
        {
            const auto& polygon = std::get<DraggingPolygon>(drag_->state);
            moved = candidate.movePolygon(polygon.polygon, worldPosition - polygon.pointerOrigin);
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
        if (!drag_)
            return false;
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
        return selection_.edge && document_.findEdge(*selection_.edge);
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
        const auto vertex = document_.splitEdge(*selection_.edge);
        if (!vertex)
            return false;
        pointerUp();
        selection_ = {};
        selection_.vertex = vertex;
        return true;
    }

    bool EditorSession::deleteSelectedVertex()
    {
        if (!canDeleteSelectedVertex() || !document_.deleteVertex(*selection_.vertex))
            return false;
        pointerUp();
        selection_ = {};
        return true;
    }

    std::optional<double> EditorSession::selectedEdgeLength() const
    {
        if (!selection_.edge)
            return std::nullopt;
        const auto* edge = document_.findEdge(*selection_.edge);
        if (!edge)
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
        const auto result = solver::solveFixedLengths(candidate, candidate.getFixedLengthConstraints(), fixedVertex);
        if (result.status != solver::SolveStatus::Converged)
            return false;

        document_ = std::move(candidate);
        pointerUp();
        return true;
    }

    bool EditorSession::removeSelectedEdgeLength()
    {
        if (!selection_.edge || !document_.removeFixedLength(*selection_.edge))
            return false;
        pointerUp();
        return true;
    }
}
