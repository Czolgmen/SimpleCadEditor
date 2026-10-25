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
        TestSuite{"DocumentParallel", cad::tests::documentParallelTests()},
        TestSuite{"EdgeConversion", cad::tests::edgeConversionTests()},
        TestSuite{"EdgeSampling", cad::tests::edgeSamplingTests()},
        TestSuite{"Vec2", cad::tests::vec2Tests()},
        TestSuite{"Bezier", cad::tests::bezierTests()},
        TestSuite{"Validation", cad::tests::validationTests()},
        TestSuite{"HitTester", cad::tests::hitTesterTests()},
        TestSuite{"CurveHitTester", cad::tests::curveHitTesterTests()},
        TestSuite{"ControlPointHitTester", cad::tests::controlPointHitTesterTests()},
        TestSuite{"ControlPointMovement", cad::tests::controlPointMovementTests()},
        TestSuite{"ControlPointDrag", cad::tests::controlPointDragTests()},
        TestSuite{"SplitEdge", cad::tests::splitEdgeTests()},
        TestSuite{"DeleteVertex", cad::tests::deleteVertexTests()},
        TestSuite{"EditorSession", cad::tests::editorSessionTests()},
        TestSuite{"EditorParallel", cad::tests::editorParallelTests()},
        TestSuite{"FixedLengthConstraint", cad::tests::fixedLengthConstraintTests()},
        TestSuite{"ParallelConstraint", cad::tests::parallelConstraintTests()},
        TestSuite{"DenseLinearAlgebra", cad::tests::denseLinearAlgebraTests()},
        TestSuite{"ConstraintSolver", cad::tests::constraintSolverTests()},
        TestSuite{"MixedConstraintSolver", cad::tests::mixedConstraintSolverTests()},
        TestSuite{"Jacobian", cad::tests::jacobianTests()},
        TestSuite{"LengthInput", cad::tests::lengthInputTests()}
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
