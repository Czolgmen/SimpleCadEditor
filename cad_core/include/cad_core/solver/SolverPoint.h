#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/ControlPointRef.h>

#include <optional>
#include <variant>

namespace cad
{
    class Document;
}

namespace cad::solver::detail
{
    using SolverPoint = std::variant<VertexId, ControlPointRef>;

    std::optional<Vec2> getPointPosition(
        const Document& document,
        const SolverPoint& point);

    bool setPointPosition(
        Document& document,
        const SolverPoint& point,
        const Vec2& position);
}