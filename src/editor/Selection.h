#pragma once
#include <cad_core/model/Id.h>
#include <cad_core/model/ControlPointRef.h>
#include <optional>

namespace cad::editor
{
    struct Selection
    {
        std::optional<cad::VertexId> vertex;
        std::optional<cad::PolygonId> polygon;
        std::optional<cad::EdgeId> edge;
        std::optional<cad::ControlPointRef> controlPoint;
    };
}