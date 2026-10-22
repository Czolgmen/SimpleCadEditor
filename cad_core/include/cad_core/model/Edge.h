#pragma once

#include <cad_core/model/Id.h>

namespace cad
{
    struct Edge {
        EdgeId id;
        VertexId start;
        VertexId end;
    };
}
