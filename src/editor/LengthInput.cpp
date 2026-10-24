#include <editor/LengthInput.h>

#include <charconv>
#include <cmath>
#include <string>

namespace cad::editor
{
    std::optional<double> parsePositiveLength(std::wstring_view text)
    {
        constexpr std::wstring_view whitespace = L" \t\r\n";
        const auto first = text.find_first_not_of(whitespace);
        if (first == std::wstring_view::npos)
            return std::nullopt;
        const auto last = text.find_last_not_of(whitespace);
        text = text.substr(first, last - first + 1);
        if (text.front() == L'+')
            text.remove_prefix(1);
        if (text.empty())
            return std::nullopt;

        std::string number;
        number.reserve(text.size());
        for (const wchar_t character : text)
        {
            if (static_cast<unsigned long>(character) > 127UL)
                return std::nullopt;
            number.push_back(character == L',' ? '.' : static_cast<char>(character));
        }

        double value = 0;
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec != std::errc{} || result.ptr != number.data() + number.size() || !std::isfinite(value) || value <= 0)
            return std::nullopt;
        return value;
    }
}
