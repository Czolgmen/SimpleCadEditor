#include "../../cad_core/src/solver/SolverDetail.h"
#include "../CurveTestSupport.h"

#include <limits>
#include <numbers>

namespace
{
    using namespace cad;
    using namespace cad::solver::detail;
    using namespace cad::tests;
    using constraint::FixedLengthConstraint;
    using constraint::ParallelConstraint;

    struct Fixture
    {
        Document document;
        Polygon polygon;

        Fixture(bool incomingCurve = true, bool outgoingCurve = true, double scale = 1)
        {
            const std::array positions{Vec2{0, 0}, Vec2{6 * scale, 0},
                Vec2{6 * scale, 6 * scale}, Vec2{0, 6 * scale}};
            polygon = *document.findPolygon(document.addPolygon(positions));
            // Independent fixture: incoming derivative is (3s,0) or (6s,0),
            // outgoing derivative is (0,3s) or (0,6s). Opposite handles differ.
            if (incomingCurve)
                const_cast<Edge*>(document.findEdge(polygon.edges[0]))->geometry =
                    CubicBezier{{2 * scale, 3 * scale}, {5 * scale, 0}};
            if (outgoingCurve)
                const_cast<Edge*>(document.findEdge(polygon.edges[1]))->geometry =
                    CubicBezier{{6 * scale, scale}, {8 * scale, 4 * scale}};
        }

        ContinuityJoin join(Continuity type) const
        {
            return {polygon.vertices[1], polygon.edges[0], polygon.edges[1], type};
        }

        std::array<SolverPoint, 3> variables(bool incomingCurve = true, bool outgoingCurve = true) const
        {
            const SolverPoint incoming = incomingCurve
                ? SolverPoint{ControlPointRef{polygon.edges[0], BezierControlPoint::Second}}
                : SolverPoint{polygon.vertices[0]};
            const SolverPoint outgoing = outgoingCurve
                ? SolverPoint{ControlPointRef{polygon.edges[1], BezierControlPoint::First}}
                : SolverPoint{polygon.vertices[2]};
            return {incoming, SolverPoint{polygon.vertices[1]}, outgoing};
        }
    };

