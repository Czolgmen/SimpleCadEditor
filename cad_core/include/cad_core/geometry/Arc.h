#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cstddef>
#include <optional>
#include <vector>

namespace cad
{
    // bulge = tan(sweep/4) must be finite and nonzero; its sign selects CCW/CW.
    // +/-1 gives a semicircle; |bulge| below/above 1 gives a minor/major arc.
    // Returns steps+1 finite points with exact endpoints; invalid geometry throws
    // invalid_argument, and a sample count exceeding vector capacity throws length_error.
    std::vector<Vec2> sampleArc(
        const Vec2& start,
        const Vec2& end,
        double bulge,
        std::size_t steps);

    struct ArcParameters
    {
        Vec2 center;
        double radius;
        double startAngle;
        double sweep;
    };

    // No sampling or allocation. Invalid or unrepresentable circle parameters
    // return nullopt; sampleArc separately validates the generated coordinates.
    std::optional<ArcParameters> computeArcParameters(
        const Vec2& start,
        const Vec2& end,
        double bulge);

    // Derivatives for the arc parameter t in [0,1], with their full magnitude.
    // Invalid parameters, non-finite components or a zero vector return nullopt.
    std::optional<Vec2> arcStartTangent(const Vec2& start, const Vec2& end, double bulge);
    std::optional<Vec2> arcEndTangent(const Vec2& start, const Vec2& end, double bulge);

    // The tangent points along start -> end; its positive magnitude is irrelevant.
    // Returns a finite nonzero bulge, or nullopt for invalid data, a tangent
    // parallel to the chord, or unrepresentable circle parameters. No sampling.
    std::optional<double> arcBulgeFromStartTangent(
        const Vec2& start,
        const Vec2& end,
        const Vec2& tangent);

    std::optional<double> arcBulgeFromEndTangent(
        const Vec2& start,
        const Vec2& end,
        const Vec2& tangent);

}
