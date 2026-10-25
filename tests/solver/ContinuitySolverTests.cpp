#include "../../cad_core/src/solver/SolverDetail.h"
#include "../CurveTestSupport.h"
#include <cad_core/constraints/ContinuityConstraint.h>

#include <limits>
#include <string>

namespace
{
    using namespace cad;
    using namespace cad::solver;
    using namespace cad::solver::detail;
    using namespace cad::tests;
    using constraint::FixedLengthConstraint;
    using constraint::ParallelConstraint;

    struct Fixture
    {
        Document document;
        Polygon polygon;

        Fixture(bool incomingCurve = true, bool outgoingCurve = true, double size = 1, Vec2 offset = {})
        {
            const auto p = [&](double x, double y) { return Vec2{x * size + offset.x, y * size + offset.y}; };
            const std::array positions{p(0, 0), p(10, 0), p(10, 10), p(0, 10)};
            polygon = *document.findPolygon(document.addPolygon(positions));
            if (incomingCurve)
                edge(0).geometry = CubicBezier{p(2, 4), p(8, -1)};
            if (outgoingCurve)
                edge(1).geometry = CubicBezier{p(12, 3), p(9, 10)};
        }

        Edge& edge(std::size_t index)
        {
            return *const_cast<Edge*>(document.findEdge(polygon.edges[index]));
        }

        void activate(Continuity type, std::size_t vertex = 1)
        {
            require(document.setContinuity(polygon.vertices[vertex], type), "Solver continuity fixture failed");
        }

        std::array<SolverPoint, 3> pins(bool incomingCurve = true, bool outgoingCurve = true) const
        {
            return {SolverPoint{polygon.vertices[1]},
                incomingCurve ? SolverPoint{ControlPointRef{polygon.edges[0], BezierControlPoint::Second}}
                    : SolverPoint{polygon.vertices[0]},
                outgoingCurve ? SolverPoint{ControlPointRef{polygon.edges[1], BezierControlPoint::First}}
                    : SolverPoint{polygon.vertices[2]}};
        }
    };

    void requireSolved(const Document& document, SolveResult result, const SolverPoint& fixed, Vec2 position,
        const SolveOptions& options = {}, std::span<const FixedLengthConstraint> lengths = {},
        std::span<const ParallelConstraint> parallels = {})
    {
        require(result.status == SolveStatus::Converged,
            "Point solver did not converge: status=" + std::to_string(static_cast<int>(result.status)) +
            ", iterations=" + std::to_string(result.iterations));
        require(result.iterations <= options.maxIterations, "Point solver exceeded iteration budget");
        require(document.validate().empty(), "Point solver invalidated topology");
        const auto pinned = getPointPosition(document, fixed);
        require(pinned && pinned->x == position.x && pinned->y == position.y, "Pinned point changed");
        for (const auto& length : lengths)
        {
            const auto error = constraint::fixedLengthError(document, length);
            require(error && std::abs(*error) <= options.lengthTolerance, "Converged with unsatisfied length");
        }
        for (const auto& parallel : parallels)
        {
            const auto error = constraint::parallelError(document, parallel);
            require(error && std::abs(*error) <= options.parallelTolerance, "Converged with unsatisfied parallel relation");
        }
        const auto joins = collectContinuityJoins(document);
        require(joins.has_value(), "Solved continuity topology invalid");
        for (const auto& join : *joins)
        {
            const auto errors = constraint::continuityErrors(document, join);
            require(errors.has_value(), "Converged with unusable tangent");
            const double tolerance = join.continuity == Continuity::G1 ? options.g1Tolerance : options.c1Tolerance;
            for (const double error : *errors)
                require(std::isfinite(error) && std::abs(error) <= tolerance, "Converged with unsatisfied continuity");
        }
    }

