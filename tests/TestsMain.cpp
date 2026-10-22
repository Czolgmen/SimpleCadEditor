#include "TestSupport.h"

#include <array>
#include <exception>
#include <iostream>

namespace
{
    struct TestSuite
    {
        const char* name;
        std::span<const cad::tests::TestCase> tests;
    };
}

int main()
{
    const std::array suites{
        TestSuite{"Document", cad::tests::documentTests()},
        TestSuite{"Vec2", cad::tests::vec2Tests()}
    };

    std::size_t total = 0;
    std::size_t failed = 0;
    for (const TestSuite& suite : suites)
    {
        for (const cad::tests::TestCase& test : suite.tests)
        {
            ++total;
            try
            {
                test.run();
                std::cout << "[PASS] " << suite.name << ": " << test.name << '\n';
            }
            catch (const std::exception& error)
            {
                ++failed;
                std::cerr << "[FAIL] " << suite.name << ": " << test.name
                    << ": " << error.what() << '\n';
            }
            catch (...)
            {
                ++failed;
                std::cerr << "[FAIL] " << suite.name << ": " << test.name
                    << ": unknown exception\n";
            }
        }
    }

    std::cout << (total - failed) << '/' << total << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
