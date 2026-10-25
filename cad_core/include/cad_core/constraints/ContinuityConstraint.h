#pragma once

#include <cad_core/geometry/Vec2.h>
#include <cad_core/model/Continuity.h>
#include <cad_core/model/ContinuityJoins.h>

#include <optional>
#include <vector>

namespace cad::constraint
{
    std::optional<std::vector<double>> continuityErrors(
        Continuity continuity,
        const Vec2& incomingTangent,
        const Vec2& outgoingTangent);

    std::optional<std::vector<double>> continuityErrors(
        const Document& document,
        const ContinuityJoin& join);    
    
} // namespace cad::constraint