    void alreadySatisfiedAndInactiveDocumentsNeedNoIterations()
    {
        Fixture inactive;
        const SolverPoint pin = inactive.polygon.vertices[1];
        const Vec2 position = *getPointPosition(inactive.document, pin);
        const Document before = inactive.document;
        SolveOptions options;
        options.maxIterations = 0;
        const auto result = solvePointConstraintSystem(inactive.document, {}, {}, pin, options);
        requireSolved(inactive.document, result, pin, position, options);
        require(result.iterations == 0, "Inactive solve used iterations");
        requireSameCurveDocument(inactive.document, before);

        for (const auto type : {Continuity::G1, Continuity::C1})
        {
            Fixture f;
            std::get<CubicBezier>(f.edge(0).geometry).control2 = {9, 0};
            std::get<CubicBezier>(f.edge(1).geometry).control1 = {11, 0};
            f.activate(type);
            const SolverPoint control = ControlPointRef{f.polygon.edges[0], BezierControlPoint::Second};
            const Document snapshot = f.document;
            const auto solved = solvePointConstraintSystem(f.document, {}, {}, control, options);
            requireSolved(f.document, solved, control, {9, 0}, options);
            require(solved.iterations == 0, "Satisfied continuity used iterations");
            requireSameCurveDocument(f.document, snapshot);
        }
    }

    void g1ConvergesForEveryCurvePairAndPin()
    {
        for (const bool incoming : {false, true})
        for (const bool outgoing : {false, true})
        {
            if (!incoming && !outgoing) continue;
            for (std::size_t i = 0; i < 3; ++i)
            {
                Fixture f(incoming, outgoing);
                f.activate(Continuity::G1);
                const auto pin = f.pins(incoming, outgoing)[i];
                const Vec2 position = *getPointPosition(f.document, pin);
                const auto result = solvePointConstraintSystem(f.document, {}, {}, pin);
                requireSolved(f.document, result, pin, position);
                require(result.iterations > 0, "Unsatisfied G1 used no corrections");
            }
        }
    }

    void c1ConvergesForEveryCurvePairAndPin()
    {
        for (const bool incoming : {false, true})
        for (const bool outgoing : {false, true})
        {
            if (!incoming && !outgoing) continue;
            for (std::size_t i = 0; i < 3; ++i)
            {
                Fixture f(incoming, outgoing);
                f.activate(Continuity::C1);
                const auto pin = f.pins(incoming, outgoing)[i];
                const Vec2 position = *getPointPosition(f.document, pin);
                const auto result = solvePointConstraintSystem(f.document, {}, {}, pin);
                requireSolved(f.document, result, pin, position);
                require(result.iterations > 0, "Unsatisfied C1 used no corrections");
            }
        }
    }

    void adjacentG1AndC1JoinsAreSolvedTogether()
    {
        Fixture f;
        f.activate(Continuity::G1);
        f.activate(Continuity::C1, 2);
        const SolverPoint pin = ControlPointRef{f.polygon.edges[1], BezierControlPoint::First};
        const Vec2 position = *getPointPosition(f.document, pin);
        const auto result = solvePointConstraintSystem(f.document, {}, {}, pin);
        requireSolved(f.document, result, pin, position);
    }

    void lengthsParallelsAndContinuityAreSolvedTogether()
    {
        Fixture f(false, true);
        f.activate(Continuity::C1);
        f.activate(Continuity::G1, 2);
        for (const auto index : {0u, 2u, 3u})
            require(f.document.setFixedLength(f.polygon.edges[index], 10), "Mixed length fixture failed");
        require(f.document.addParallel(f.polygon.edges[0], f.polygon.edges[2]), "Mixed parallel fixture failed");
        const SolverPoint pin = f.polygon.vertices[0];
        const Vec2 position = *getPointPosition(f.document, pin);
        // The solver receives spans owned by the document it will eventually commit.
        const auto result = solvePointConstraintSystem(f.document, f.document.getFixedLengthConstraints(),
            f.document.getParallelConstraints(), pin);
        requireSolved(f.document, result, pin, position, {}, f.document.getFixedLengthConstraints(),
            f.document.getParallelConstraints());
    }

