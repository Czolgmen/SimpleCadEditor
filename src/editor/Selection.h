#pragma once
#include <cad_core/model/Id.h>
#include <optional>

namespace cad::editor
{
    struct Selection
    {
        std::optional<cad::VertexId> vertex;
        std::optional<cad::PolygonId> polygon;
    };
}