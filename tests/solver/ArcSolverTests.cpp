#include "../../cad_core/src/solver/SolverDetail.h"
#include "../CurveTestSupport.h"

#include <cad_core/model/ArcContinuity.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include <editor/EditorSession.h>

#include <algorithm>
#include <limits>
#include <numbers>

namespace
{
    using namespace cad;
    using namespace cad::solver;
    using namespace cad::solver::detail;
    using namespace cad::tests;
    using cad::editor::EditorSession;

    struct Fixture
    {
        Document document;
        Polygon polygon;
        EdgeId arc;
        EdgeId neighbor;

        Fixture(bool incomingArc = true, bool bezier = false, double size = 1, Vec2 offset = {})
        {
            const auto p = [&](double x, double y) { return Vec2{x, y} * size + offset; };
            const std::array points{p(0, 0), p(10, 0), p(10, 10), p(0, 10)};
            polygon = *document.findPolygon(document.addPolygon(points));
            arc = polygon.edges[incomingArc ? 0 : 1];
            neighbor = polygon.edges[incomingArc ? 1 : 0];
            edge(arc).geometry = CircularArc{-2};
            if (bezier)
                edge(neighbor).geometry = incomingArc
                    ? CubicBezier{p(12, 3), p(11, 8)}
                    : CubicBezier{p(2, -1), p(7, -2)};
            require(document.setContinuity(polygon.vertices[1], Continuity::G1), "Arc fixture setting failed");
        }

        Edge& edge(EdgeId id) { return *const_cast<Edge*>(document.findEdge(id)); }
        Vertex& vertex(std::size_t i) { return *const_cast<Vertex*>(document.findVertex(polygon.vertices[i])); }
        double bulge() const { return std::get<CircularArc>(document.findEdge(arc)->geometry).bulge; }
        ControlPointRef control(bool incomingArc = true) const
        {
            return {neighbor, incomingArc ? BezierControlPoint::First : BezierControlPoint::Second};
        }
    };