    void requireNear(double actual, double expected, double tolerance = 2e-6)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            "Continuity Jacobian differs from analytic derivative");
    }

    void requireMatrix(const std::optional<DenseMatrix>& matrix, const DenseMatrix& expected)
    {
        require(matrix && matrix->size() == expected.size(), "Wrong continuity Jacobian row count");
        for (std::size_t row = 0; row < expected.size(); ++row)
        {
            require((*matrix)[row].size() == expected[row].size(), "Wrong continuity Jacobian column count");
            for (std::size_t column = 0; column < expected[row].size(); ++column)
                requireNear((*matrix)[row][column], expected[row][column]);
        }
    }

    void g1DerivativesMatchEveryGeometryPair()
    {
        for (const bool incomingCurve : {false, true})
        for (const bool outgoingCurve : {false, true})
        {
            const Fixture f(incomingCurve, outgoingCurve);
            const std::array joins{f.join(Continuity::G1)};
            const auto variables = f.variables(incomingCurve, outgoingCurve);
            const auto errors = evaluateErrors(f.document, {}, {}, joins);
            require(errors && errors->size() == 1, "G1 base errors unavailable");
            requireNear((*errors)[0], std::numbers::pi_v<double> / 2);
            const double in = incomingCurve ? 6.0 : 1.0;
            const double out = outgoingCurve ? 6.0 : 1.0;
            requireMatrix(computePointJacobian(f.document, {}, {}, joins, variables, *errors, 6),
                {{0, in, out, -in, -out, 0}});
        }
    }

    void c1DerivativesMatchEveryGeometryPair()
    {
        for (const bool incomingCurve : {false, true})
        for (const bool outgoingCurve : {false, true})
        {
            const Fixture f(incomingCurve, outgoingCurve);
            const std::array joins{f.join(Continuity::C1)};
            const auto variables = f.variables(incomingCurve, outgoingCurve);
            const auto errors = evaluateErrors(f.document, {}, {}, joins);
            require(errors && errors->size() == 2, "C1 base errors unavailable");
            const double in = incomingCurve ? 3.0 : 1.0;
            const double out = outgoingCurve ? 3.0 : 1.0;
            requireMatrix(computePointJacobian(f.document, {}, {}, joins, variables, *errors, 6),
                {{in, 0, -(in + out), 0, out, 0}, {0, in, 0, -(in + out), 0, out}});
        }
    }

    void mixedRowsMatchMetadataRawErrorsAndAnalyticMatrix()
    {
        Fixture f;
        const std::array positions{Vec2{20, 0}, Vec2{23, 4}, Vec2{20, 5}};
        const EdgeId other = f.document.findPolygon(f.document.addPolygon(positions))->edges[0];
        const std::array lengths{FixedLengthConstraint{f.polygon.edges[2], 4}};
        const std::array parallels{ParallelConstraint{f.polygon.edges[2], other}};
        const std::array joins{f.join(Continuity::G0), f.join(Continuity::C1), f.join(Continuity::G1)};
        const auto points = f.variables();
        const std::array<SolverPoint, 4> variables{points[0], points[1], points[2], f.polygon.vertices[2]};
        const auto kinds = buildResidualKinds(1, 1, joins);
        const std::vector expectedKinds{ResidualKind::Length, ResidualKind::Parallel,
            ResidualKind::C1, ResidualKind::C1, ResidualKind::G1};
        require(kinds && *kinds == expectedKinds, "Row metadata lost order or G0 added a row");
        const auto errors = evaluateErrors(f.document, lengths, parallels, joins);
        const std::array expectedErrors{2.0, -0.8, -3.0, 3.0, std::numbers::pi_v<double> / 2};
        require(errors && errors->size() == expectedErrors.size(), "Mixed continuity errors have wrong size");
        for (std::size_t i = 0; i < expectedErrors.size(); ++i) requireNear((*errors)[i], expectedErrors[i]);
        const auto originalErrors = *errors;
        const Document before = f.document;
        requireMatrix(computePointJacobian(f.document, lengths, parallels, joins, variables, *errors, 6), {
            {0, 0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 0, 0, 0.6},
            {3, 0, -6, 0, 3, 0, 0, 0},
            {0, 3, 0, -6, 0, 3, 0, 0},
            {0, 6, 6, -6, -6, 0, 0, 0}});
        require(*errors == originalErrors, "Sampling changed raw base errors");
        requireSameCurveDocument(f.document, before);
    }

    void continuityRowsRespectGeometryAndNormalizationScales()
    {
        for (const double geometry : {1e-90, 1.0, 1e90})
        for (const double normalization : {1.0, 6.0, 60.0})
        {
            const Fixture f(true, true, geometry);
            const std::array joins{f.join(Continuity::G1), f.join(Continuity::C1)};
            const auto variables = f.variables();
            const auto errors = evaluateErrors(f.document, {}, {}, joins);
            require(errors.has_value(), "Scaled continuity errors unavailable");
            requireMatrix(computePointJacobian(f.document, {}, {}, joins, variables, *errors,
                geometry * normalization), {
                {0, normalization, normalization, -normalization, -normalization, 0},
                {3, 0, -6, 0, 3, 0},
                {0, 3, 0, -6, 0, 3}});
        }
    }

    void angularDifferencesWrapAcrossBothPiBranches()
    {
        constexpr double epsilon = 1e-9;
        for (const bool crossPositivePi : {false, true})
        {
            Fixture f;
            const double slope = crossPositivePi ? epsilon : -epsilon;
            require(f.document.moveControlPoint({f.polygon.edges[1], BezierControlPoint::First}, {5, slope}),
                "Angle branch fixture failed");
            const ControlPointRef variable = crossPositivePi
                ? ControlPointRef{f.polygon.edges[0], BezierControlPoint::Second}
                : ControlPointRef{f.polygon.edges[1], BezierControlPoint::First};
            const std::array<SolverPoint, 1> variables{variable};
            const std::array joins{f.join(Continuity::G1)};
            const auto errors = evaluateErrors(f.document, {}, {}, joins);
            require(errors && errors->size() == 1, "Angle branch errors unavailable");
            require(crossPositivePi ? (*errors)[0] > 3 : (*errors)[0] < -3, "Fixture is not near intended pi branch");
            const Document before = f.document;
            const auto matrix = computePointJacobian(f.document, {}, {}, joins, variables, *errors, 6);
            if (crossPositivePi) requireMatrix(matrix, {{0, 6}});
            else requireMatrix(matrix, {{6 * epsilon / (1 + epsilon * epsilon), -6 / (1 + epsilon * epsilon)}});
            requireSameCurveDocument(f.document, before);
        }
    }

    void invalidContinuityInputsRejectInsteadOfDroppingRows()
    {
        const Fixture f;
        const auto variables = f.variables();
        const std::array joins{f.join(Continuity::G1), f.join(Continuity::C1)};
        const auto errors = evaluateErrors(f.document, {}, {}, joins);
        require(errors.has_value(), "Invalid input fixture failed");
        require(!computePointJacobian(f.document, {}, {}, joins, variables,
            std::span<const double>{*errors}.first(2), 6), "Missing continuity row accepted");
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (std::size_t i = 0; i < errors->size(); ++i)
        {
            auto broken = *errors;
            broken[i] = value;
            require(!computePointJacobian(f.document, {}, {}, joins, variables, broken, 6), "Invalid raw error accepted");
        }
        for (const auto type : {static_cast<Continuity>(-1), static_cast<Continuity>(3)})
        {
            const std::array broken{f.join(type)};
            require(!buildResidualKinds(0, 0, broken), "Unknown continuity class has row metadata");
            require(!evaluateErrors(f.document, {}, {}, broken), "Unknown continuity class has errors");
            require(!computePointJacobian(f.document, {}, {}, broken, variables, *errors, 6), "Unknown class accepted");
        }
        auto broken = joins;
        broken[1].incoming = EdgeId{999999};
        require(!evaluateErrors(f.document, {}, {}, broken), "Invalid C1 join silently skipped");
        require(!computePointJacobian(f.document, {}, {}, broken, variables, *errors, 6), "Invalid join sampled");
    }

    void zeroDirectionInSampleRejectsWithoutMutation()
    {
        Fixture f;
        const double step = std::sqrt(std::numeric_limits<double>::epsilon());
        const ControlPointRef control{f.polygon.edges[0], BezierControlPoint::Second};
        require(f.document.moveControlPoint(control, {6 - step, 0}), "Small derivative fixture failed");
        const std::array joins{f.join(Continuity::G1)};
        const auto errors = evaluateErrors(f.document, {}, {}, joins);
        require(errors.has_value(), "Nonzero initial direction rejected");
        const std::array<SolverPoint, 1> variables{control};
        const Document before = f.document;
        require(!computePointJacobian(f.document, {}, {}, joins, variables, *errors, 1),
            "Sample with zero G1 direction accepted");
        requireSameCurveDocument(f.document, before);
    }

    // Deliberately unoptimized reference: evaluate every row on a fresh document
    // for every coordinate, without using the production dependency table.
    std::optional<DenseMatrix> fullSamplingReference(
        const Document& document,
        std::span<const FixedLengthConstraint> lengths,
        std::span<const ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins,
        std::span<const SolverPoint> variables,
        std::span<const double> baseErrors,
        double scale)
    {
        const auto kinds = buildResidualKinds(lengths.size(), parallels.size(), joins);
        if (!kinds || kinds->size() != baseErrors.size()) return std::nullopt;
        DenseMatrix matrix(baseErrors.size(), std::vector<double>(variables.size() * 2));
        const double step = std::sqrt(std::numeric_limits<double>::epsilon()) * scale;
        for (std::size_t point = 0; point < variables.size(); ++point)
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const auto original = getPointPosition(document, variables[point]);
            if (!original) return std::nullopt;
            Vec2 shifted = *original;
            double& coordinate = axis == 0 ? shifted.x : shifted.y;
            const double before = coordinate;
            coordinate += step;
            if (coordinate == before)
                coordinate = std::nextafter(before, std::numeric_limits<double>::infinity());
            const double actualStep = coordinate - before;
            Document probe = document;
            if (!setPointPosition(probe, variables[point], shifted)) return std::nullopt;
            const auto errors = evaluateErrors(probe, lengths, parallels, joins);
            if (!errors) return std::nullopt;
            for (std::size_t row = 0; row < baseErrors.size(); ++row)
            {
                double difference = (*errors)[row] - baseErrors[row];
                if ((*kinds)[row] == ResidualKind::G1)
                    difference = std::remainder(difference, 2 * std::numbers::pi_v<double>);
                const bool dimensional = (*kinds)[row] == ResidualKind::Length || (*kinds)[row] == ResidualKind::C1;
                matrix[row][2 * point + axis] = difference / (dimensional ? actualStep : actualStep / scale);
            }
        }
        return matrix;
    }

    void dependencySamplingMatchesFullMixedReference()
    {
        for (const double size : {1e-6, 1.0, 1e6})
        {
            Fixture f(true, true, size);
            const std::array positions{Vec2{30 * size, 0}, Vec2{34 * size, 3 * size}, Vec2{34 * size, 9 * size}};
            const Polygon other = *f.document.findPolygon(f.document.addPolygon(positions));
            const std::array lengths{FixedLengthConstraint{f.polygon.edges[2], 5 * size},
                FixedLengthConstraint{f.polygon.edges[3], 7 * size}, FixedLengthConstraint{other.edges[0], 4 * size}};
            const std::array parallels{ParallelConstraint{f.polygon.edges[2], other.edges[0]},
                ParallelConstraint{f.polygon.edges[3], other.edges[1]}};
            const std::array joins{f.join(Continuity::G1),
                ContinuityJoin{f.polygon.vertices[2], f.polygon.edges[1], f.polygon.edges[2], Continuity::C1},
                f.join(Continuity::G0)};
            std::vector<SolverPoint> variables;
            for (const auto vertex : f.polygon.vertices) variables.emplace_back(vertex);
            for (const auto vertex : other.vertices) variables.emplace_back(vertex);
            for (const auto edge : {f.polygon.edges[0], f.polygon.edges[1]})
            for (const auto point : {BezierControlPoint::First, BezierControlPoint::Second})
                variables.emplace_back(ControlPointRef{edge, point});
            variables.emplace_back(f.polygon.vertices[1]); // Repeated column, after other perturbations.
            const auto errors = evaluateErrors(f.document, lengths, parallels, joins);
            require(errors.has_value(), "Dependency fixture errors unavailable");
            const auto expected = fullSamplingReference(f.document, lengths, parallels, joins, variables, *errors, 6 * size);
            require(expected.has_value(), "Full reference sampling failed");
            const Document before = f.document;
            requireMatrix(computePointJacobian(f.document, lengths, parallels, joins, variables, *errors, 6 * size), *expected);
            requireSameCurveDocument(f.document, before);
        }
    }

    void unrelatedInvalidResidualsStillRejectSampling()
    {
        Fixture f;
        const std::array positions{Vec2{30, 0}, Vec2{36, 0}, Vec2{30, 6}};
        const auto id = f.document.findPolygon(f.document.addPolygon(positions))->vertices[0];
        const std::array<SolverPoint, 1> variables{id};
        const std::array baseErrors{0.0};
        auto join = f.join(Continuity::G1);
        join.outgoing = EdgeId{999999};
        require(!computePointJacobian(f.document, {}, {}, std::span{&join, 1}, variables, baseErrors, 6),
            "Unrelated malformed relation was skipped");
        join = f.join(Continuity::G1);
        require(f.document.moveControlPoint({f.polygon.edges[0], BezierControlPoint::Second}, {6, 0}),
            "Unrelated zero tangent fixture failed");
        const Document before = f.document;
        require(!computePointJacobian(f.document, {}, {}, std::span{&join, 1}, variables, baseErrors, 6),
            "Unrelated degenerate G1 was skipped");
        requireSameCurveDocument(f.document, before);
    }

    void staleBaseErrorsRetainFullSamplingBehavior()
    {
        const Fixture f;
        const std::array joins{f.join(Continuity::C1), f.join(Continuity::G1)};
        // This opposite handle does not affect either join. A stale base vector
        // must still produce the same finite differences as full sampling.
        const std::array<SolverPoint, 1> variables{
            ControlPointRef{f.polygon.edges[0], BezierControlPoint::First}};
        auto errors = evaluateErrors(f.document, {}, {}, joins);
        require(errors.has_value(), "Stale error fixture failed");
        (*errors)[0] += 0.25;
        const auto expected = fullSamplingReference(f.document, {}, {}, joins, variables, *errors, 6);
        require(expected.has_value(), "Stale reference sampling failed");
        requireMatrix(computePointJacobian(f.document, {}, {}, joins, variables, *errors, 6), *expected);
    }

    void g0AndZeroColumnMatricesKeepCorrectDimensions()
    {
        const Fixture f;
        const auto variables = f.variables();
        const std::array g0{f.join(Continuity::G0)};
        const auto empty = computePointJacobian(f.document, {}, {}, g0, variables, {}, 6);
        require(empty && empty->empty(), "G0 introduced Jacobian rows");
        const std::array joins{f.join(Continuity::C1), f.join(Continuity::G1)};
        const auto errors = evaluateErrors(f.document, {}, {}, joins);
        require(errors.has_value(), "Zero-column errors unavailable");
        requireMatrix(computePointJacobian(f.document, {}, {}, joins, {}, *errors, 6), {{}, {}, {}});
    }
}

