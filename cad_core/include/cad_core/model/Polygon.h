#pragma once

#include <cad_core/model/Id.h>
#include <vector>

namespace cad
{
    struct Polygon {
        PolygonId id;
        std::vector<VertexId> vertices;
        std::vector<EdgeId> edges;
    };
}
