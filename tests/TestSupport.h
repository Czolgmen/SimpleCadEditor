#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cad::tests
{
    struct TestCase
    {
        const char* name;
        void (*run)();
    };

    inline void require(bool condition, std::string_view message)
    {
        if (!condition)
            throw std::runtime_error(std::string(message));
    }

    std::span<const TestCase> documentTests();
    std::span<const TestCase> vec2Tests();
}
