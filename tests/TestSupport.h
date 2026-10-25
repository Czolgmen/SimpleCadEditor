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
    std::span<const TestCase> documentParallelTests();
    std::span<const TestCase> edgeConversionTests();
    std::span<const TestCase> edgeSamplingTests();
    std::span<const TestCase> edgeTangentsTests();
    std::span<const TestCase> continuityTests();
    std::span<const TestCase> continuityJoinsTests();
    std::span<const TestCase> vec2Tests();
    std::span<const TestCase> bezierTests();
    std::span<const TestCase> arcTests();
    std::span<const TestCase> validationTests();
    std::span<const TestCase> hitTesterTests();
    std::span<const TestCase> curveHitTesterTests();
    std::span<const TestCase> controlPointHitTesterTests();
    std::span<const TestCase> controlPointMovementTests();
    std::span<const TestCase> controlPointDragTests();
    std::span<const TestCase> splitEdgeTests();
    std::span<const TestCase> deleteVertexTests();
    std::span<const TestCase> editorSessionTests();
    std::span<const TestCase> editorParallelTests();
    std::span<const TestCase> fixedLengthConstraintTests();
    std::span<const TestCase> parallelConstraintTests();
    std::span<const TestCase> continuityConstraintTests();
    std::span<const TestCase> denseLinearAlgebraTests();
    std::span<const TestCase> constraintSolverTests();
    std::span<const TestCase> mixedConstraintSolverTests();
    std::span<const TestCase> jacobianTests();
    std::span<const TestCase> continuityJacobianTests();
    std::span<const TestCase> continuitySolverTests();
    std::span<const TestCase> lengthInputTests();
}
