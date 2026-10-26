#pragma once
#include <cad_core/model/Id.h>


namespace cad {
    enum class ValidationError
    {
        TooFewVertices,
        EdgeCountMismatch,
        MissingVertex,
        MissingEdge,
        InvalidEdgeConnection,
        NonFinitePosition,
        InvalidArcGeometry,
        InvalidArcContinuity
    };

    struct ValidationIssue
    {
        ValidationError code;
        // IDs not associated with this issue remain zero.
        PolygonId polygon{};
        VertexId vertex{};
        EdgeId edge{};
    };
}

