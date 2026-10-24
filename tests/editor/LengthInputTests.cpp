#include <editor/LengthInput.h>
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <utility>

namespace
{
    using cad::editor::parsePositiveLength;
    using cad::tests::require;

    void acceptsPositiveDecimalAndExponent()
    {
        for (const auto& [text, expected] : std::array{
            std::pair{L"100", 100.0}, std::pair{L"12.5", 12.5}, std::pair{L".5", 0.5},
            std::pair{L"1e3", 1000.0}, std::pair{L"2.5e-2", 0.025}, std::pair{L"1E+2", 100.0}})
        {
            const auto result = parsePositiveLength(text);
            require(result && std::abs(*result - expected) <= 1e-12, "Valid length was not parsed");
        }
    }

    void acceptsDecimalComma()
    {
        for (const auto& [text, expected] : std::array{
            std::pair{L"12,5", 12.5}, std::pair{L",5", 0.5}, std::pair{L"1,25e2", 125.0}})
        {
            const auto result = parsePositiveLength(text);
            require(result && std::abs(*result - expected) <= 1e-12, "Comma input failed");
        }
    }

    void trimsWhitespaceAndAcceptsPlus()
    {
        for (const auto text : {L" 12.5 ", L"\t+12,5\r\n", L"+12.5"})
        {
            const auto result = parsePositiveLength(text);
            require(result && *result == 12.5, "Whitespace or leading plus failed");
        }
    }

    void rejectsEmptyOrMalformedInput()
    {
        for (const auto text : {L"", L" \t\r\n", L"+", L"abc", L"12px", L"1 000", L"1,2.3", L"1..2",
            L"++1", L"+ 1", L"0x10", L"1e", L"1e2junk", L"12\u00a05", L"\u22121"})
            require(!parsePositiveLength(text), "Malformed length was accepted");
        const std::wstring_view embeddedNull{L"12\0junk", 7};
        require(!parsePositiveLength(embeddedNull), "Embedded null hid trailing garbage");
    }

    void rejectsNonpositiveAndNonFiniteInput()
    {
        for (const auto text : {L"0", L"-0", L"0.0", L"-1", L"-0,5", L"nan", L"NaN", L"inf", L"+inf",
            L"-inf", L"infinity", L"1e9999", L"1e-9999"})
            require(!parsePositiveLength(text), "Invalid length value was accepted");
    }

    void preservesRepresentableSmallAndLargeValues()
    {
        for (const auto& [text, expected] : std::array{std::pair{L"1e-200", 1e-200}, std::pair{L"1e200", 1e200}})
        {
            const auto result = parsePositiveLength(text);
            require(result && std::isfinite(*result) && std::abs(*result / expected - 1) < 1e-12,
                "Representable extreme length was lost");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> lengthInputTests()
    {
        static const std::array tests{
            TestCase{"positive decimals and exponents", acceptsPositiveDecimalAndExponent},
            TestCase{"decimal comma", acceptsDecimalComma},
            TestCase{"surrounding whitespace and plus", trimsWhitespaceAndAcceptsPlus},
            TestCase{"empty, malformed and partial input", rejectsEmptyOrMalformedInput},
            TestCase{"nonpositive, non-finite and out-of-range values", rejectsNonpositiveAndNonFiniteInput},
            TestCase{"representable small and large values", preservesRepresentableSmallAndLargeValues}
        };
        return tests;
    }
}
