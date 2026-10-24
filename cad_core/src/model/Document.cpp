#include <cad_core/model/Document.h>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <cmath>

namespace cad {
    const Vertex *Document::findVertex(VertexId id) const
    {
        auto it = vertices.find(id);

        if (it == vertices.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const Polygon *Document::findPolygon(PolygonId id) const
    {
        auto it = polygons.find(id);

        if (it == polygons.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const Edge *Document::findEdge(EdgeId id) const
    {
        auto it = edges.find(id);

        if (it == edges.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const std::map<PolygonId, Polygon>& Document::getPolygons() const
    {
        return polygons;
    }

    PolygonId Document::addPolygon(std::span<const Vec2> positions)
    {

        if (positions.size() < 3)
            throw std::invalid_argument("Polygon requires at least 3 vertices");

        for (const auto& position : positions)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y))
                throw std::invalid_argument("Coordinates must be finite");
        }


        PolygonId polygonId{nextPolygonId++};
        Polygon polygon{polygonId};

        for(auto item : positions)
        {
            VertexId v_id{nextVertexId++};
            vertices.emplace(v_id, Vertex{v_id, item});
            polygon.vertices.push_back(v_id);
        }

        for(std::size_t i = 0; i < polygon.vertices.size(); i++)
        {
            VertexId start = polygon.vertices[i];
            VertexId end = polygon.vertices[(i+1) % polygon.vertices.size()];
            EdgeId e_id{nextEdgeId++};
            edges.emplace(e_id, Edge{e_id, start, end});
            polygon.edges.push_back(e_id);
        }

        polygons.emplace(polygonId, polygon);
        return polygonId;
    }

    std::vector<ValidationIssue> Document::validate() const
    {
        std::vector<ValidationIssue> issues;

        for (const auto& [polygonId, polygon] : polygons)
        {
            const std::size_t vertexCount = polygon.vertices.size();
            if (vertexCount < 3)
                issues.push_back({ValidationError::TooFewVertices, polygonId});

            if (polygon.edges.size() != vertexCount)
                issues.push_back({ValidationError::EdgeCountMismatch, polygonId});

            for (const VertexId vertexId : polygon.vertices)
            {
                const Vertex* vertex = findVertex(vertexId);
                if (vertex == nullptr)
                {
                    issues.push_back({ValidationError::MissingVertex, polygonId, vertexId});
                    continue;
                }

                if (!std::isfinite(vertex->position.x) || !std::isfinite(vertex->position.y))
                    issues.push_back({ValidationError::NonFinitePosition, polygonId, vertexId});
            }

            for (std::size_t i = 0; i < polygon.edges.size(); ++i)
            {
                const EdgeId edgeId = polygon.edges[i];
                const Edge* edge = findEdge(edgeId);
                if (edge == nullptr)
                {
                    issues.push_back({ValidationError::MissingEdge, polygonId, {}, edgeId});
                    continue;
                }

                // An edge can reference missing endpoints even when the polygon's
                // own vertex list contains only valid IDs.
                if (findVertex(edge->start) == nullptr)
                    issues.push_back({ValidationError::MissingVertex, polygonId, edge->start, edgeId});
                if (edge->end != edge->start && findVertex(edge->end) == nullptr)
                    issues.push_back({ValidationError::MissingVertex, polygonId, edge->end, edgeId});

                // Extra edges have no corresponding slot in the vertex cycle.
                // Their count has already been reported; avoid invalid indexing.
                if (i >= vertexCount)
                    continue;

                const VertexId expectedStart = polygon.vertices[i];
                const VertexId expectedEnd = polygon.vertices[(i + 1) % vertexCount];
                if (edge->start != expectedStart || edge->end != expectedEnd)
                    issues.push_back({ValidationError::InvalidEdgeConnection, polygonId, {}, edgeId});
            }
        }

        return issues;
    }
    bool Document::moveVertex(VertexId id, const Vec2 &position)
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y))
            return false;

        auto it = vertices.find(id);
        if (it == vertices.end())
            return false;

        it->second.position = position;
        return true;
    }
    bool Document::movePolygon(PolygonId id, const Vec2 &delta)
    {
        const auto* polygon = findPolygon(id);
        if(!polygon)
            return false;

        for(const auto vertexId: polygon->vertices)
        {
            const Vertex* vertex = findVertex(vertexId);

            if (!vertex)
                return false;

            const Vec2 newPosition = vertex->position + delta;

            if (!std::isfinite(newPosition.x) ||
                !std::isfinite(newPosition.y))
                {
                    return false;
                }
        }

        for(const auto vertexId: polygon->vertices)
        {
            auto it = vertices.find(vertexId);
            
            it->second.position = it->second.position + delta;
        }

        return true;
    }
    std::optional<VertexId> Document::splitEdge(EdgeId id)
    {
        auto edgeIt = edges.find(id);
        if (edgeIt == edges.end())
            return std::nullopt;

        const auto startIt = vertices.find(edgeIt->second.start);
        const auto endIt = vertices.find(edgeIt->second.end);
        if (startIt == vertices.end() || endIt == vertices.end())
            return std::nullopt;

        Polygon* owner = nullptr;
        std::size_t edgeIndex = 0;
        for (auto& [polygonId, polygon] : polygons)
        {
            const auto position = std::find(polygon.edges.begin(), polygon.edges.end(), id);
            if (position == polygon.edges.end())
                continue;

            // A document edge must occur exactly once in exactly one polygon.
            if (owner || std::count(polygon.edges.begin(), polygon.edges.end(), id) != 1)
                return std::nullopt;

            owner = &polygon;
            edgeIndex = static_cast<std::size_t>(std::distance(polygon.edges.begin(), position));
        }

        if (!owner || owner->vertices.size() < 3 ||
            owner->vertices.size() != owner->edges.size())
            return std::nullopt;

        const std::size_t nextIndex = (edgeIndex + 1) % owner->vertices.size();
        if (owner->vertices[edgeIndex] != edgeIt->second.start ||
            owner->vertices[nextIndex] != edgeIt->second.end)
            return std::nullopt;

        const Vec2& start = startIt->second.position;
        const Vec2& end = endIt->second.position;
        if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
            !std::isfinite(end.x) || !std::isfinite(end.y))
            return std::nullopt;

