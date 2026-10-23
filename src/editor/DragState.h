#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/Id.h>

#include <optional>

namespace cad::editor
{
    struct DragState {
        std::optional<cad::VertexId> vertex;
        cad::Vec2 grabOffset{};
        std::optional<cad::PolygonId> polygon;
        cad::Vec2 previousWorldPosition{};
    };
} // namespace cad::editor