    void requireNear(double actual, double expected, double tolerance = 1e-6)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            "Unexpected arc solver value");
    }

    void requireSameState(const Document& actual, const Document& before)
    {
        requireSameCurveDocument(actual, before);
        Document a = actual, b = before;
        const std::array points{Vec2{200, 200}, Vec2{220, 200}, Vec2{200, 220}};
        require(a.addPolygon(points) == b.addPolygon(points), "Solver consumed IDs");
        requireSameCurveDocument(a, b);
    }

    void requireOnlyBulgeChanged(const Document& actual, Document expected, EdgeId arc)
    {
        const_cast<Edge*>(expected.findEdge(arc))->geometry = actual.findEdge(arc)->geometry;
        requireSameState(actual, expected);
    }

    void requireSatisfied(const Document& document, const SolveOptions& options = {})
    {
        require(document.validate().empty(), "Solved arc document invalid");
        for (const auto& relation : document.getFixedLengthConstraints())
        {
            const auto error = constraint::fixedLengthError(document, relation);
            require(error && std::abs(*error) <= options.lengthTolerance, "Arc solve left an unsatisfied length");
        }
        for (const auto& relation : document.getParallelConstraints())
        {
            const auto error = constraint::parallelError(document, relation);
            require(error && std::abs(*error) <= options.parallelTolerance, "Arc solve left unsatisfied parallelism");
        }
        const auto joins = collectContinuityJoins(document);
        require(joins.has_value(), "Solved joins unavailable");
        for (const auto& join : *joins)
        {
            const auto errors = constraint::continuityErrors(document, join);
            require(errors.has_value(), "Solved continuity invalid");
            for (const double error : *errors)
                requireNear(error, 0, join.continuity == Continuity::G1 ? options.g1Tolerance : options.c1Tolerance);
        }
    }

    void collectorIncludesArcsAndRejectsC1()
    {
        for (const bool incoming : {false, true})
        for (const bool bezier : {false, true})
        {
            Fixture f(incoming, bezier);
            const auto joins = collectContinuityJoins(f.document);
            require(joins && joins->size() == 1 && joins->front().vertex == f.polygon.vertices[1] &&
                joins->front().incoming == f.polygon.edges[0] && joins->front().outgoing == f.polygon.edges[1],
                "Collector lost arc join or changed its orientation");
            f.vertex(1).continuity.value = Continuity::C1;
            require(!collectContinuityJoins(f.document), "Collector accepted arc C1");
            f.vertex(1).continuity.value = Continuity::G0;
            const auto inactive = collectContinuityJoins(f.document);
            require(inactive && inactive->empty(), "G0 arc introduced equations");
        }
    }

    void analyticFittingCommitsWithZeroCoordinateBudget()
    {
        for (const bool incoming : {false, true})
        for (const bool bezier : {false, true})
        {
            Fixture f(incoming, bezier);
            const Document before = f.document;
            SolveOptions options;
            options.maxIterations = 0;
            const auto result = solveConstraints(f.document, {}, {}, f.polygon.vertices[1], options);
            require(result.status == SolveStatus::Converged && result.iterations == 0, "Analytic fit used coordinate iterations");
            requireNear(f.bulge(), bezier ? (std::sqrt(13.0) - 2) / 3 : 1, 1e-12);
            requireOnlyBulgeChanged(f.document, before, f.arc);
            requireSatisfied(f.document);
        }
    }

    void g0NormalizationRunsBeforeValidation()
    {
        for (const double old : {-8.0, -0.5, 0.5, 8.0})
        {
            Fixture f;
            f.vertex(1).continuity.value = Continuity::G0;
            std::get<CircularArc>(f.edge(f.arc).geometry).bulge = old;
            require(!f.document.validate().empty(), "G0 fixture already normalized");
            const Document before = f.document;
            const auto result = solveFixedLengths(f.document, {}, f.polygon.vertices[0]);
            require(result.status == SolveStatus::Converged && result.iterations == 0, "G0 normalization rejected");
            require(f.bulge() == std::copysign(1.0, old), "G0 normalization lost sign");
            requireOnlyBulgeChanged(f.document, before, f.arc);
        }
    }

    void publicHandleSolveFitsArcWithoutMovingEndpoints()
    {
        for (const bool incoming : {false, true})
        {
            Fixture f(incoming, true);
            const auto pin = f.control(incoming);
            const Document before = f.document;
            const auto position = getPointPosition(f.document, SolverPoint{pin});
            const auto result = solveConstraints(f.document, f.document.getFixedLengthConstraints(),
                f.document.getParallelConstraints(), pin);
            require(result.status == SolveStatus::Converged && result.iterations == 0, "Pinned handle arc solve failed");
            require(getPointPosition(f.document, SolverPoint{pin}) == position, "Arc solve moved pinned handle");
            requireOnlyBulgeChanged(f.document, before, f.arc);
            requireSatisfied(f.document);
        }
    }

    struct MixedFixture : Fixture
    {
        Polygon other;
        explicit MixedFixture(double size = 1, Vec2 offset = {}) : Fixture(true, false, size, offset)
        {
            const auto p = [&](double x, double y) { return Vec2{x, y} * size + offset; };
            vertex(2).position = p(13, 9);
            vertex(3).position = p(0, 12);
            edge(polygon.edges[2]).geometry = CubicBezier{p(8, 15), p(-2, 15)};
            require(document.setContinuity(polygon.vertices[3], Continuity::C1), "Mixed C1 fixture failed");
            const std::array points{p(30, 0), p(40, 2), p(34, 12)};
            other = *document.findPolygon(document.addPolygon(points));
            require(document.setFixedLength(polygon.edges[1], 8 * size) &&
                document.setFixedLength(polygon.edges[3], 10 * size) &&
                document.setFixedLength(other.edges[0], 7 * size) &&
                document.addParallel(polygon.edges[1], other.edges[0]), "Mixed arc relations failed");
        }
    };

    void mixedLengthsParallelsC1AndArcConverge()
    {
        MixedFixture f;
        const auto fixed = f.vertex(1).position;
        const auto unrelated = f.document.findVertex(f.other.vertices[2])->position;
        const auto result = solveConstraints(f.document, f.document.getFixedLengthConstraints(),
            f.document.getParallelConstraints(), f.polygon.vertices[1]);
        require(result.status == SolveStatus::Converged && result.iterations > 0, "Mixed arc solve failed");
        require(f.vertex(1).position == fixed, "Mixed arc solve moved cursor pin");
        require(f.document.findVertex(f.other.vertices[2])->position == unrelated, "Unrelated vertex moved");
        requireSatisfied(f.document);
    }

    void rejectedBudgetsAndConflictsKeepOriginalBulges()
    {
        for (const unsigned budget : {0u, 1u})
        {
            MixedFixture f;
            const Document before = f.document;
            SolveOptions options;
            options.maxIterations = budget;
            const auto result = solveConstraints(f.document, f.document.getFixedLengthConstraints(),
                f.document.getParallelConstraints(), f.polygon.vertices[1], options);
            require(result.status == SolveStatus::NoConvergence, "Insufficient arc solve budget accepted");
            requireSameState(f.document, before);
        }
        Fixture f;
        const std::array lengths{constraint::FixedLengthConstraint{f.neighbor, 8},
            constraint::FixedLengthConstraint{f.neighbor, 11}};
        const Document before = f.document;
        const auto result = solveConstraints(f.document, lengths, {}, f.polygon.vertices[1]);
        require(result.status != SolveStatus::Converged, "Conflicting arc solve accepted");
        requireSameState(f.document, before);
    }

    void unconstructableArcFitsRejectWithoutMutation()
    {
        for (const bool zero : {false, true})
        {
            Fixture f(true, true);
            std::get<CubicBezier>(f.edge(f.neighbor).geometry).control1 = zero ? Vec2{10, 0} : Vec2{15, 0};
            const Document before = f.document;
            const auto result = solveConstraints(f.document, {}, {}, f.control());
            require(result.status == SolveStatus::InvalidInput, "Undefined arc fit accepted");
            requireSameState(f.document, before);
        }
    }

    void invalidArcSettingsAndPinsKeepDocument()
    {
        for (int kind = 0; kind < 5; ++kind)
        {
            Fixture f;
            VertexId pin = f.polygon.vertices[1];
            if (kind == 0) f.vertex(1).continuity.value = Continuity::C1;
            if (kind == 1) f.vertex(0).continuity.value = Continuity::G1;
            if (kind == 2) std::get<CircularArc>(f.edge(f.arc).geometry).bulge = 0;
            if (kind == 3) pin = VertexId{99999};
            if (kind == 4) f.edge(f.arc).start = VertexId{99999};
            const Document before = f.document;
            require(solveConstraints(f.document, {}, {}, pin).status == SolveStatus::InvalidInput, "Invalid arc input accepted");
            requireSameState(f.document, before);
        }
    }

    void arcPairsKeepReferenceAndClosingJoin()
    {
        for (const std::size_t shared : {0u, 1u})
        {
            Fixture f;
            f.vertex(1).continuity.value = Continuity::G0;
            f.vertex(shared).continuity.value = Continuity::G1;
            const EdgeId incoming = f.polygon.edges[(shared + 3) % 4], outgoing = f.polygon.edges[shared];
            const EdgeId kept = (std::min)(incoming, outgoing), fitted = (std::max)(incoming, outgoing);
            f.edge(kept).geometry = CircularArc{0.5};
            f.edge(fitted).geometry = CircularArc{-8};
            const Document before = f.document;
            const auto joins = collectContinuityJoins(f.document);
            require(joins && joins->size() == 1 && joins->front().vertex == f.polygon.vertices[shared], "Arc pair join duplicated or lost");
            const auto result = solveConstraints(f.document, {}, {}, f.polygon.vertices[shared]);
            require(result.status == SolveStatus::Converged && result.iterations == 0, "Arc pair solve failed");
            requireNear(std::get<CircularArc>(f.document.findEdge(fitted)->geometry).bulge, 1.0 / 3, 1e-12);
            requireOnlyBulgeChanged(f.document, before, fitted);
            requireSatisfied(f.document);
        }
    }

    void scaledAndTranslatedMixedArcSolvesConverge()
    {
        for (const double size : {1e-90, 1.0, 1e90})
        {
            MixedFixture f(size, Vec2{100, -300} * size);
            SolveOptions options;
            options.lengthTolerance *= size;
            options.c1Tolerance *= size;
            const Vec2 pin = f.vertex(1).position;
            const auto result = solveConstraints(f.document, f.document.getFixedLengthConstraints(),
                f.document.getParallelConstraints(), f.polygon.vertices[1], options);
            require(result.status == SolveStatus::Converged, "Scaled mixed arc solve failed");
            require(f.vertex(1).position == pin, "Scaled arc pin moved");
            requireSatisfied(f.document, options);
        }
    }

    DenseMatrix fullSampling(const Document& document,
        std::span<const constraint::FixedLengthConstraint> lengths,
        std::span<const constraint::ParallelConstraint> parallels,
        std::span<const ContinuityJoin> joins, std::span<const SolverPoint> variables,
        std::span<const double> base, double scale)
    {
        const auto kinds = buildResidualKinds(lengths.size(), parallels.size(), joins);
        require(kinds && kinds->size() == base.size(), "Reference row count invalid");
        DenseMatrix result(base.size(), std::vector<double>(variables.size() * 2));
        const double step = std::sqrt(std::numeric_limits<double>::epsilon()) * scale;
        for (std::size_t point = 0; point < variables.size(); ++point)
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            auto original = getPointPosition(document, variables[point]);
            require(original.has_value(), "Reference point missing");
            Vec2 position = *original;
            double& coordinate = axis == 0 ? position.x : position.y;
            const double old = coordinate;
            coordinate += step;
            if (coordinate == old) coordinate = std::nextafter(old, std::numeric_limits<double>::infinity());
            const double actualStep = coordinate - old;
            Document probe = document;
            require(setPointPosition(probe, variables[point], position) && updateArcBulges(probe), "Reference arc sample failed");
            const auto errors = evaluateErrors(probe, lengths, parallels, joins);
            require(errors && errors->size() == base.size(), "Reference residuals missing");
            for (std::size_t row = 0; row < base.size(); ++row)
            {
                double difference = (*errors)[row] - base[row];
                if ((*kinds)[row] == ResidualKind::G1)
                    difference = std::remainder(difference, 2 * std::numbers::pi_v<double>);
                const bool dimensional = (*kinds)[row] == ResidualKind::Length || (*kinds)[row] == ResidualKind::C1;
                result[row][2 * point + axis] = difference / (dimensional ? actualStep : actualStep / scale);
            }
        }
        return result;
    }

    void requireMatrix(const std::optional<DenseMatrix>& actual, const DenseMatrix& expected)
    {
        require(actual && actual->size() == expected.size(), "Arc Jacobian missing or wrong row count");
        for (std::size_t row = 0; row < expected.size(); ++row)
        {
            require((*actual)[row].size() == expected[row].size(), "Arc Jacobian wrong column count");
            for (std::size_t column = 0; column < expected[row].size(); ++column)
                requireNear((*actual)[row][column], expected[row][column], 1e-6 * (std::max)(1.0, std::abs(expected[row][column])));
        }
    }

    void mixedArcJacobianMatchesFreshFullSampling()
    {
        for (const double size : {1e-6, 1.0, 1e6})
        {
            MixedFixture f(size);
            require(updateArcBulges(f.document), "Jacobian baseline arc fit failed");
            const auto lengths = f.document.getFixedLengthConstraints();
            const auto parallels = f.document.getParallelConstraints();
            const auto joins = collectContinuityJoins(f.document);
            const auto errors = evaluateErrors(f.document, lengths, parallels, *joins);
            require(errors.has_value(), "Arc Jacobian base errors unavailable");
            std::vector<SolverPoint> variables;
            for (const auto id : f.polygon.vertices) variables.emplace_back(id);
            for (const auto id : f.other.vertices) variables.emplace_back(id);
            variables.emplace_back(ControlPointRef{f.polygon.edges[2], BezierControlPoint::First});
            variables.emplace_back(ControlPointRef{f.polygon.edges[2], BezierControlPoint::Second});
            variables.emplace_back(f.polygon.vertices[1]); // Repeat after other columns to expose state leakage.
            const Document before = f.document;
            const auto expected = fullSampling(f.document, lengths, parallels, *joins, variables, *errors, 10 * size);
            requireMatrix(computePointJacobian(f.document, lengths, parallels, *joins, variables, *errors, 10 * size), expected);
            requireSameState(f.document, before);
        }
    }

    void arcPairJacobianRestoresAllBulgesAndColumns()
    {
        Fixture f;
        f.edge(f.polygon.edges[0]).geometry = CircularArc{0.5};
        f.edge(f.polygon.edges[1]).geometry = CircularArc{-8};
        require(updateArcBulges(f.document), "Pair baseline fit failed");
        const auto joins = collectContinuityJoins(f.document);
        const auto errors = evaluateErrors(f.document, {}, {}, *joins);
        const std::array<SolverPoint, 5> variables{f.polygon.vertices[0], f.polygon.vertices[1],
            f.polygon.vertices[2], f.polygon.vertices[3], f.polygon.vertices[0]};
        const Document before = f.document;
        requireMatrix(computePointJacobian(f.document, {}, {}, *joins, variables, *errors, 10),
            fullSampling(f.document, {}, {}, *joins, variables, *errors, 10));
        requireSameState(f.document, before);
    }

    void staleArcBaseErrorsRetainFullSamplingSemantics()
    {
        Fixture f(true, true);
        const auto joins = collectContinuityJoins(f.document);
        auto errors = evaluateErrors(f.document, {}, {}, *joins); // Stored bulge is deliberately not fitted.
        require(errors.has_value(), "Stale arc baseline unavailable");
        (*errors)[0] += 0.25;
        const std::array<SolverPoint, 3> variables{f.control(), f.polygon.vertices[0], f.polygon.vertices[3]};
        const Document before = f.document;
        requireMatrix(computePointJacobian(f.document, {}, {}, *joins, variables, *errors, 10),
            fullSampling(f.document, {}, {}, *joins, variables, *errors, 10));
        requireSameState(f.document, before);
    }

    void invalidArcSampleRejectsWithoutTouchingSource()
    {
        Fixture f(true, true);
        const double step = std::sqrt(std::numeric_limits<double>::epsilon());
        std::get<CubicBezier>(f.edge(f.neighbor).geometry).control1 = {12, -step};
        require(updateArcBulges(f.document), "Boundary sample baseline invalid");
        const auto joins = collectContinuityJoins(f.document);
        const auto errors = evaluateErrors(f.document, {}, {}, *joins);
        const std::array<SolverPoint, 1> variables{f.control()};
        const Document before = f.document;
        require(!computePointJacobian(f.document, {}, {}, *joins, variables, *errors, 1), "Parallel-tangent probe accepted");
        requireSameState(f.document, before);
    }

    void g0ArcSamplingNormalizesOnlyPrivateGeometry()
    {
        Fixture f;
        f.vertex(1).continuity.value = Continuity::G0;
        const std::array lengths{constraint::FixedLengthConstraint{f.neighbor, 9}};
        const auto errors = evaluateErrors(f.document, lengths, {});
        const std::array<SolverPoint, 1> variables{f.polygon.vertices[2]};
        const Document before = f.document;
        requireMatrix(computePointJacobian(f.document, lengths, {}, variables, *errors, 10),
            fullSampling(f.document, lengths, {}, {}, variables, *errors, 10));
        requireSameState(f.document, before);
        std::get<CircularArc>(f.edge(f.arc).geometry).bulge = 0;
        require(!computePointJacobian(f.document, lengths, {}, variables, *errors, 10), "Unrelated invalid arc skipped");
    }

    void editorVertexDragRefitsAndCancelsArc()
    {
        Fixture f;
        require(updateArcBulges(f.document), "Drag initial arc invalid");
        const Document initial = f.document;
        EditorSession editor(f.document);
        require(editor.pointerDown({1, 0}, 2) && editor.pointerMove({-3, 2}), "Arc vertex drag failed");
        requireSameCurvePoint(editor.document().findVertex(f.polygon.vertices[0])->position, {-4, 2});
        requireSatisfied(editor.document());
        require(editor.cancelInteraction(), "Arc vertex cancellation failed");
        requireSameState(editor.document(), initial);
    }

    void editorHandleDragFitsArcAndKeepsEndpoints()
    {
        Fixture f(true, true);
        require(updateArcBulges(f.document), "Handle initial arc invalid");
        const Document initial = f.document;
        EditorSession editor(f.document);
        require(editor.pointerDown({12, 3}, 1) && editor.pointerMove({15, 5}), "Arc-linked handle drag failed");
        requireSameCurvePoint(curveHandlePosition(editor.document(), f.control()), {15, 5});
        for (const auto id : f.polygon.vertices)
            requireSameCurvePoint(editor.document().findVertex(id)->position, initial.findVertex(id)->position);
        requireNear(std::get<CircularArc>(editor.document().findEdge(f.arc)->geometry).bulge, std::sqrt(2.0) - 1, 1e-12);
        requireSatisfied(editor.document());
        require(editor.cancelInteraction(), "Arc handle cancellation failed");
        requireSameState(editor.document(), initial);
    }

    void editorContinuityChangesNormalizeAndRejectInvalidClasses()
    {
        Fixture f;
        f.vertex(2).position = {16, 8}; // Endpoint tangent (6,8), bulge 0.5.
        require(updateArcBulges(f.document), "Continuity baseline fit failed");
        EditorSession editor(f.document);
        require(editor.selectAt({10, 0}, 0) && editor.selectedVertexContinuity() == Continuity::G1,
            "Arc continuity selection unavailable");
        require(editor.setSelectedVertexContinuity(Continuity::G0), "Arc G0 change failed");
        require(std::get<CircularArc>(editor.document().findEdge(f.arc)->geometry).bulge == 1, "G0 did not reset radius");
        const Document g0 = editor.document();
        require(!editor.setSelectedVertexContinuity(Continuity::C1), "Arc C1 change accepted");
        requireSameState(editor.document(), g0);
        require(editor.setSelectedVertexContinuity(Continuity::G1), "Arc G1 restoration failed");
        requireNear(std::get<CircularArc>(editor.document().findEdge(f.arc)->geometry).bulge, 0.5, 1e-12);
        require(editor.selectAt({0, 0}, 0), "Other endpoint selection failed");
        const Document before = editor.document();
        require(!editor.setSelectedVertexContinuity(Continuity::G1), "Second arc G1 accepted");
        requireSameState(editor.document(), before);
    }

    void rejectedHandlePreviewKeepsLastFitAndGesture()
    {
        Fixture f(true, true);
        require(updateArcBulges(f.document), "Preview initial arc invalid");
        const Document initial = f.document;
        EditorSession editor(f.document);
        require(editor.pointerDown({12, 3}, 0) && editor.pointerMove({15, 5}), "First valid arc preview failed");
        const Document preview = editor.document();
        require(!editor.pointerMove({15, 0}) && editor.isDragging(), "Undefined arc preview accepted or cancelled gesture");
        requireSameState(editor.document(), preview);
        require(editor.pointerMove({16, 4}), "Arc drag could not resume after rejection");
        requireSatisfied(editor.document());
        require(editor.cancelInteraction(), "Preview cancellation failed");
        requireSameState(editor.document(), initial);
    }
}

