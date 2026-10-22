#pragma once

#include <cad_core/model/Edge.h>
#include <cad_core/model/Vertex.h>
#include <cad_core/model/Polygon.h>
#include <cad_core/model/Id.h>
#include <cad_core/geometry/Vec2.h>

#include <map>
#include <span>


namespace cad
{
    // Document: wlasciciel wierzcholkow, krawedzi i wielokatow.
    class Document
    {
    private:
        std::map<VertexId, Vertex> vertices;
        std::map<PolygonId, Polygon> polygons;
        std::map<EdgeId, Edge> edges;

        std::uint64_t nextVertexId = 1;
        std::uint64_t nextEdgeId = 1;
        std::uint64_t nextPolygonId = 1;

    public:
        const Vertex* findVertex(VertexId id) const;
        const Polygon* findPolygon(PolygonId id) const;
        const Edge* findEdge(EdgeId id) const;

        PolygonId addPolygon(std::span<const Vec2> positions);
    };

}
