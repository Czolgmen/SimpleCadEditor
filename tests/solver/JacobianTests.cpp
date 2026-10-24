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
    using constraint::ParallelConstraint;
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

    void requireMatrixNear(const std::optional<DenseMatrix>& actual, const DenseMatrix& expected,
        double tolerance = 2e-7)
    {
        require(actual && actual->size() == expected.size(), "Wrong Jacobian row count");
        for (std::size_t row = 0; row < expected.size(); ++row)
        {
            require((*actual)[row].size() == expected[row].size(), "Wrong Jacobian column count");
            for (std::size_t column = 0; column < expected[row].size(); ++column)
                requireNear((*actual)[row][column], expected[row][column], tolerance);
        }
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

    void mixedRowsUseKnownDerivativesAndInputOrder()
    {
        Fixture f;
        const std::array extraPositions{Vec2{-5, -5}, Vec2{-4, -5}, Vec2{-5, -4}};
        const VertexId unrelated = f.document.findPolygon(f.document.addPolygon(extraPositions))->vertices[0];
        const std::array lengths{FixedLengthConstraint{f.edges[0], 5}, FixedLengthConstraint{f.edges[1], 7}};
        const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}};
        const std::array variables{f.vertices[2], f.vertices[0], f.vertices[1], unrelated};
        const auto errors = evaluateErrors(f.document, lengths, parallels);
        require(errors && errors->size() == 3, "Mixed raw error vector has wrong size");
        requireNear((*errors)[0], 5);
        requireNear((*errors)[1], 3);
        requireNear((*errors)[2], 0.6);
        const auto matrix = computeConstraintJacobian(f.document, lengths, parallels, variables, *errors, 10);
        // Both edges meet at B. Its parallel derivative contains contributions from BOTH edges.
        const DenseMatrix expected{
            {0, 0, -0.6, -0.8, 0.6, 0.8, 0, 0},
            {0, 1, 0, 0, 0, -1, 0, 0},
            {-0.8, 0, -0.64, 0.48, 1.44, -0.48, 0, 0}};
        requireMatrixNear(matrix, expected);
    }

    void parallelOnlyJacobianIncludesAllFourEndpoints()
    {
        Document document;
        const std::array firstPoints{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
        const std::array secondPoints{Vec2{20, 0}, Vec2{26, 8}, Vec2{20, 10}};
        const auto* first = document.findPolygon(document.addPolygon(firstPoints));
        const auto* second = document.findPolygon(document.addPolygon(secondPoints));
        const std::array parallels{ParallelConstraint{first->edges[0], second->edges[0]}};
        const std::array variables{first->vertices[0], first->vertices[1], second->vertices[0], second->vertices[1]};
        const auto errors = evaluateErrors(document, {}, parallels);
        require(errors.has_value(), "Parallel error evaluation failed");
        const auto matrix = computeConstraintJacobian(document, {}, parallels, variables, *errors, 10);
        requireMatrixNear(matrix, {{0, 0.6, 0, -0.6, 0.48, -0.36, -0.48, 0.36}});
    }

    void mixedJacobianIsInvariantUnderUniformGeometryScaling()
    {
        for (const double scale : {1e-100, 1.0, 1e100})
        {
            const Fixture f{scale};
            const std::array lengths{FixedLengthConstraint{f.edges[0], 5 * scale}, FixedLengthConstraint{f.edges[1], 7 * scale}};
            const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}};
            const std::array variables{f.vertices[2], f.vertices[0], f.vertices[1]};
            const auto errors = evaluateErrors(f.document, lengths, parallels);
            require(errors.has_value(), "Scaled mixed errors failed");
            const auto matrix = computeConstraintJacobian(f.document, lengths, parallels, variables, *errors, 10 * scale);
            requireMatrixNear(matrix, {{0, 0, -0.6, -0.8, 0.6, 0.8}, {0, 1, 0, 0, 0, -1},
                {-0.8, 0, -0.64, 0.48, 1.44, -0.48}});
        }
    }

    void changingNormalizationScaleAffectsOnlyParallelRows()
    {
        const Fixture f;
        const std::array lengths{FixedLengthConstraint{f.edges[0], 5}};
        const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}};
        const std::array variables{f.vertices[1]};
        const auto errors = evaluateErrors(f.document, lengths, parallels);
        require(errors.has_value(), "Mixed error evaluation failed");
        for (const double scale : {2.0, 10.0, 50.0})
        {
            const auto matrix = computeConstraintJacobian(f.document, lengths, parallels, variables, *errors, scale);
            require(matrix && matrix->size() == 2 && (*matrix)[0].size() == 2 && (*matrix)[1].size() == 2,
                "Wrong mixed matrix dimensions");
            requireNear((*matrix)[0][0], 0.6);
            requireNear((*matrix)[0][1], 0.8);
            requireNear((*matrix)[1][0] / scale, 0.144);
            requireNear((*matrix)[1][1] / scale, -0.048);
        }
    }

    void reversedParallelPairsHaveOppositeDerivativeRows()
    {
        const Fixture f;
        const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}, ParallelConstraint{f.edges[1], f.edges[0]}};
        const std::array variables{f.vertices[1]};
        const auto errors = evaluateErrors(f.document, {}, parallels);
        require(errors.has_value(), "Parallel error evaluation failed");
        const auto matrix = computeConstraintJacobian(f.document, {}, parallels, variables, *errors, 10);
        requireMatrixNear(matrix, {{1.44, -0.48}, {-1.44, 0.48}});
    }

    void mixedSamplesPreserveSourceAndRawInputs()
    {
        Fixture f;
        require(f.document.setFixedLength(f.edges[0], 10), "Fixture relation failed");
        const Document before = f.document;
        const std::array lengths{FixedLengthConstraint{f.edges[0], 5}};
        const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}};
        const std::array variables{f.vertices[0], f.vertices[1], f.vertices[2]};
        const auto errors = evaluateErrors(f.document, lengths, parallels);
        require(errors.has_value(), "Mixed error evaluation failed");
        const auto originalErrors = *errors;
        require(computeConstraintJacobian(f.document, lengths, parallels, variables, *errors, 10).has_value(),
            "Mixed sampling failed");
        require(*errors == originalErrors && lengths[0].length == 5 && parallels[0].first == f.edges[0] &&
            parallels[0].second == f.edges[1], "Sampling modified the input arrays");
        require(f.document.getPolygons().size() == before.getPolygons().size(), "Sampling changed polygon count");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* actual = f.document.findPolygon(id);
            require(actual && actual->vertices == polygon.vertices && actual->edges == polygon.edges, "Topology changed");
            for (const VertexId vertexId : polygon.vertices)
            {
                const Vec2 original = before.findVertex(vertexId)->position;
                const Vec2 position = f.document.findVertex(vertexId)->position;
                require(position.x == original.x && position.y == original.y, "Source position changed");
            }
        }
        const auto stored = f.document.getFixedLengthConstraints();
        require(stored.size() == 1 && stored[0].edge == f.edges[0] && stored[0].length == 10,
            "Sampling overwrote a stored constraint with a supplied constraint");
    }

    void mixedJacobianRejectsInvalidInputs()
    {
        const Fixture f;
        const std::array lengths{FixedLengthConstraint{f.edges[0], 5}};
        const std::array parallels{ParallelConstraint{f.edges[0], f.edges[1]}};
        const std::array variables{f.vertices[1]};
        const auto errors = evaluateErrors(f.document, lengths, parallels);
        require(errors.has_value(), "Mixed error evaluation failed");
        const std::array tooFew{5.0};
        require(!computeConstraintJacobian(f.document, lengths, parallels, variables, tooFew, 10),
            "Missing parallel residual was accepted");
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        {
            for (std::size_t row = 0; row < errors->size(); ++row)
            {
                auto broken = *errors;
                broken[row] = invalid;
                require(!computeConstraintJacobian(f.document, lengths, parallels, variables, broken, 10),
                    "Non-finite mixed residual was accepted");
            }
        }
        for (const double scale : {0.0, -1.0, std::numeric_limits<double>::infinity()})
            require(!computeConstraintJacobian(f.document, lengths, parallels, variables, *errors, scale),
                "Invalid mixed scale was accepted");
        const std::array missing{VertexId{999999}};
        require(!computeConstraintJacobian(f.document, lengths, parallels, missing, *errors, 10),
            "Missing mixed variable was accepted");
        const std::array brokenParallels{ParallelConstraint{f.edges[0], EdgeId{}}};
        require(!computeConstraintJacobian(f.document, lengths, brokenParallels, variables, *errors, 10),
            "Failed parallel sample evaluation was accepted");
    }

    void collapsedParallelSampleRejectsWithoutMutation()
    {
        Document document;
        const double step = std::sqrt(std::numeric_limits<double>::epsilon());
        const std::array firstPoints{Vec2{0, 0}, Vec2{step, 0}, Vec2{0, 1}};
        const std::array secondPoints{Vec2{0, 2}, Vec2{1, 2}, Vec2{0, 3}};
        const auto* first = document.findPolygon(document.addPolygon(firstPoints));
        const auto* second = document.findPolygon(document.addPolygon(secondPoints));
        const std::array parallels{ParallelConstraint{first->edges[0], second->edges[0]}};
        const std::array variables{first->vertices[0]};
        const auto errors = evaluateErrors(document, {}, parallels);
        require(errors.has_value(), "Initial directions should be valid");
        require(!computeConstraintJacobian(document, {}, parallels, variables, *errors, 1),
            "Sample collapsing a parallel edge was accepted");
        const Vec2 position = document.findVertex(variables[0])->position;
        require(position.x == 0 && position.y == 0 && document.validate().empty(), "Rejected sample modified source");
        const auto after = evaluateErrors(document, {}, parallels);
        require(after && *after == *errors, "Rejected sample changed original residuals");
    }

    void mixedJacobianUsesActualNormalizedDisplacement()
    {
        Document document;
        const double offset = 1e12;
        const std::array firstPoints{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
        const std::array secondPoints{Vec2{offset, 0}, Vec2{offset + 6, 8}, Vec2{offset, 10}};
        const auto* first = document.findPolygon(document.addPolygon(firstPoints));
        const auto* second = document.findPolygon(document.addPolygon(secondPoints));
        const std::array lengths{FixedLengthConstraint{second->edges[0], 5}};
        const std::array parallels{ParallelConstraint{first->edges[0], second->edges[0]}};
        const std::array variables{second->vertices[1]};
        const auto errors = evaluateErrors(document, lengths, parallels);
        require(errors.has_value(), "Offset mixed residuals failed");
        const auto matrix = computeConstraintJacobian(document, lengths, parallels, variables, *errors, 10);
        // At this offset the representable X displacement is much larger than the requested step.
        requireMatrixNear(matrix, {{0.6, 0.8}, {-0.48, 0.36}}, 2e-5);
        require(document.findVertex(variables[0])->position.x == offset + 6, "Mixed fallback changed source");
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
            TestCase{"empty matrix dimensions", emptyDimensionsAreSupported},
            TestCase{"mixed rows: known derivatives, order and unrelated zeros", mixedRowsUseKnownDerivativesAndInputOrder},
            TestCase{"parallel-only Jacobian across four endpoints", parallelOnlyJacobianIncludesAllFourEndpoints},
            TestCase{"mixed Jacobian is invariant under uniform geometry scaling", mixedJacobianIsInvariantUnderUniformGeometryScaling},
            TestCase{"normalization scale affects only parallel rows", changingNormalizationScaleAffectsOnlyParallelRows},
            TestCase{"reversed parallel pairs have opposite derivative rows", reversedParallelPairsHaveOppositeDerivativeRows},
            TestCase{"mixed samples preserve source and raw inputs", mixedSamplesPreserveSourceAndRawInputs},
            TestCase{"invalid mixed Jacobian inputs", mixedJacobianRejectsInvalidInputs},
            TestCase{"collapsed parallel sample rejects without mutation", collapsedParallelSampleRejectsWithoutMutation},
            TestCase{"mixed Jacobian uses actual normalized displacement", mixedJacobianUsesActualNormalizedDisplacement}
        };
        return tests;
    }
}
