#pragma once

namespace cad
{
    enum class Continuity
    {
        G0,
        G1,
        C1
    };

    enum class ContinuitySource
    {
        Default,
        Explicit
    };

    struct ContinuitySetting
    {
        Continuity value = Continuity::G0;
        ContinuitySource source = ContinuitySource::Default;

        bool operator==(const ContinuitySetting&) const = default;
    };
} // namespace cad