    void perpendicularParallelsPreserveContinuity()
    {
        Fixture f;
        std::get<CubicBezier>(f.edge(0).geometry).control2 = {9, 0};
        std::get<CubicBezier>(f.edge(1).geometry).control1 = {11, 0};
        f.activate(Continuity::C1);
        const std::array a{Vec2{30, 0}, Vec2{40, 0}, Vec2{30, 10}};
        const std::array b{Vec2{60, 0}, Vec2{60, 10}, Vec2{50, 0}};
        const Polygon first = *f.document.findPolygon(f.document.addPolygon(a));
        const Polygon second = *f.document.findPolygon(f.document.addPolygon(b));
        const std::array lengths{FixedLengthConstraint{first.edges[0], 10}, FixedLengthConstraint{second.edges[0], 10}};
        const std::array parallels{ParallelConstraint{first.edges[0], second.edges[0]}};
        const SolverPoint pin = first.vertices[0];
        const auto result = solvePointConstraintSystem(f.document, lengths, parallels, pin);
        requireSolved(f.document, result, pin, {30, 0}, {}, lengths, parallels);
    }

    void conflictingRelationsRollbackAllState()
    {
        Fixture f;
        f.activate(Continuity::C1);
        const Document before = f.document;
        const std::array lengths{FixedLengthConstraint{f.polygon.edges[2], 10},
            FixedLengthConstraint{f.polygon.edges[2], 20}};
        const auto result = solvePointConstraintSystem(f.document, lengths, {}, f.pins()[0]);
        require(result.status == SolveStatus::NoConvergence, "Conflicting lengths reported success");
        require(result.iterations <= SolveOptions{}.maxIterations, "Conflict exceeded budget");
        requireSameCurveDocument(f.document, before);
    }

    void iterationBudgetsRollbackAcceptedPreviews()
    {
        for (const unsigned budget : {0u, 1u})
        {
            Fixture f;
            f.activate(Continuity::G1);
            const Document before = f.document;
            SolveOptions options;
            options.maxIterations = budget;
            options.g1Tolerance = 1e-12;
            const auto result = solvePointConstraintSystem(f.document, {}, {}, f.pins()[0], options);
            require(result.status == SolveStatus::NoConvergence && result.iterations == budget,
                "Iteration limit did not stop the point solver");
            requireSameCurveDocument(f.document, before);
        }
    }

    void invalidTolerancesRejectWithoutMutation()
    {
        Fixture f;
        f.activate(Continuity::G1);
        const Document before = f.document;
        for (int field = 0; field < 4; ++field)
        for (const double value : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            SolveOptions options;
            if (field == 0) options.lengthTolerance = value;
            if (field == 1) options.parallelTolerance = value;
            if (field == 2) options.g1Tolerance = value;
            if (field == 3) options.c1Tolerance = value;
            const auto result = solvePointConstraintSystem(f.document, {}, {}, f.pins()[0], options);
            require(result.status == SolveStatus::InvalidInput && result.iterations == 0, "Invalid tolerance accepted");
            requireSameCurveDocument(f.document, before);
        }
    }

    void invalidPinsRejectWithoutMutation()
    {
        Fixture f;
        f.activate(Continuity::C1);
        const Document before = f.document;
        const std::array<SolverPoint, 5> pins{VertexId{}, VertexId{999999},
            ControlPointRef{EdgeId{999999}, BezierControlPoint::First},
            ControlPointRef{f.polygon.edges[2], BezierControlPoint::First},
            ControlPointRef{f.polygon.edges[0], static_cast<BezierControlPoint>(999)}};
        for (const auto& pin : pins)
        {
            const auto result = solvePointConstraintSystem(f.document, {}, {}, pin);
            require(result.status == SolveStatus::InvalidInput && result.iterations == 0, "Invalid pin accepted");
            requireSameCurveDocument(f.document, before);
        }
    }

    void invalidTopologyAndHandleDataReject()
    {
        for (const bool brokenTopology : {false, true})
        {
            Fixture f;
            f.activate(Continuity::G1);
            if (brokenTopology) f.edge(0).end = VertexId{999999};
            else std::get<CubicBezier>(f.edge(0).geometry).control2.x = std::numeric_limits<double>::quiet_NaN();
            const Document before = f.document;
            const auto result = solvePointConstraintSystem(f.document, {}, {}, f.pins()[0]);
            require(result.status == SolveStatus::InvalidInput && result.iterations == 0, "Broken geometry accepted");
            requireSameCurveDocument(f.document, before);
        }
    }

