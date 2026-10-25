#include <cad_core/model/EdgeTangents.h>

namespace cad
{
    Vec2 edgeStartTangent(const EdgeGeometry &geometry, const Vec2 &start, const Vec2 &end)
    {
        const CubicBezier* bezier;
        if((bezier = std::get_if<CubicBezier>(&geometry)))
        {
            return (bezier->control1 - start) * 3.0;
        }

        return end - start;
    }
    Vec2 edgeEndTangent(const EdgeGeometry &geometry, const Vec2 &start, const Vec2 &end)
    {
        const CubicBezier* bezier;
        if((bezier = std::get_if<CubicBezier>(&geometry)))
        {
            return (end - bezier->control2) * 3.0;
        }

        return end-start;
    }

} // namespace cad


