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
}