    void zeroTangentsRejectG1ButPermitSatisfiedC1()
    {
        for (const auto type : {Continuity::G1, Continuity::C1})
        {
            Fixture f;
            std::get<CubicBezier>(f.edge(0).geometry).control2 = {10, 0};
            std::get<CubicBezier>(f.edge(1).geometry).control1 = {10, 0};
            f.activate(type);
            const Document before = f.document;
            const auto result = solvePointConstraintSystem(f.document, {}, {}, f.pins()[0]);
            require(result.status == (type == Continuity::G1 ? SolveStatus::InvalidInput : SolveStatus::Converged)
                && result.iterations == 0, "Wrong zero-tangent behavior");
            requireSameCurveDocument(f.document, before);
        }
    }

    void scaledAndTranslatedContinuityConverges()
    {
        for (const auto type : {Continuity::G1, Continuity::C1})
        for (const double size : {1e-90, 1.0, 1e90})
        {
            Fixture f(true, true, size, {1000 * size, -300 * size});
            f.activate(type);
            const auto pin = f.pins()[0];
            const Vec2 position = *getPointPosition(f.document, pin);
            SolveOptions options;
            options.lengthTolerance *= size;
            options.c1Tolerance *= size;
            const auto result = solvePointConstraintSystem(f.document, {}, {}, pin, options);
            requireSolved(f.document, result, pin, position, options);
        }
    }

    void successKeepsUnrelatedPointsAndMetadata()
    {
        Fixture f;
        f.activate(Continuity::C1);
        const std::array positions{Vec2{100, 0}, Vec2{110, 0}, Vec2{110, 10}};
        const Polygon other = *f.document.findPolygon(f.document.addPolygon(positions));
        require(f.document.setFixedLength(other.edges[0], 10) &&
            f.document.addParallel(other.edges[0], f.polygon.edges[2]), "Unrelated relation fixture failed");
        Document expected = f.document;
        const auto pin = f.pins()[0];
        const auto result = solvePointConstraintSystem(f.document, {}, {}, pin);
        requireSolved(f.document, result, pin, {10, 0});
        // With the joint pinned, these two tangent handles are the only unknowns.
        std::get<CubicBezier>(const_cast<Edge*>(expected.findEdge(f.polygon.edges[0]))->geometry).control2 =
            std::get<CubicBezier>(f.document.findEdge(f.polygon.edges[0])->geometry).control2;
        std::get<CubicBezier>(const_cast<Edge*>(expected.findEdge(f.polygon.edges[1]))->geometry).control1 =
            std::get<CubicBezier>(f.document.findEdge(f.polygon.edges[1])->geometry).control1;
        requireSameCurveDocument(f.document, expected);
    }
}

namespace cad::tests
{
    std::span<const TestCase> continuitySolverTests()
    {
        static const std::array tests{
            TestCase{"satisfied and inactive documents need no iterations", alreadySatisfiedAndInactiveDocumentsNeedNoIterations},
            TestCase{"G1 converges for every curve pair and pin", g1ConvergesForEveryCurvePairAndPin},
            TestCase{"C1 converges for every curve pair and pin", c1ConvergesForEveryCurvePairAndPin},
            TestCase{"adjacent G1 and C1 joins solve together", adjacentG1AndC1JoinsAreSolvedTogether},
            TestCase{"lengths, parallels and continuity solve together", lengthsParallelsAndContinuityAreSolvedTogether},
            TestCase{"perpendicular parallels preserve continuity", perpendicularParallelsPreserveContinuity},
            TestCase{"conflicting relations roll back all state", conflictingRelationsRollbackAllState},
            TestCase{"iteration budgets roll back accepted previews", iterationBudgetsRollbackAcceptedPreviews},
            TestCase{"invalid tolerances reject without mutation", invalidTolerancesRejectWithoutMutation},
            TestCase{"invalid pins reject without mutation", invalidPinsRejectWithoutMutation},
            TestCase{"invalid topology and handle data reject", invalidTopologyAndHandleDataReject},
            TestCase{"zero tangents reject G1 and permit satisfied C1", zeroTangentsRejectG1ButPermitSatisfiedC1},
            TestCase{"scaled and translated continuity converges", scaledAndTranslatedContinuityConverges},
            TestCase{"success keeps unrelated points and metadata", successKeepsUnrelatedPointsAndMetadata}
        };
        return tests;
    }
}
