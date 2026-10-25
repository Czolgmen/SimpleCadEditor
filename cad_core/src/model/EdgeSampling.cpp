#include <cad_core/model/EdgeSampling.h>

#include <cad_core/geometry/Bezier.h>
#include <cad_core/geometry/Arc.h>

namespace cad
{
    std::vector<Vec2> sampleEdge(const EdgeGeometry &geometry, const Vec2 &start, const Vec2 &end, std::size_t steps)
    {
        if (std::holds_alternative<CubicBezier>(geometry))
        {
            const auto& bezier = std::get<CubicBezier>(geometry);

            return sampleBezier(start, bezier.control1, bezier.control2, end, steps);
        } if (const auto* arc = std::get_if<CircularArc>(&geometry))
        {
            return sampleArc(start, end, arc->bulge, steps);
        } 
        else 
        {
            return {start, end};
        }
    }


} // namespace cad
