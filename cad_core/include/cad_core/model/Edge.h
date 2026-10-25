#pragma once

#include <cad_core/model/Id.h>
#include <cad_core/model/EdgeGeometry.h>

namespace cad
{
    struct Edge {
        EdgeId id;
        VertexId start;
        VertexId end;
        EdgeGeometry geometry = LineSegment{};
    };
}
