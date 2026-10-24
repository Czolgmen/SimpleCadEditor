#include <cad_core/model/Document.h>

#include "../../cad_core/src/solver/SolverDetail.h"
#include "../TestSupport.h"

#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::solver::detail;
    using constraint::FixedLengthConstraint;
    using cad::tests::require;

    struct Fixture
    {
        Document document;
        std::array<VertexId, 3> vertices;
        std::array<EdgeId, 2> edges;

        Fixture(double scale = 1)
        {
            const std::array points{Vec2{0, 0}, Vec2{6 * scale, 8 * scale}, Vec2{6 * scale, 18 * scale}};
            const auto* polygon = document.findPolygon(document.addPolygon(points));
            vertices = {polygon->vertices[0], polygon->vertices[1], polygon->vertices[2]};
            edges = {polygon->edges[0], polygon->edges[1]};
        }
    };

    void requireNear(double actual, double expected, double tolerance = 2e-7)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, "Unexpected derivative");
    }

    void diagonalEndHasExpectedDerivatives()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[1]};
        const auto errors = evaluateErrors(f.document, constraints);
        const auto matrix = computeJacobian(f.document, constraints, variables, *errors, 10);
        require(matrix && matrix->size() == 1 && (*matrix)[0].size() == 2, "Wrong matrix dimensions");
        requireNear((*matrix)[0][0], 0.6);
        requireNear((*matrix)[0][1], 0.8);
    }

    void bothEndpointsHaveOppositeSigns()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[0], f.vertices[1]};
        const auto errors = evaluateErrors(f.document, constraints);
        const auto matrix = computeJacobian(f.document, constraints, variables, *errors, 10);
        require(matrix && (*matrix)[0].size() == 4, "Missing endpoint columns");
        const std::array expected{-0.6, -0.8, 0.6, 0.8};
        for (std::size_t column = 0; column < expected.size(); ++column)
            requireNear((*matrix)[0][column], expected[column]);
    }

    void rowsAndColumnsFollowInputOrder()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[1], 7}, FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[2], f.vertices[0], f.vertices[1]};
        const auto errors = evaluateErrors(f.document, constraints);
        const auto matrix = computeJacobian(f.document, constraints, variables, *errors, 10);
        require(matrix && matrix->size() == 2, "Wrong row count");
        const DenseMatrix expected{{0, 1, 0, 0, 0, -1}, {0, 0, -0.6, -0.8, 0.6, 0.8}};
        for (std::size_t row = 0; row < expected.size(); ++row)
        {
            require((*matrix)[row].size() == expected[row].size(), "Wrong column count");
            for (std::size_t column = 0; column < expected[row].size(); ++column)
                requireNear((*matrix)[row][column], expected[row][column]);
        }
        require((*matrix)[0][2] == 0 && (*matrix)[0][3] == 0 && (*matrix)[1][0] == 0 && (*matrix)[1][1] == 0,
            "Unrelated vertex affected a constraint");
    }

    void samplingPreservesDocumentAndInputs()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[0], f.vertices[1]};
        const auto errors = evaluateErrors(f.document, constraints);
        const auto originalErrors = *errors;
        const auto matrix = computeJacobian(f.document, constraints, variables, *errors, 10);
        require(matrix.has_value(), "Sampling failed");
        const std::array expected{Vec2{0, 0}, Vec2{6, 8}, Vec2{6, 18}};
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            const auto* vertex = f.document.findVertex(f.vertices[i]);
            require(vertex && vertex->position.x == expected[i].x && vertex->position.y == expected[i].y,
                "Sampling changed the source vertex");
        }
        require(*errors == originalErrors && constraints[0].length == 5, "Sampling modified input arrays");
        require(f.document.validate().empty(), "Sampling invalidated topology");
    }

    void handlesSmallAndLargeScales()
    {
        for (const double scale : {1e-150, 1e150})
        {
            const Fixture f{scale};
            const std::array constraints{FixedLengthConstraint{f.edges[0], 5 * scale}};
            const std::array variables{f.vertices[1]};
            const auto errors = evaluateErrors(f.document, constraints);
            const auto matrix = computeJacobian(f.document, constraints, variables, *errors, 10 * scale);
            require(matrix.has_value(), "Scaled geometry could not be sampled");
            requireNear((*matrix)[0][0], 0.6);
            requireNear((*matrix)[0][1], 0.8);
        }
    }

    void usesRepresentableStepAtLargeOffset()
    {
        Document document;
        const double offset = 1e12;
        const std::array points{Vec2{offset, 0}, Vec2{offset + 10, 0}, Vec2{offset, 20}};
        const auto* polygon = document.findPolygon(document.addPolygon(points));
        const std::array constraints{FixedLengthConstraint{polygon->edges[0], 5}};
        const std::array variables{polygon->vertices[1]};
        const auto errors = evaluateErrors(document, constraints);
        require(offset + 10 + std::sqrt(std::numeric_limits<double>::epsilon()) * 10 == offset + 10,
            "Fixture does not exercise rounded-away addition");
        const auto matrix = computeJacobian(document, constraints, variables, *errors, 10);
        require(matrix.has_value(), "Representable fallback step failed");
        requireNear((*matrix)[0][0], 1);
        requireNear((*matrix)[0][1], 0);
        require(document.findVertex(variables[0])->position.x == offset + 10, "Fallback modified source geometry");
    }

    void rejectsInvalidScaleOrUnusableStep()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[1]};
        const auto errors = evaluateErrors(f.document, constraints);
        for (const double scale : {0.0, -1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min()})
            require(!computeJacobian(f.document, constraints, variables, *errors, scale), "Invalid scale/step accepted");
    }

    void rejectsInvalidBaseErrors()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const std::array variables{f.vertices[1]};
        require(!computeJacobian(f.document, constraints, variables, {}, 10), "Wrong error count accepted");
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            const std::array errors{invalid};
            require(!computeJacobian(f.document, constraints, variables, errors, 10), "Non-finite base error accepted");
        }
    }

    void rejectsMissingOrNonFiniteVariable()
    {
        Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const auto errors = evaluateErrors(f.document, constraints);
        const std::array missing{VertexId{999999}};
        require(!computeJacobian(f.document, constraints, missing, *errors, 10), "Unknown variable accepted");
        // Corrupt an unrelated vertex; errors for the constrained edge remain finite.
        auto* vertex = const_cast<Vertex*>(f.document.findVertex(f.vertices[2]));
        vertex->position.y = std::numeric_limits<double>::infinity();
        const std::array variables{f.vertices[2]};
        require(!computeJacobian(f.document, constraints, variables, *errors, 10), "Non-finite variable accepted");
    }

    void rejectsFailedErrorEvaluation()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{EdgeId{}, 5}};
        const std::array variables{f.vertices[1]};
        const std::array errors{0.0};
        require(!computeJacobian(f.document, constraints, variables, errors, 10), "Failed sample evaluation accepted");
    }

    void rejectsUnrepresentablePositiveSample()
    {
        Document document;
        const double maximum = std::numeric_limits<double>::max();
        const std::array points{Vec2{maximum, 0}, Vec2{maximum, 1}, Vec2{0, 0}};
        const auto* polygon = document.findPolygon(document.addPolygon(points));
        const std::array constraints{FixedLengthConstraint{polygon->edges[0], 1}};
        const std::array variables{polygon->vertices[1]};
        const auto errors = evaluateErrors(document, constraints);
        require(!computeJacobian(document, constraints, variables, *errors, 1), "Infinite sample position accepted");
        require(document.findVertex(variables[0])->position.x == maximum, "Rejected sample changed document");
    }

    void emptyDimensionsAreSupported()
    {
        const Fixture f;
        const std::array constraints{FixedLengthConstraint{f.edges[0], 5}};
        const auto errors = evaluateErrors(f.document, constraints);
        const auto noColumns = computeJacobian(f.document, constraints, {}, *errors, 10);
        require(noColumns && noColumns->size() == 1 && (*noColumns)[0].empty(), "Zero-column matrix failed");
        const auto empty = computeJacobian(f.document, {}, {}, {}, 10);
        require(empty && empty->empty(), "Empty matrix failed");
    }
}

namespace cad::tests
{
    std::span<const TestCase> jacobianTests()
    {
        static const std::array tests{
            TestCase{"diagonal endpoint derivatives", diagonalEndHasExpectedDerivatives},
            TestCase{"opposite signs at both endpoints", bothEndpointsHaveOppositeSigns},
            TestCase{"row and column order with unrelated zero entries", rowsAndColumnsFollowInputOrder},
            TestCase{"independent samples preserve source geometry and inputs", samplingPreservesDocumentAndInputs},
            TestCase{"small and large geometry scales", handlesSmallAndLargeScales},
            TestCase{"nextafter fallback and actual displacement", usesRepresentableStepAtLargeOffset},
            TestCase{"invalid scales and underflowing step", rejectsInvalidScaleOrUnusableStep},
            TestCase{"invalid base error dimensions and values", rejectsInvalidBaseErrors},
            TestCase{"missing or non-finite variable vertex", rejectsMissingOrNonFiniteVariable},
            TestCase{"failed sample error evaluation", rejectsFailedErrorEvaluation},
            TestCase{"unrepresentable positive sample", rejectsUnrepresentablePositiveSample},
            TestCase{"empty matrix dimensions", emptyDimensionsAreSupported}
        };
        return tests;
    }
}
