#include <cad_core/model/Document.h>
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
}