namespace cad::tests
{
    std::span<const TestCase> continuityJacobianTests()
    {
        static const std::array tests{
            TestCase{"G1 analytic derivatives for every geometry pair", g1DerivativesMatchEveryGeometryPair},
            TestCase{"C1 analytic derivatives for every geometry pair", c1DerivativesMatchEveryGeometryPair},
            TestCase{"mixed rows match metadata, raw errors and analytic matrix", mixedRowsMatchMetadataRawErrorsAndAnalyticMatrix},
            TestCase{"continuity rows respect geometry and normalization scales", continuityRowsRespectGeometryAndNormalizationScales},
            TestCase{"angular differences wrap across both pi branches", angularDifferencesWrapAcrossBothPiBranches},
            TestCase{"invalid continuity inputs reject instead of dropping rows", invalidContinuityInputsRejectInsteadOfDroppingRows},
            TestCase{"zero direction in sample rejects without mutation", zeroDirectionInSampleRejectsWithoutMutation},
            TestCase{"G0 and zero-column matrices retain dimensions", g0AndZeroColumnMatricesKeepCorrectDimensions},
            TestCase{"dependency sampling matches full mixed reference", dependencySamplingMatchesFullMixedReference},
            TestCase{"unrelated invalid residuals still reject sampling", unrelatedInvalidResidualsStillRejectSampling},
            TestCase{"stale base errors retain full sampling behavior", staleBaseErrorsRetainFullSamplingBehavior}
        };
        return tests;
    }
}
