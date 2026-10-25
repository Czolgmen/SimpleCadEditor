#pragma once

#include <cad_core/model/Id.h>

namespace cad
{
    enum class BezierControlPoint
    {
        First,
        Second
    };

    struct ControlPointRef
    {
        EdgeId edge;
        BezierControlPoint point;

        bool operator==(const ControlPointRef&) const = default;
    };
    
} // namespace cad
