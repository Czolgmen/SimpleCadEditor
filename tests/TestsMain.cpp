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
        TestSuite{"SceneIO", cad::tests::sceneIOTests()},
        TestSuite{"EdgeStyle", cad::tests::edgeStyleTests()},
        TestSuite{"DocumentArc", cad::tests::documentArcTests()},
        TestSuite{"ArcContinuity", cad::tests::arcContinuityTests()},
        TestSuite{"ArcSolver", cad::tests::arcSolverTests()},
        TestSuite{"ArcConversion", cad::tests::arcConversionTests()},
        TestSuite{"ArcSplit", cad::tests::arcSplitTests()},
        TestSuite{"Bresenham", cad::tests::bresenhamTests()},
        TestSuite{"Wu", cad::tests::wuTests()},
        TestSuite{"LineClipping", cad::tests::lineClippingTests()},
        TestSuite{"DocumentParallel", cad::tests::documentParallelTests()},
        TestSuite{"EdgeConversion", cad::tests::edgeConversionTests()},
        TestSuite{"EdgeSampling", cad::tests::edgeSamplingTests()},
        TestSuite{"EdgeTangents", cad::tests::edgeTangentsTests()},
        TestSuite{"Continuity", cad::tests::continuityTests()},
        TestSuite{"ContinuityJoins", cad::tests::continuityJoinsTests()},
        TestSuite{"Vec2", cad::tests::vec2Tests()},
        TestSuite{"Bezier", cad::tests::bezierTests()},
        TestSuite{"Arc", cad::tests::arcTests()},
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
        TestSuite{"ContinuityConstraint", cad::tests::continuityConstraintTests()},
        TestSuite{"DenseLinearAlgebra", cad::tests::denseLinearAlgebraTests()},
        TestSuite{"ConstraintSolver", cad::tests::constraintSolverTests()},
        TestSuite{"MixedConstraintSolver", cad::tests::mixedConstraintSolverTests()},
        TestSuite{"Jacobian", cad::tests::jacobianTests()},
        TestSuite{"ContinuityJacobian", cad::tests::continuityJacobianTests()},
        TestSuite{"ContinuitySolver", cad::tests::continuitySolverTests()},
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
