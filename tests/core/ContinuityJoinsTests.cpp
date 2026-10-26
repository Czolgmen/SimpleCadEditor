#include "../CurveTestSupport.h"

#include <cad_core/constraints/ContinuityConstraint.h>
#include <cad_core/model/ContinuityJoins.h>

#include <initializer_list>
#include <limits>
#include <numbers>

namespace
{
    using namespace cad;
    using namespace cad::tests;
    using cad::constraint::continuityErrors;

    struct Fixture
    {
        Document document;
        Polygon polygon;

        Fixture()
        {
            const std::array positions{Vec2{0, 0}, Vec2{12, 0}, Vec2{12, 9}, Vec2{0, 9}};
            polygon = *document.findPolygon(document.addPolygon(positions));
        }

        Edge& edge(std::size_t index)
        {
            return *const_cast<Edge*>(document.findEdge(polygon.edges[index]));
        }

        Vertex& vertex(std::size_t index)
        {
            return *const_cast<Vertex*>(document.findVertex(polygon.vertices[index]));
        }

        ContinuityJoin join(Continuity type = Continuity::G1) const
        {
            return {polygon.vertices[1], polygon.edges[0], polygon.edges[1], type};
        }

        void convert(std::size_t index)
        {
            require(document.convertEdgeToBezier(polygon.edges[index]), "Bezier fixture conversion failed");
        }
    };

