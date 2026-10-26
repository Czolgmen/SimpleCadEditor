#include "../CurveTestSupport.h"

#include <cad_core/constraints/ContinuityConstraint.h>
#include <cad_core/model/ArcContinuity.h>

#include <algorithm>
#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;

    struct Fixture : CurveTestScene
    {
        Edge& edge(EdgeId id) { return *const_cast<Edge*>(document.findEdge(id)); }
        Vertex& vertex(VertexId id) { return *const_cast<Vertex*>(document.findVertex(id)); }

        void arc(EdgeId id, double bulge)
        {
            document.removeFixedLength(id);
            document.removeParallelsForEdge(id);
            edge(id).geometry = CircularArc{bulge};
        }

        void bezier(EdgeId id, CubicBezier curve)
        {
            document.removeFixedLength(id);
            document.removeParallelsForEdge(id);
            edge(id).geometry = curve;
        }
    };

    void requireSameState(const Document& actual, const Document& expected)
    {
        requireSameCurveDocument(actual, expected);
        Document a = actual, b = expected;
        const std::array probe{Vec2{200, 200}, Vec2{220, 200}, Vec2{200, 220}};
        require(a.addPolygon(probe) == b.addPolygon(probe), "Arc update consumed polygon IDs");
        requireSameCurveDocument(a, b);
    }

    void requireRejected(Fixture& f)
    {
        const Document before = f.document;
        require(!updateArcBulges(f.document), "Invalid arc update accepted");
        requireSameState(f.document, before);
    }

    void requireUpdated(Fixture& f, std::initializer_list<std::pair<EdgeId, double>> changes)
    {
        const Document before = f.document;
        require(updateArcBulges(f.document), "Valid arc update rejected");
        Document expected = before;
        for (const auto& [id, bulge] : changes)
        {
            auto& result = std::get<CircularArc>(const_cast<Edge*>(expected.findEdge(id))->geometry).bulge;
            const double actual = std::get<CircularArc>(f.document.findEdge(id)->geometry).bulge;
            require(std::isfinite(actual) && std::abs(actual / bulge - 1) <= 2e-12,
                "Wrong fitted bulge or arc orientation");
            result = actual; // All other fields and all untouched arcs must match exactly.
        }
        requireSameState(f.document, expected);
        require(f.document.validate().empty(), "Arc update left invalid model geometry");
        for (const auto& [polygonId, polygon] : f.document.getPolygons())
        for (std::size_t i = 0; i < polygon.vertices.size(); ++i)
        {
            const auto vertex = polygon.vertices[i];
            const auto incoming = polygon.edges[(i + polygon.edges.size() - 1) % polygon.edges.size()];
            const auto outgoing = polygon.edges[i];
            if (f.document.findVertex(vertex)->continuity.value != Continuity::G1 ||
                (!std::holds_alternative<CircularArc>(f.document.findEdge(incoming)->geometry) &&
                 !std::holds_alternative<CircularArc>(f.document.findEdge(outgoing)->geometry)))
                continue;
            const auto errors = constraint::continuityErrors(f.document, {vertex, incoming, outgoing, Continuity::G1});
            require(errors && errors->size() == 1 && std::abs(errors->front()) <= 1e-11,
                "Updated arc does not match the neighbor tangent direction");
        }
    }

    // Set the neighboring line/Bezier to a known endpoint direction. Independent
    // half-sweep identities provide reference data rather than using Arc helpers.
    void prepareJoin(Fixture& f, std::size_t index, bool atStart, bool bezier, double wanted)
    {
        const EdgeId arcId = f.first.edges[index];
        f.arc(arcId, -8);
        const auto& edge = f.edge(arcId);
        const Vec2 start = f.vertex(edge.start).position, end = f.vertex(edge.end).position;
        const Vec2 chord = end - start, perpendicular{-chord.y, chord.x};
        const double denominator = 1 + wanted * wanted;
        const Vec2 tangent = chord * ((1 - wanted * wanted) / denominator) +
            perpendicular * ((atStart ? -2 : 2) * wanted / denominator);
        const VertexId sharedId = atStart ? edge.start : edge.end;
        const Vec2 shared = f.vertex(sharedId).position;
        f.vertex(sharedId).continuity = {Continuity::G1, ContinuitySource::Explicit};
        const EdgeId neighborId = f.first.edges[atStart ? (index + 3) % 4 : (index + 1) % 4];
        auto& neighbor = f.edge(neighborId);
        if (bezier)
        {
            f.bezier(neighborId, atStart
                ? CubicBezier{{-90, 130}, shared - tangent * (1.0 / 3)}
                : CubicBezier{shared + tangent * (1.0 / 3), {-90, 130}});
        }
        else
        {
            neighbor.geometry = LineSegment{};
            f.vertex(atStart ? neighbor.start : neighbor.end).position = shared + tangent * (atStart ? -1 : 1);
        }
    }

    void emptyAndArcFreeScenesRemainUnchanged()
    {
        Document empty;
        require(updateArcBulges(empty), "Empty document rejected");
        requireSameState(empty, Document{});
        Fixture f;
        requireUpdated(f, {});
    }

    void restoresSignedSemicirclesAcrossFigures()
    {
        for (std::size_t index = 0; index < 4; ++index)
        for (const double bulge : {-8.0, -2.0, -1.0, -0.125, 0.125, 1.0, 2.0, 8.0})
        {
            Fixture f;
            f.arc(f.first.edges[index], bulge);
            f.arc(f.other.edges[2], -bulge);
            f.vertex(f.first.vertices[index]).continuity.source = ContinuitySource::Explicit;
            requireUpdated(f, {{f.first.edges[index], std::copysign(1.0, bulge)},
                {f.other.edges[2], std::copysign(1.0, -bulge)}});
        }
    }

    void fitsBothEndsToSegmentsIncludingClosingEdge()
    {
        for (std::size_t index = 0; index < 4; ++index)
        for (const bool atStart : {false, true})
        for (const double bulge : {-2.0, -1.0, -0.5, 0.5, 1.0, 2.0})
        {
            Fixture f;
            prepareJoin(f, index, atStart, false, bulge);
            requireUpdated(f, {{f.first.edges[index], bulge}});
        }
    }

    void fitsBothEndsToAdjacentBezierHandles()
    {
        for (std::size_t index = 0; index < 4; ++index)
        for (const bool atStart : {false, true})
        for (const double bulge : {-2.0, -0.5, 0.5, 2.0})
        {
            Fixture f;
            prepareJoin(f, index, atStart, true, bulge);
            // Default and Explicit select the same geometric rule.
            if (bulge > 0) f.vertex(f.first.vertices[atStart ? index : (index + 1) % 4]).continuity.source = ContinuitySource::Default;
            requireUpdated(f, {{f.first.edges[index], bulge}});
        }
    }

    void reevaluatesCurrentBezierHandles()
    {
        Fixture f;
        prepareJoin(f, 0, true, true, 0.5);
        requireUpdated(f, {{f.first.edges[0], 0.5}});
        // The new incoming tangent is proportional to (-0.6, 0.8), giving bulge -2.
        require(f.document.moveControlPoint({f.first.edges[3], BezierControlPoint::Second}, {6, -8}),
            "Could not move reference handle");
        requireUpdated(f, {{f.first.edges[0], -2}});
    }

    void twoArcsKeepLowerIdAtEveryJoin()
    {
        for (std::size_t shared = 0; shared < 4; ++shared)
        for (const double reference : {-4.0, -2.0, -0.5, -0.25, 0.25, 0.5, 2.0, 4.0})
        {
            Fixture f;
            const EdgeId incoming = f.first.edges[(shared + 3) % 4], outgoing = f.first.edges[shared];
            const EdgeId kept = (std::min)(incoming, outgoing), fitted = (std::max)(incoming, outgoing);
            f.arc(kept, reference);
            f.arc(fitted, 8);
            f.vertex(f.first.vertices[shared]).continuity.value = Continuity::G1;
            // For a right-angle polygon join, tan(pi/4 - atan(reference)).
            requireUpdated(f, {{fitted, (1 - reference) / (1 + reference)}});
        }
    }

    void arcPairsAreIndependentOfTraversalAndIdempotent()
    {
        for (std::size_t rotation = 0; rotation < 4; ++rotation)
        {
            Fixture f;
            f.arc(f.first.edges[0], 0.5);
            f.arc(f.first.edges[1], -8);
            f.vertex(f.first.vertices[1]).continuity.value = Continuity::G1;
            auto& polygon = *const_cast<Polygon*>(f.document.findPolygon(f.first.id));
            std::rotate(polygon.vertices.begin(), polygon.vertices.begin() + rotation, polygon.vertices.end());
            std::rotate(polygon.edges.begin(), polygon.edges.begin() + rotation, polygon.edges.end());
            requireUpdated(f, {{f.first.edges[1], 1.0 / 3}});
            const Document before = f.document;
            require(updateArcBulges(f.document), "Repeated update rejected");
            requireSameState(f.document, before);
        }
    }

    void laterFailureRollsBackEarlierPlannedChanges()
    {
        Fixture f;
        f.arc(f.first.edges[0], 2);
        f.arc(f.other.edges[0], 1);
        f.vertex(f.other.vertices[0]).continuity.value = Continuity::G1;
        f.vertex(f.other.vertices[3]).position = {70, 100}; // Incoming direction parallel to the arc chord.
        requireRejected(f);
    }

    void rejectsZeroParallelAndInvalidNeighborTangents()
    {
        for (const bool atStart : {false, true})
        for (const bool bezier : {false, true})
        for (int invalid = 0; invalid < 4; ++invalid)
        {
            Fixture f;
            prepareJoin(f, 0, atStart, bezier, 0.5);
            const EdgeId neighborId = f.first.edges[atStart ? 3 : 1];
            auto& neighbor = f.edge(neighborId);
            const Vec2 shared = f.vertex(atStart ? f.first.vertices[0] : f.first.vertices[1]).position;
            if (bezier)
            {
                auto& curve = std::get<CubicBezier>(neighbor.geometry);
                auto& handle = atStart ? curve.control2 : curve.control1;
                if (invalid == 0) handle = shared; // Zero derivative.
                if (invalid == 1) handle = shared + Vec2{10, 0}; // Parallel derivative, either direction.
                if (invalid == 2) handle.x = std::numeric_limits<double>::quiet_NaN();
                if (invalid == 3) handle.x = std::numeric_limits<double>::max(); // Derivative overflow.
            }
            else
            {
                auto& far = f.vertex(atStart ? neighbor.start : neighbor.end).position;
                if (invalid == 0) far = shared;
                if (invalid == 1) far = shared + Vec2{10, 0};
                if (invalid == 2) far.x = std::numeric_limits<double>::quiet_NaN();
                if (invalid == 3) far.y = std::numeric_limits<double>::infinity();
            }
            requireRejected(f);
        }
    }

    void rejectsInvalidSettingsAndStoredBulges()
    {
        for (const bool end : {false, true})
        for (const auto value : {Continuity::C1, static_cast<Continuity>(-1), static_cast<Continuity>(3)})
        {
            Fixture f;
            f.arc(f.first.edges[0], 1);
            f.vertex(f.first.vertices[end ? 1 : 0]).continuity.value = value;
            requireRejected(f);
        }
        for (const auto source : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            f.arc(f.first.edges[0], 1);
            for (std::size_t i = 0; i < 2; ++i) f.vertex(f.first.vertices[i]).continuity = {Continuity::G1, source};
            requireRejected(f);
        }
        for (const double invalid : {0.0, -0.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            f.arc(f.first.edges[0], invalid);
            requireRejected(f);
        }
    }

    void rejectsBrokenCyclesAndReferences()
    {
        for (int kind = 0; kind < 8; ++kind)
        {
            Fixture f;
            f.arc(f.first.edges[0], 2); // Would change if the later broken cycle were ignored.
            auto& polygon = *const_cast<Polygon*>(f.document.findPolygon(f.other.id));
            if (kind == 0) { polygon.vertices.resize(2); polygon.edges.resize(2); }
            if (kind == 1) polygon.vertices.clear();
            if (kind == 2) polygon.edges.pop_back();
            if (kind == 3) polygon.edges[1] = EdgeId{99999};
            if (kind == 4) polygon.vertices[1] = VertexId{99999};
            if (kind == 5) f.edge(polygon.edges[1]).start = VertexId{};
            if (kind == 6) f.edge(polygon.edges[1]).end = polygon.vertices[0];
            if (kind == 7) { polygon.vertices = f.first.vertices; polygon.edges = f.first.edges; }
            requireRejected(f);
        }
    }

    void rejectsCoincidentAndNonfiniteArcEndpoints()
    {
        for (const bool end : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (const bool y : {false, true})
        {
            Fixture f;
            f.arc(f.first.edges[0], 2);
            auto& position = f.vertex(f.first.vertices[end ? 1 : 0]).position;
            (y ? position.y : position.x) = invalid;
            requireRejected(f);
        }
        Fixture f;
        f.arc(f.first.edges[0], 2);
        f.vertex(f.first.vertices[1]).position = f.vertex(f.first.vertices[0]).position;
        requireRejected(f);
    }

    void rejectsUnrepresentableCircleOrG1Derivative()
    {
        const double maximum = std::numeric_limits<double>::max();
        Fixture radius;
        radius.arc(radius.first.edges[0], 2);
        radius.vertex(radius.first.vertices[0]).position = {-maximum, -maximum};
        radius.vertex(radius.first.vertices[1]).position = {maximum, maximum};
        requireRejected(radius);

        Fixture tangent;
        tangent.arc(tangent.first.edges[0], 1);
        tangent.vertex(tangent.first.vertices[0]).continuity.value = Continuity::G1;
        tangent.vertex(tangent.first.vertices[0]).position = {-maximum * 0.5, 0};
        tangent.vertex(tangent.first.vertices[1]).position = {maximum * 0.5, 0};
        tangent.edge(tangent.first.edges[3]).geometry = LineSegment{};
        tangent.vertex(tangent.first.vertices[3]).position = {-maximum * 0.5, maximum * 0.25};
        requireRejected(tangent);
    }

    void fittingRespectsScalingRotationAndReflection()
    {
        for (const bool atStart : {false, true})
        for (const bool reflected : {false, true})
        for (const double scale : {1e-90, 1.0, 1e90})
        {
            Fixture f;
            prepareJoin(f, 0, atStart, true, 0.5);
            const auto transform = [&](Vec2 point)
            {
                if (reflected) point.x = -point.x;
                return Vec2{-point.y + 100, point.x - 300} * scale;
            };
            for (const auto id : f.first.vertices) f.vertex(id).position = transform(f.vertex(id).position);
            for (const auto id : f.first.edges)
                if (auto* curve = std::get_if<CubicBezier>(&f.edge(id).geometry))
                { curve->control1 = transform(curve->control1); curve->control2 = transform(curve->control2); }
            requireUpdated(f, {{f.first.edges[0], reflected ? -0.5 : 0.5}});
        }
    }

    void movingAnEndpointRefitsWithoutMovingOtherPoints()
    {
        Fixture f;
        prepareJoin(f, 0, true, false, 1);
        requireUpdated(f, {{f.first.edges[0], 1}});
        // The reference remains downward while the chord rotates to (30,30).
        require(f.document.moveVertex(f.first.vertices[1], {30, 30}), "Endpoint fixture move failed");
        requireUpdated(f, {{f.first.edges[0], 1 + std::sqrt(2.0)}});
    }
}

namespace cad::tests
{
    std::span<const TestCase> arcContinuityTests()
    {
        static const std::array tests{
            TestCase{"empty and arc-free scenes remain unchanged", emptyAndArcFreeScenesRemainUnchanged},
            TestCase{"signed semicircles restored across figures", restoresSignedSemicirclesAcrossFigures},
            TestCase{"both ends fit to segments including closing edge", fitsBothEndsToSegmentsIncludingClosingEdge},
            TestCase{"both ends fit to adjacent Bezier handles", fitsBothEndsToAdjacentBezierHandles},
            TestCase{"current Bezier handles are reevaluated", reevaluatesCurrentBezierHandles},
            TestCase{"two arcs keep lower ID at every join", twoArcsKeepLowerIdAtEveryJoin},
            TestCase{"arc pairs are independent of traversal and idempotent", arcPairsAreIndependentOfTraversalAndIdempotent},
            TestCase{"later failure rolls back earlier planned changes", laterFailureRollsBackEarlierPlannedChanges},
            TestCase{"zero parallel and invalid neighbor tangents rejected", rejectsZeroParallelAndInvalidNeighborTangents},
            TestCase{"invalid settings and stored bulges rejected", rejectsInvalidSettingsAndStoredBulges},
            TestCase{"broken cycles and references rejected", rejectsBrokenCyclesAndReferences},
            TestCase{"coincident and nonfinite arc endpoints rejected", rejectsCoincidentAndNonfiniteArcEndpoints},
            TestCase{"unrepresentable circle or G1 derivative rejected", rejectsUnrepresentableCircleOrG1Derivative},
            TestCase{"fitting respects scaling rotation and reflection", fittingRespectsScalingRotationAndReflection},
            TestCase{"moving an endpoint refits without moving other points", movingAnEndpointRefitsWithoutMovingOtherPoints}
        };
        return tests;
    }
}
