#include <cad_core/model/EdgeTangents.h>
#include <cad_core/geometry/Arc.h>
#include <cmath>

namespace
{
    std::optional<cad::Vec2> finiteTangent(cad::Vec2 tangent)
    {
        if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y))
            return std::nullopt;
        return tangent;
    }
}

namespace cad
{
    std::optional<Vec2> edgeStartTangent(const EdgeGeometry &geometry, const Vec2 &start, const Vec2 &end)
    {
        if (std::holds_alternative<LineSegment>(geometry))
            return finiteTangent(end - start);
        if (const auto* bezier = std::get_if<CubicBezier>(&geometry))
            return finiteTangent((bezier->control1 - start) * 3.0);
        if (const auto* arc = std::get_if<CircularArc>(&geometry))
            return arcStartTangent(start, end, arc->bulge);
        return std::nullopt;
    }
    std::optional<Vec2> edgeEndTangent(const EdgeGeometry &geometry, const Vec2 &start, const Vec2 &end)
    {
        if (std::holds_alternative<LineSegment>(geometry))
            return finiteTangent(end - start);
        if (const auto* bezier = std::get_if<CubicBezier>(&geometry))
            return finiteTangent((end - bezier->control2) * 3.0);
        if (const auto* arc = std::get_if<CircularArc>(&geometry))
            return arcEndTangent(start, end, arc->bulge);
        return std::nullopt;
    }

} // namespace cad


