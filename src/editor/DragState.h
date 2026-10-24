#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/Id.h>

#include <variant>

namespace cad::editor
{
    struct DraggingVertex
    {
        cad::VertexId vertex;
        cad::Vec2 grabOffset{};
    };

    struct DraggingPolygon
    {
        cad::PolygonId polygon;
        cad::Vec2 pointerOrigin{};
    };

    using DragState = std::variant<DraggingVertex, DraggingPolygon>;
} // namespace cad::editor
