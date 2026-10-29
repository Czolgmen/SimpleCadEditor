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
        int strokeWidth = 2; // Screen pixels; persistent appearance, not a geometric constraint.
    };
}
