#pragma once

#include <cad_core/model/Edge.h>
#include <cad_core/model/Vertex.h>
#include <cad_core/model/Polygon.h>
#include <cad_core/model/Id.h>
#include <cad_core/model/Validation.h>

#include <cad_core/constraints/FixedLengthConstraint.h>
#include <cad_core/constraints/ParallelConstraint.h>

#include <cad_core/geometry/Vec2.h>

#include <map>
#include <optional>
#include <span>
#include <vector>


namespace cad
{
    // Document: wlasciciel wierzcholkow, krawedzi i wielokatow.
    class Document
    {
    private:
        std::map<VertexId, Vertex> vertices;
        std::map<PolygonId, Polygon> polygons;
        std::map<EdgeId, Edge> edges;
        std::vector<constraint::FixedLengthConstraint> fixedLengthConstraints;
        std::vector<constraint::ParallelConstraint> parallelConstraints;

        std::uint64_t nextVertexId = 1;
        std::uint64_t nextEdgeId = 1;
        std::uint64_t nextPolygonId = 1;

    public:
        const Vertex* findVertex(VertexId id) const;
        const Polygon* findPolygon(PolygonId id) const;
        const Edge* findEdge(EdgeId id) const;
        const std::map<PolygonId, Polygon>& getPolygons() const;

        PolygonId addPolygon(std::span<const Vec2> positions);
        std::vector<ValidationIssue> validate() const;
        bool moveVertex(VertexId id, const Vec2& position);
        bool movePolygon(PolygonId id, const Vec2& delta);
        // Keeps the original edge ID for start -> midpoint; returns the new vertex ID.
        std::optional<VertexId> splitEdge(EdgeId id);
        // Replaces both incident edges with a new segment; triangles cannot lose vertices.
        bool deleteVertex(VertexId id);

        std::span<const constraint::FixedLengthConstraint> getFixedLengthConstraints() const;
        std::span<const constraint::ParallelConstraint> getParallelConstraints() const;

        bool setFixedLength(EdgeId edge, double length);
        bool removeFixedLength(EdgeId edge);

        // Adds a valid unordered pair without moving geometry; duplicates return false.
        bool addParallel(EdgeId first, EdgeId second);
        // Either order is accepted; a missing pair returns false. Geometry need not be valid.
        bool removeParallel(EdgeId first, EdgeId second);

        void removeParallelsForEdge(EdgeId edge);

        bool convertEdgeToBezier(EdgeId id);
        bool convertEdgeToSegment(EdgeId id);
        

    };

}