    void requireNear(double actual, double expected)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= 1e-12,
            "Unexpected document continuity residual");
    }

    void requireErrors(const Document& document, const ContinuityJoin& join,
        std::initializer_list<double> expected)
    {
        const auto errors = continuityErrors(document, join);
        require(errors && errors->size() == expected.size(), "Wrong residual count or rejected valid join");
        std::size_t i = 0;
        for (const double value : expected)
            requireNear((*errors)[i++], value);
    }

    std::vector<ContinuityJoin> collected(const Document& document)
    {
        const auto joins = collectContinuityJoins(document);
        require(joins.has_value(), "Valid document rejected by collector");
        return *joins;
    }

    void requireCollected(const Fixture& fixture, std::initializer_list<std::size_t> indices)
    {
        const auto joins = collected(fixture.document);
        require(joins.size() == indices.size(), "Wrong number of active joins");
        std::size_t i = 0;
        for (const auto index : indices)
        {
            const auto& join = joins[i++];
            require(join.vertex == fixture.polygon.vertices[index] &&
                join.incoming == fixture.polygon.edges[(index + 3) % 4] &&
                join.outgoing == fixture.polygon.edges[index], "Wrong join IDs, orientation or order");
            require(join.continuity == fixture.document.findVertex(join.vertex)->continuity.value,
                "Collector lost the continuity class");
        }
    }

    void emptyAndStraightDocumentsHaveNoActiveJoins()
    {
        require(collected(Document{}).empty(), "Empty document has joins");
        Fixture f;
        requireCollected(f, {});
        for (std::size_t i = 0; i < 4; ++i)
            require(f.document.setContinuity(f.polygon.vertices[i], i % 2 ? Continuity::G1 : Continuity::C1),
                "Line setting fixture failed");
        requireCollected(f, {});
    }

    void singleCurveIncludesBothEndsAndClosingJoin()
    {
        for (std::size_t i = 0; i < 4; ++i)
        {
            Fixture f;
            f.convert(i);
            switch (i)
            {
            case 0: requireCollected(f, {0, 1}); break;
            case 1: requireCollected(f, {1, 2}); break;
            case 2: requireCollected(f, {2, 3}); break;
            case 3: requireCollected(f, {0, 3}); break;
            }
        }
    }

    void adjacentCurvesDoNotDuplicateTheirSharedVertex()
    {
        for (std::size_t i = 0; i < 4; ++i)
        {
            Fixture f;
            f.convert(i);
            f.convert((i + 1) % 4);
            switch (i)
            {
            case 0: requireCollected(f, {0, 1, 2}); break;
            case 1: requireCollected(f, {1, 2, 3}); break;
            case 2: requireCollected(f, {0, 2, 3}); break;
            case 3: requireCollected(f, {0, 1, 3}); break;
            }
        }
    }

    void closedCurveContourHasOneJoinPerVertex()
    {
        Fixture f;
        for (std::size_t i = 0; i < 4; ++i) f.convert(i);
        requireCollected(f, {0, 1, 2, 3});
    }

    void g0SkipsOnlyItsChosenEndpoints()
    {
        for (const bool skipStart : {false, true})
        for (const bool skipEnd : {false, true})
        {
            Fixture f;
            f.convert(0);
            if (skipStart) require(f.document.setContinuity(f.polygon.vertices[0], Continuity::G0), "G0 start failed");
            if (skipEnd) require(f.document.setContinuity(f.polygon.vertices[1], Continuity::G0), "G0 end failed");
            if (skipStart && skipEnd) requireCollected(f, {});
            else if (skipStart) requireCollected(f, {1});
            else if (skipEnd) requireCollected(f, {0});
            else requireCollected(f, {0, 1});
        }
    }

    void collectorKeepsG1AndC1RegardlessOfSource()
    {
        Fixture f;
        f.convert(0);
        require(f.document.setContinuity(f.polygon.vertices[0], Continuity::C1), "C1 fixture failed");
        auto joins = collected(f.document);
        require(joins.size() == 2 && joins[0].continuity == Continuity::C1 && joins[1].continuity == Continuity::G1,
            "Explicit C1 or default G1 lost");
        require(f.document.setContinuity(f.polygon.vertices[1], Continuity::G1), "Explicit G1 failed");
        requireCollected(f, {0, 1});
    }

    void multiplePolygonsKeepTheirOwnIdsAndOrder()
    {
        Fixture f;
        f.convert(3);
        const std::array positions{Vec2{30, 0}, Vec2{42, 0}, Vec2{42, 9}, Vec2{30, 9}};
        const Polygon other = *f.document.findPolygon(f.document.addPolygon(positions));
        require(f.document.convertEdgeToBezier(other.edges[1]), "Other polygon conversion failed");
        const auto joins = collected(f.document);
        require(joins.size() == 4 && joins[0].vertex == f.polygon.vertices[0] &&
            joins[1].vertex == f.polygon.vertices[3] && joins[2].vertex == other.vertices[1] &&
            joins[3].vertex == other.vertices[2], "Cross-polygon join order or IDs changed");
        require(joins[2].incoming == other.edges[0] && joins[2].outgoing == other.edges[1] &&
            joins[3].incoming == other.edges[1] && joins[3].outgoing == other.edges[2], "Polygons mixed edges");
    }

    void recollectionReflectsConversionsAndSettings()
    {
        Fixture f;
        f.convert(0);
        requireCollected(f, {0, 1});
        require(f.document.convertEdgeToSegment(f.polygon.edges[0]), "Segment conversion failed");
        requireCollected(f, {});
        f.convert(1);
        requireCollected(f, {1, 2});
        require(f.document.setContinuity(f.polygon.vertices[1], Continuity::G0), "Setting change failed");
        requireCollected(f, {2});
    }

    void invalidPolygonSizesAreRejectedBeforeIndexing()
    {
        for (const std::size_t count : {0u, 1u, 2u})
        {
            Fixture f;
            auto* polygon = const_cast<Polygon*>(f.document.findPolygon(f.polygon.id));
            polygon->vertices.resize(count);
            polygon->edges.resize(count);
            require(!collectContinuityJoins(f.document), "Too few vertices accepted");
        }
        for (const std::size_t count : {0u, 3u, 5u})
        {
            Fixture f;
            const_cast<Polygon*>(f.document.findPolygon(f.polygon.id))->edges.resize(count);
            require(!collectContinuityJoins(f.document), "Mismatched list lengths accepted");
        }
    }

    void missingPolygonReferencesAreRejected()
    {
        for (const bool vertex : {false, true})
        for (std::size_t i = 0; i < 4; ++i)
        {
            Fixture f;
            f.convert(0);
            auto* polygon = const_cast<Polygon*>(f.document.findPolygon(f.polygon.id));
            if (vertex) polygon->vertices[i] = VertexId{999999};
            else polygon->edges[i] = EdgeId{999999};
            require(!collectContinuityJoins(f.document), "Missing polygon reference accepted");
        }
    }

    void wrongEdgeConnectionsAreRejected()
    {
        for (const bool start : {false, true})
        for (std::size_t i = 0; i < 4; ++i)
        {
            Fixture f;
            f.convert(0);
            auto& edge = f.edge(i);
            (start ? edge.start : edge.end) = f.polygon.vertices[(i + 2) % 4];
            require(!collectContinuityJoins(f.document), "Wrong endpoint connection accepted");
        }
    }

    void inactiveJoinsStillRejectBrokenTopology()
    {
        Fixture f;
        f.edge(3).end = VertexId{999999};
        require(!collectContinuityJoins(f.document), "G0 line-only contour bypassed topology checks");
    }

    void documentErrorsUseCorrectEndTangentsForEveryGeometryPair()
    {
        for (const bool incomingCurve : {false, true})
        for (const bool outgoingCurve : {false, true})
        {
            Fixture f;
            // Deliberately unrelated opposite handles expose choosing the wrong end.
            if (incomingCurve) f.edge(0).geometry = CubicBezier{{3, 2}, {10, -1}};
            if (outgoingCurve) f.edge(1).geometry = CubicBezier{{16, 2}, {15, 8}};
            const Vec2 incoming = incomingCurve ? Vec2{6, 3} : Vec2{12, 0};
            const Vec2 outgoing = outgoingCurve ? Vec2{12, 6} : Vec2{0, 9};
            requireErrors(f.document, f.join(),
                {std::atan2(outgoing.y, outgoing.x) - std::atan2(incoming.y, incoming.x)});
            requireErrors(f.document, f.join(Continuity::C1), {outgoing.x - incoming.x, outgoing.y - incoming.y});
            requireErrors(f.document, f.join(Continuity::G0), {});
        }
    }

    void g0AndC1AllowZeroDerivativesWhileG1RejectsThem()
    {
        Fixture f;
        f.edge(0).geometry = CubicBezier{{2, 3}, {12, 0}};
        f.edge(1).geometry = CubicBezier{{12, 0}, {14, 5}};
        requireErrors(f.document, f.join(Continuity::G0), {});
        requireErrors(f.document, f.join(Continuity::C1), {0, 0});
        require(!continuityErrors(f.document, f.join()), "G1 accepted two zero derivatives");
    }

    void documentErrorsRejectUnknownIds()
    {
        Fixture f;
        for (const std::uint64_t missing : {std::uint64_t{0}, std::uint64_t{999999}})
        for (int field = 0; field < 3; ++field)
        {
            auto join = f.join();
            if (field == 0) join.vertex = VertexId{missing};
            if (field == 1) join.incoming = EdgeId{missing};
            if (field == 2) join.outgoing = EdgeId{missing};
            require(!continuityErrors(f.document, join), "Unknown join ID accepted");
        }
        require(!continuityErrors(Document{}, f.join()), "Empty document accepted a join");
    }

    void documentErrorsRejectWrongJoinOrientation()
    {
        Fixture f;
        auto join = f.join();
        std::swap(join.incoming, join.outgoing);
        require(!continuityErrors(f.document, join), "Reversed edge order accepted");
        for (const bool incoming : {false, true})
        {
            join = f.join();
            (incoming ? join.incoming : join.outgoing) = f.polygon.edges[2];
            require(!continuityErrors(f.document, join), "Nonincident edge accepted");
        }
        join = f.join();
        join.vertex = f.polygon.vertices[2];
        require(!continuityErrors(f.document, join), "Wrong existing joint vertex accepted");
    }

    void documentErrorsRejectMissingEndpointVertices()
    {
        for (int endpoint = 0; endpoint < 3; ++endpoint)
        {
            Fixture f;
            auto join = f.join();
            if (endpoint == 0) f.edge(0).start = VertexId{999999};
            if (endpoint == 1)
            {
                f.edge(0).end = f.edge(1).start = join.vertex = VertexId{999999};
            }
            if (endpoint == 2) f.edge(1).end = VertexId{999999};
            require(!continuityErrors(f.document, join), "Missing endpoint vertex accepted");
        }
    }

    void documentErrorsRejectNonFiniteEndpointPositions()
    {
        for (const auto type : {Continuity::G0, Continuity::G1, Continuity::C1})
        for (std::size_t index = 0; index < 3; ++index)
        for (const bool x : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            auto& position = f.vertex(index).position;
            (x ? position.x : position.y) = invalid;
            require(!continuityErrors(f.document, f.join(type)), "Non-finite endpoint position accepted");
        }
    }

    void documentErrorsRejectNonFiniteRelevantHandles()
    {
        for (const auto type : {Continuity::G1, Continuity::C1})
        for (const bool incoming : {false, true})
        for (const bool x : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            f.convert(0);
            f.convert(1);
            auto& curve = std::get<CubicBezier>(f.edge(incoming ? 0 : 1).geometry);
            auto& handle = incoming ? curve.control2 : curve.control1;
            (x ? handle.x : handle.y) = invalid;
            require(!continuityErrors(f.document, f.join(type)), "Non-finite active handle accepted");
        }
    }

    void sameJoinReevaluatesCurrentHandlesAndVertices()
    {
        Fixture f;
        f.convert(0);
        f.convert(1);
        const ContinuityJoin join = f.join();
        requireErrors(f.document, join, {std::numbers::pi_v<double> / 2});
        require(f.document.moveControlPoint({f.polygon.edges[0], BezierControlPoint::Second}, {12, -3}),
            "Incoming handle edit failed");
        requireErrors(f.document, join, {0});
        require(f.document.moveControlPoint({f.polygon.edges[1], BezierControlPoint::First}, {15, 0}),
            "Outgoing handle edit failed");
        requireErrors(f.document, join, {-std::numbers::pi_v<double> / 2});
        require(f.document.moveVertex(f.polygon.vertices[1], {15, -3}), "Joint vertex edit failed");
        requireErrors(f.document, join, {std::numbers::pi_v<double> / 2});
    }

    void oneZeroDerivativeIsRejectedOnlyByG1()
    {
        for (const bool incoming : {false, true})
        {
            Fixture f;
            if (incoming) f.edge(0).geometry = CubicBezier{{3, 2}, {12, 0}};
            else f.edge(1).geometry = CubicBezier{{12, 0}, {15, 8}};
            require(!continuityErrors(f.document, f.join()), "G1 accepted a zero endpoint derivative");
            if (incoming) requireErrors(f.document, f.join(Continuity::C1), {0, 9});
            else requireErrors(f.document, f.join(Continuity::C1), {-12, 0});
        }
    }

    void documentErrorsRejectDerivativeOverflow()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool incoming : {false, true})
        for (const bool curve : {false, true})
        for (const auto type : {Continuity::G1, Continuity::C1})
        {
            Fixture f;
            if (curve)
            {
                if (incoming) f.edge(0).geometry = CubicBezier{{3, 2}, {-maximum, 0}};
                else f.edge(1).geometry = CubicBezier{{maximum, 0}, {15, 8}};
            }
            else
            {
                f.vertex(1).position.x = maximum;
                f.vertex(incoming ? 0 : 2).position.x = -maximum;
            }
            require(!continuityErrors(f.document, f.join(type)), "Overflowed endpoint derivative accepted");
        }
    }

    void documentErrorsRejectInvalidContinuityClass()
    {
        Fixture f;
        for (const auto type : {static_cast<Continuity>(-1), static_cast<Continuity>(3), static_cast<Continuity>(999)})
            require(!continuityErrors(f.document, f.join(type)), "Invalid continuity class accepted");
    }

    void documentErrorsUseArcTangentsWithSegments()
    {
        for (const bool incoming : {false, true})
        for (const double sign : {-1.0, 1.0})
        {
            Fixture f;
            f.vertex(2).position = {24, 0};
            f.edge(incoming ? 0 : 1).geometry = CircularArc{sign};
            requireErrors(f.document, f.join(), {-sign * std::numbers::pi / 2});

            Fixture aligned;
            aligned.edge(incoming ? 0 : 1).geometry = CircularArc{sign};
            if (incoming) aligned.vertex(2).position = {12, sign * 9};
            else aligned.vertex(0).position = {12 - sign * 12, 0};
            requireErrors(aligned.document, aligned.join(), {0});
        }
    }

    void documentErrorsUseArcTangentsWithBeziers()
    {
        for (const double sign : {-1.0, 1.0})
        {
            Fixture incoming;
            incoming.edge(0).geometry = CircularArc{sign};
            incoming.edge(1).geometry = CubicBezier{{12, sign * 3}, {-20, 15}};
            requireErrors(incoming.document, incoming.join(), {0});

            Fixture outgoing;
            outgoing.edge(0).geometry = CubicBezier{{-20, 15}, {12 - sign * 2, 0}};
            outgoing.edge(1).geometry = CircularArc{sign};
            requireErrors(outgoing.document, outgoing.join(), {0});
        }
    }

    void documentErrorsPropagateInvalidArcTangents()
    {
        for (const bool incoming : {false, true})
        {
            for (const double bulge : {0.0, std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity()})
            {
                Fixture f;
                f.edge(incoming ? 0 : 1).geometry = CircularArc{bulge};
                require(!continuityErrors(f.document, f.join()), "Invalid arc G1 accepted");
            }
            Fixture coincident;
            coincident.edge(incoming ? 0 : 1).geometry = CircularArc{};
            coincident.vertex(incoming ? 0 : 2).position = coincident.vertex(1).position;
            require(!continuityErrors(coincident.document, coincident.join()), "Coincident arc G1 accepted");

            Fixture overflow;
            const double halfMaximum = std::numeric_limits<double>::max() / 2;
            overflow.vertex(1).position = {incoming ? halfMaximum : -halfMaximum, 0};
            overflow.vertex(incoming ? 0 : 2).position = {incoming ? -halfMaximum : halfMaximum, 0};
            overflow.edge(incoming ? 0 : 1).geometry = CircularArc{};
            require(!continuityErrors(overflow.document, overflow.join()), "Overflowed arc derivative accepted");
        }
    }

    void g0DoesNotRequireRepresentableDerivatives()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (int kind = 0; kind < 3; ++kind)
        {
            Fixture f;
            const double extent = kind == 0 ? maximum : maximum / 2;
            f.vertex(0).position = {-extent, 0};
            f.vertex(1).position = {extent, 0};
            f.vertex(2).position = {};
            if (kind == 1) f.edge(0).geometry = CubicBezier{{}, {-extent, 0}};
            if (kind == 2) f.edge(0).geometry = CircularArc{};
            requireErrors(f.document, f.join(Continuity::G0), {});
            require(!continuityErrors(f.document, f.join()), "Unrepresentable G1 derivative accepted");
            f.edge(0).start = VertexId{};
            require(!continuityErrors(f.document, f.join(Continuity::G0)), "G0 skipped reference checks");
        }
    }

    void documentArcJoinsRejectC1()
    {
        for (int mask = 1; mask <= 3; ++mask)
        {
            Fixture f;
            if (mask & 1) f.edge(0).geometry = CircularArc{};
            if (mask & 2) f.edge(1).geometry = CircularArc{};
            requireErrors(f.document, f.join(Continuity::G0), {});
            const auto g1 = continuityErrors(f.document, f.join());
            require(g1 && g1->size() == 1 && std::isfinite(g1->front()), "Valid arc G1 rejected");
            require(!continuityErrors(f.document, f.join(Continuity::C1)), "C1 arc join accepted");
        }
    }

    void collectingAndEvaluatingDoNotModifyDocument()
    {
        CurveTestScene scene;
        for (const auto id : scene.first.vertices)
            require(scene.document.setContinuity(id, Continuity::C1), "Read-only fixture setting failed");
        const Document before = scene.document;
        const auto joins = collected(scene.document);
        require(joins.size() == 4, "Wrong read-only fixture join count");
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            for (const auto& join : joins)
                require(continuityErrors(scene.document, join).has_value(), "Valid read-only evaluation failed");
            require(!continuityErrors(scene.document, ContinuityJoin{}), "Invalid read-only evaluation accepted");
            requireSameCurveDocument(scene.document, before);
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> continuityJoinsTests()
    {
        static const std::array tests{
            TestCase{"empty and straight documents have no active joins", emptyAndStraightDocumentsHaveNoActiveJoins},
            TestCase{"single curve includes both ends and closing join", singleCurveIncludesBothEndsAndClosingJoin},
            TestCase{"adjacent curves do not duplicate shared vertex", adjacentCurvesDoNotDuplicateTheirSharedVertex},
            TestCase{"closed curve contour has one join per vertex", closedCurveContourHasOneJoinPerVertex},
            TestCase{"G0 skips only chosen endpoints", g0SkipsOnlyItsChosenEndpoints},
            TestCase{"collector keeps G1 and C1 regardless of source", collectorKeepsG1AndC1RegardlessOfSource},
            TestCase{"multiple polygons keep their own IDs and order", multiplePolygonsKeepTheirOwnIdsAndOrder},
            TestCase{"recollection reflects conversions and settings", recollectionReflectsConversionsAndSettings},
            TestCase{"invalid polygon sizes are rejected before indexing", invalidPolygonSizesAreRejectedBeforeIndexing},
            TestCase{"missing polygon references are rejected", missingPolygonReferencesAreRejected},
            TestCase{"wrong edge connections are rejected", wrongEdgeConnectionsAreRejected},
            TestCase{"inactive joins still reject broken topology", inactiveJoinsStillRejectBrokenTopology},
            TestCase{"document errors use correct end tangents for every geometry pair", documentErrorsUseCorrectEndTangentsForEveryGeometryPair},
            TestCase{"G0 and C1 allow zero derivatives while G1 rejects them", g0AndC1AllowZeroDerivativesWhileG1RejectsThem},
            TestCase{"document errors reject unknown IDs", documentErrorsRejectUnknownIds},
            TestCase{"document errors reject wrong join orientation", documentErrorsRejectWrongJoinOrientation},
            TestCase{"document errors reject missing endpoint vertices", documentErrorsRejectMissingEndpointVertices},
            TestCase{"document errors reject non-finite endpoint positions", documentErrorsRejectNonFiniteEndpointPositions},
            TestCase{"document errors reject non-finite relevant handles", documentErrorsRejectNonFiniteRelevantHandles},
            TestCase{"same join reevaluates current handles and vertices", sameJoinReevaluatesCurrentHandlesAndVertices},
            TestCase{"one zero derivative is rejected only by G1", oneZeroDerivativeIsRejectedOnlyByG1},
            TestCase{"document errors reject derivative overflow", documentErrorsRejectDerivativeOverflow},
            TestCase{"document errors reject invalid continuity class", documentErrorsRejectInvalidContinuityClass},
            TestCase{"collecting and evaluating do not modify document", collectingAndEvaluatingDoNotModifyDocument},
            TestCase{"document errors use arc tangents with segments", documentErrorsUseArcTangentsWithSegments},
            TestCase{"document errors use arc tangents with Beziers", documentErrorsUseArcTangentsWithBeziers},
            TestCase{"document errors propagate invalid arc tangents", documentErrorsPropagateInvalidArcTangents},
            TestCase{"G0 does not require representable derivatives", g0DoesNotRequireRepresentableDerivatives},
            TestCase{"document arc joins reject C1", documentArcJoinsRejectC1}
        };
        return tests;
    }
}