        // std::midpoint avoids overflow when adding two large finite coordinates.
        const Vec2 midpoint{std::midpoint(start.x, end.x), std::midpoint(start.y, end.y)};
        const VertexId newVertexId{nextVertexId};
        const EdgeId newEdgeId{nextEdgeId};

        // Prepare the updated cycle before modifying the stored entities.
        auto updatedVertices = owner->vertices;
        auto updatedEdges = owner->edges;
        const auto insertionOffset = static_cast<std::ptrdiff_t>(edgeIndex + 1);
        updatedVertices.insert(updatedVertices.begin() + insertionOffset, newVertexId);
        updatedEdges.insert(updatedEdges.begin() + insertionOffset, newEdgeId);

        vertices.emplace(newVertexId, Vertex{newVertexId, midpoint});
        try
        {
            edges.emplace(newEdgeId, Edge{newEdgeId, newVertexId, edgeIt->second.end});
        }
        catch (...)
        {
            vertices.erase(newVertexId);
            throw;
        }

        edgeIt->second.end = newVertexId;
        owner->vertices.swap(updatedVertices);
        owner->edges.swap(updatedEdges);
        ++nextVertexId;
        ++nextEdgeId;
        return newVertexId;
    }

    bool Document::deleteVertex(VertexId id)
    {
        const auto vertexIt = vertices.find(id);
        if (vertexIt == vertices.end())
            return false;

        Polygon* owner = nullptr;
        std::size_t vertexIndex = 0;
        for (auto& [polygonId, polygon] : polygons)
        {
            const auto position = std::find(polygon.vertices.begin(), polygon.vertices.end(), id);
            if (position == polygon.vertices.end())
                continue;

            if (owner || std::count(polygon.vertices.begin(), polygon.vertices.end(), id) != 1)
                return false;
            owner = &polygon;
            vertexIndex = static_cast<std::size_t>(std::distance(polygon.vertices.begin(), position));
        }

        if (!owner || owner->vertices.size() <= 3 || owner->edges.size() != owner->vertices.size())
            return false;

        const std::size_t count = owner->vertices.size();
        const std::size_t previousIndex = vertexIndex == 0 ? count - 1 : vertexIndex - 1;
        const std::size_t nextIndex = (vertexIndex + 1) % count;
        const VertexId previousId = owner->vertices[previousIndex];
        const VertexId nextId = owner->vertices[nextIndex];
        const auto previousIt = vertices.find(previousId);
        const auto nextIt = vertices.find(nextId);
        const auto incomingIt = edges.find(owner->edges[previousIndex]);
        const auto outgoingIt = edges.find(owner->edges[vertexIndex]);
        if (previousIt == vertices.end() || nextIt == vertices.end() ||
            incomingIt == edges.end() || outgoingIt == edges.end() || incomingIt == outgoingIt)
            return false;

        if (incomingIt->second.start != previousId || incomingIt->second.end != id ||
            outgoingIt->second.start != id || outgoingIt->second.end != nextId)
            return false;

        for (const Vec2& position : {vertexIt->second.position, previousIt->second.position, nextIt->second.position})
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y))
                return false;
        }

        const EdgeId newEdgeId{nextEdgeId};
        auto updatedVertices = owner->vertices;
        auto updatedEdges = owner->edges;
        const auto offset = static_cast<std::ptrdiff_t>(vertexIndex);
        updatedVertices.erase(updatedVertices.begin() + offset);
        updatedEdges[previousIndex] = newEdgeId;
        updatedEdges.erase(updatedEdges.begin() + offset);

        // The only map allocation happens before removing any stored entity.
        edges.emplace(newEdgeId, Edge{newEdgeId, previousId, nextId});
        owner->vertices.swap(updatedVertices);
        owner->edges.swap(updatedEdges);
        edges.erase(incomingIt);
        edges.erase(outgoingIt);
        vertices.erase(vertexIt);
        ++nextEdgeId;
        return true;
    }
}
