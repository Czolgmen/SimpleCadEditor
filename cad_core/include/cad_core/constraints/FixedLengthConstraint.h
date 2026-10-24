#pragma once
#include <cad_core/model/Id.h>
#include <optional>

namespace cad
{
    class Document;
}

namespace cad::constraint
{
    struct FixedLengthConstraint
    {
        EdgeId edge;
        double length;
    };

    std::optional<double> fixedLengthError(
        const Document& document,
        const FixedLengthConstraint& constraint
    );
    
} // namespace cad::constraint
