#pragma once

#include <cad_core/model/Id.h>
#include <optional>

namespace cad
{
    class Document;
} // namespace cad

namespace cad::constraint
{
    struct ParallelConstraint
    {
        EdgeId first;
        EdgeId second;
    };

    std::optional<double> parallelError(
        const Document& document,
        const ParallelConstraint& constraint
    );

} // namespace cad::constraint
