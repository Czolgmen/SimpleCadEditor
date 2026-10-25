#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/Id.h>
#include <cad_core/model/Continuity.h>


namespace cad
{
    struct Vertex {
        VertexId id;
        Vec2 position;
        ContinuitySetting continuity{};
    };
}