namespace cad::tests
{
    std::span<const TestCase> arcSolverTests()
    {
        static const std::array tests{
            TestCase{"collector includes arcs and rejects C1", collectorIncludesArcsAndRejectsC1},
            TestCase{"analytic fitting commits with zero coordinate budget", analyticFittingCommitsWithZeroCoordinateBudget},
            TestCase{"G0 normalization runs before validation", g0NormalizationRunsBeforeValidation},
            TestCase{"public handle solve fits arc without moving endpoints", publicHandleSolveFitsArcWithoutMovingEndpoints},
            TestCase{"mixed lengths parallels C1 and arc converge", mixedLengthsParallelsC1AndArcConverge},
            TestCase{"rejected budgets and conflicts keep original bulges", rejectedBudgetsAndConflictsKeepOriginalBulges},
            TestCase{"unconstructable arc fits reject without mutation", unconstructableArcFitsRejectWithoutMutation},
            TestCase{"invalid arc settings and pins keep document", invalidArcSettingsAndPinsKeepDocument},
            TestCase{"arc pairs keep reference and closing join", arcPairsKeepReferenceAndClosingJoin},
            TestCase{"scaled and translated mixed arc solves converge", scaledAndTranslatedMixedArcSolvesConverge},
            TestCase{"mixed arc Jacobian matches fresh full sampling", mixedArcJacobianMatchesFreshFullSampling},
            TestCase{"arc pair Jacobian restores all bulges and columns", arcPairJacobianRestoresAllBulgesAndColumns},
            TestCase{"stale arc base errors retain full sampling semantics", staleArcBaseErrorsRetainFullSamplingSemantics},
            TestCase{"invalid arc sample rejects without touching source", invalidArcSampleRejectsWithoutTouchingSource},
            TestCase{"G0 sampling normalizes only private geometry", g0ArcSamplingNormalizesOnlyPrivateGeometry},
            TestCase{"editor vertex drag refits and cancels arc", editorVertexDragRefitsAndCancelsArc},
            TestCase{"editor handle drag fits arc and keeps endpoints", editorHandleDragFitsArcAndKeepsEndpoints},
            TestCase{"editor continuity changes normalize and reject invalid classes", editorContinuityChangesNormalizeAndRejectInvalidClasses},
            TestCase{"rejected handle preview keeps last fit and gesture", rejectedHandlePreviewKeepsLastFitAndGesture}
        };
        return tests;
    }
}
