#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/Id.h>

#include <optional>

namespace cad
{
    class Document;
}

namespace cad::editor
{
    // Both the pointer position and tolerance are expressed in world units.
    std::optional<VertexId> hitTestVertex(
        const Document& document,
        const Vec2& worldPosition,
        double tolerance);

    std::optional<PolygonId> hitTestPolygon (
        const Document& document,
        const Vec2& worldPosition
    );
}
