#pragma once

#include <cad_core/model/Id.h>
#include <cad_core/model/Continuity.h>

#include <optional>
#include <vector>

namespace cad
{
    class Document;

    struct ContinuityJoin
    {
        VertexId vertex;
        EdgeId incoming;
        EdgeId outgoing;
        Continuity continuity;
    };

    std::optional<std::vector<ContinuityJoin>>
        collectContinuityJoins(const Document& document);
}