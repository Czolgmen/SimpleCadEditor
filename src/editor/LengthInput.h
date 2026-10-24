#pragma once

#include <optional>
#include <string_view>

namespace cad::editor
{
    // Locale-independent decimal input; accepts either comma or dot, and exponents.
    std::optional<double> parsePositiveLength(std::wstring_view text);
}
