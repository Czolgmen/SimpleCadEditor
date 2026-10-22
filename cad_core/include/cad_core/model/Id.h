#pragma once

#include <compare>
#include <cstdint>

namespace cad
{
    // Zero denotes an invalid ID. Document assigns IDs starting from 1.
    struct VertexId
    {
        std::uint64_t value = 0;

        auto operator<=>(const VertexId&) const = default;
    };

    struct EdgeId
    {
        std::uint64_t value = 0;

        auto operator<=>(const EdgeId&) const = default;
    };

    struct PolygonId
    {
        std::uint64_t value = 0;

        auto operator<=>(const PolygonId&) const = default;
    };
}
