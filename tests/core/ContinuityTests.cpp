#include "../CurveTestSupport.h"

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;

    constexpr ContinuitySetting automaticG1{Continuity::G1, ContinuitySource::Default};

    struct Fixture
    {
        Document document;
        Polygon first;
        Polygon other;

        Fixture()
        {
            const std::array a{Vec2{0, 0}, Vec2{12, 0}, Vec2{12, 9}, Vec2{0, 9}};
            const std::array b{Vec2{30, 0}, Vec2{42, 0}, Vec2{42, 9}, Vec2{30, 9}};
            first = *document.findPolygon(document.addPolygon(a));
            other = *document.findPolygon(document.addPolygon(b));
            require(document.setFixedLength(first.edges[1], 9) && document.setFixedLength(other.edges[1], 9) &&
                document.addParallel(first.edges[1], other.edges[1]), "Continuity relation fixture failed");
        }
    };

    Document withSetting(Document document, VertexId vertex, ContinuitySetting setting)
    {
        const_cast<Vertex*>(document.findVertex(vertex))->continuity = setting;
        return document;
    }

    Document expectedConversion(Document document, EdgeId id, ContinuitySetting startSetting, ContinuitySetting endSetting)
    {
        auto* edge = const_cast<Edge*>(document.findEdge(id));
        const auto start = edge->start, end = edge->end;
        const Vec2 p0 = document.findVertex(start)->position, p3 = document.findVertex(end)->position;
        edge->geometry = CubicBezier{
            {(2 * p0.x + p3.x) / 3, (2 * p0.y + p3.y) / 3},
            {(p0.x + 2 * p3.x) / 3, (p0.y + 2 * p3.y) / 3}};
        const_cast<Vertex*>(document.findVertex(start))->continuity = startSetting;
        const_cast<Vertex*>(document.findVertex(end))->continuity = endSetting;
        document.removeFixedLength(id);
        document.removeParallelsForEdge(id);
        return document;
    }

    void newSettingsAndVerticesDefaultToG0()
    {
        const ContinuitySetting initial{};
        require(initial.value == Continuity::G0 && initial.source == ContinuitySource::Default, "Wrong continuity default");
        Fixture f;
        for (const auto& [id, polygon] : f.document.getPolygons())
        {
            (void)id;
            for (const VertexId vertex : polygon.vertices)
                require(f.document.findVertex(vertex)->continuity == initial, "New vertex did not receive default G0");
        }
    }

    void eachClassIsStoredAsExplicitWithoutOtherChanges()
    {
        for (const auto value : {Continuity::G0, Continuity::G1, Continuity::C1})
        for (std::size_t index = 0; index < 8; ++index)
        {
            Fixture f;
            const VertexId vertex = index < 4 ? f.first.vertices[index] : f.other.vertices[index - 4];
            const Document expected = withSetting(f.document, vertex, {value, ContinuitySource::Explicit});
            require(f.document.setContinuity(vertex, value), "Valid continuity class rejected");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void sameAutomaticValueBecomesExplicit()
    {
        Fixture f;
        require(f.document.convertEdgeToBezier(f.first.edges[0]), "Automatic G1 fixture failed");
        const VertexId vertex = f.first.vertices[0];
        require(f.document.findVertex(vertex)->continuity == automaticG1, "Fixture is not automatic G1");
        const Document expected = withSetting(f.document, vertex, {Continuity::G1, ContinuitySource::Explicit});
        require(f.document.setContinuity(vertex, Continuity::G1), "Same-value explicit choice rejected");
        requireSameCurveDocument(f.document, expected);
    }

    void explicitClassCanBeChangedAndRepeated()
    {
        Fixture f;
        const VertexId vertex = f.first.vertices[1];
        for (const auto value : {Continuity::C1, Continuity::G0, Continuity::G1, Continuity::G0})
        {
            const Document expected = withSetting(f.document, vertex, {value, ContinuitySource::Explicit});
            require(f.document.setContinuity(vertex, value) && f.document.setContinuity(vertex, value),
                "Changing or repeating an explicit class failed");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void missingVertexRejectsWithoutChanges()
    {
        Document empty;
        require(!empty.setContinuity(VertexId{1}, Continuity::G1), "Empty document accepted a continuity choice");
        Fixture f;
        const Document before = f.document;
        for (const auto id : {VertexId{}, VertexId{999999}, VertexId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!f.document.setContinuity(id, Continuity::G1), "Missing vertex accepted a continuity choice");
            requireSameCurveDocument(f.document, before);
        }
    }

    void invalidClassPreservesValueAndSource()
    {
        for (const bool explicitChoice : {false, true})
        {
            Fixture f;
            const VertexId vertex = f.first.vertices[0];
            if (explicitChoice) require(f.document.setContinuity(vertex, Continuity::C1), "Explicit fixture failed");
            const Document before = f.document;
            for (const auto value : {static_cast<Continuity>(-1), static_cast<Continuity>(3), static_cast<Continuity>(999)})
            {
                require(!f.document.setContinuity(vertex, value), "Invalid enum value accepted");
                requireSameCurveDocument(f.document, before);
            }
        }
    }

    void conversionChangesOnlyItsTwoDefaultEndpoints()
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            Fixture f;
            const EdgeId edge = index < 4 ? f.first.edges[index] : f.other.edges[index - 4];
            const Document expected = expectedConversion(f.document, edge, automaticG1, automaticG1);
            require(f.document.convertEdgeToBezier(edge), "Default conversion failed");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void conversionPreservesEveryExplicitEndpointCombination()
    {
        for (const auto start : {Continuity::G0, Continuity::G1, Continuity::C1})
        for (const auto end : {Continuity::G0, Continuity::G1, Continuity::C1})
        {
            Fixture f;
            require(f.document.setContinuity(f.first.vertices[0], start) && f.document.setContinuity(f.first.vertices[1], end),
                "Explicit endpoint fixture failed");
            const Document expected = expectedConversion(f.document, f.first.edges[0],
                {start, ContinuitySource::Explicit}, {end, ContinuitySource::Explicit});
            require(f.document.convertEdgeToBezier(f.first.edges[0]), "Explicit endpoint conversion failed");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void conversionHandlesMixedDefaultAndExplicitEndpoints()
    {
        for (const bool explicitAtStart : {false, true})
        for (const auto value : {Continuity::G0, Continuity::G1, Continuity::C1})
        {
            Fixture f;
            const auto chosen = f.first.vertices[explicitAtStart ? 0 : 1];
            require(f.document.setContinuity(chosen, value), "Mixed endpoint fixture failed");
            const ContinuitySetting explicitSetting{value, ContinuitySource::Explicit};
            const Document expected = expectedConversion(f.document, f.first.edges[0],
                explicitAtStart ? explicitSetting : automaticG1, explicitAtStart ? automaticG1 : explicitSetting);
            require(f.document.convertEdgeToBezier(f.first.edges[0]), "Mixed endpoint conversion failed");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void repeatedConversionKeepsHandlesAndSettings()
    {
        Fixture f;
        const EdgeId edge = f.first.edges[0];
        require(f.document.convertEdgeToBezier(edge) && f.document.setContinuity(f.first.vertices[0], Continuity::G0),
            "Repeated conversion fixture failed");
        require(f.document.moveControlPoint({edge, BezierControlPoint::First}, {-4, 15}) &&
            f.document.moveControlPoint({edge, BezierControlPoint::Second}, {19, -8}), "Edited controls fixture failed");
        // An existing Bezier is a no-op even when its default setting differs from conversion defaults.
        const_cast<Vertex*>(f.document.findVertex(f.first.vertices[1]))->continuity = {};
        const Document before = f.document;
        require(f.document.convertEdgeToBezier(edge) && f.document.convertEdgeToBezier(edge), "Repeated conversion failed");
        requireSameCurveDocument(f.document, before);
    }

    void missingReferencesDoNotChangeSettingsOrRelations()
    {
        Fixture f;
        require(f.document.setContinuity(f.first.vertices[0], Continuity::C1), "Rejected conversion fixture failed");
        const Document before = f.document;
        require(!f.document.convertEdgeToBezier(EdgeId{}) && !f.document.convertEdgeToBezier(EdgeId{999999}),
            "Missing edge conversion accepted");
        requireSameCurveDocument(f.document, before);
        for (const bool start : {false, true})
        {
            Document damaged = before;
            auto* edge = const_cast<Edge*>(damaged.findEdge(f.first.edges[0]));
            (start ? edge->start : edge->end) = VertexId{999999};
            const Document snapshot = damaged;
            require(!damaged.convertEdgeToBezier(f.first.edges[0]), "Missing endpoint conversion accepted");
            requireSameCurveDocument(damaged, snapshot);
        }
    }

    void nonFiniteGeometryDoesNotChangeSettingsOrRelations()
    {
        for (const bool start : {false, true})
        for (const bool x : {false, true})
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            require(f.document.setContinuity(f.first.vertices[0], Continuity::C1), "Invalid geometry fixture failed");
            auto* vertex = const_cast<Vertex*>(f.document.findVertex(f.first.vertices[start ? 0 : 1]));
            (x ? vertex->position.x : vertex->position.y) = value;
            const Document before = f.document;
            require(!f.document.convertEdgeToBezier(f.first.edges[0]), "Non-finite geometry conversion accepted");
            requireSameCurveDocument(f.document, before);
        }
        for (const double sign : {-1.0, 1.0})
        {
            Fixture f;
            const double value = sign * std::numeric_limits<double>::max();
            require(f.document.moveVertex(f.first.vertices[0], {value, 0}) &&
                f.document.moveVertex(f.first.vertices[1], {-value, 0}), "Control overflow fixture failed");
            const Document before = f.document;
            require(!f.document.convertEdgeToBezier(f.first.edges[0]), "Overflow conversion accepted");
            requireSameCurveDocument(f.document, before);
        }
    }

    void conversionBackRetainsStoredContinuity()
    {
        for (const bool explicitChoice : {false, true})
        {
            Fixture f;
            const EdgeId edge = f.first.edges[0];
            require(f.document.convertEdgeToBezier(edge), "Round trip fixture failed");
            if (explicitChoice)
                require(f.document.setContinuity(f.first.vertices[0], Continuity::G0) &&
                    f.document.setContinuity(f.first.vertices[1], Continuity::C1), "Round trip explicit fixture failed");
            Document expected = f.document;
            const_cast<Edge*>(expected.findEdge(edge))->geometry = LineSegment{};
            require(f.document.convertEdgeToSegment(edge), "Conversion back failed");
            requireSameCurveDocument(f.document, expected);
        }
    }

    void splitSegmentInitializesNewVertexAndPreservesExistingChoices()
    {
        Fixture f;
        require(f.document.setContinuity(f.first.vertices[0], Continuity::C1) &&
            f.document.setContinuity(f.first.vertices[1], Continuity::G0), "Split setting fixture failed");
        const Document before = f.document;
        const auto created = f.document.splitEdge(f.first.edges[0]);
        require(created.has_value(), "Segment split failed");
        require(f.document.findVertex(*created)->continuity == ContinuitySetting{}, "New segment vertex did not get default G0");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            (void)id;
            for (const VertexId vertex : polygon.vertices)
                require(f.document.findVertex(vertex)->continuity == before.findVertex(vertex)->continuity,
                    "Split changed an existing vertex setting");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> continuityTests()
    {
        static const std::array tests{
            TestCase{"new settings and vertices default to G0", newSettingsAndVerticesDefaultToG0},
            TestCase{"each class is stored explicitly without other changes", eachClassIsStoredAsExplicitWithoutOtherChanges},
            TestCase{"same automatic value becomes explicit", sameAutomaticValueBecomesExplicit},
            TestCase{"explicit class can be changed and repeated", explicitClassCanBeChangedAndRepeated},
            TestCase{"missing vertex rejects without changes", missingVertexRejectsWithoutChanges},
            TestCase{"invalid class preserves value and source", invalidClassPreservesValueAndSource},
            TestCase{"conversion changes only its two default endpoints", conversionChangesOnlyItsTwoDefaultEndpoints},
            TestCase{"conversion preserves every explicit endpoint combination", conversionPreservesEveryExplicitEndpointCombination},
            TestCase{"conversion handles mixed default and explicit endpoints", conversionHandlesMixedDefaultAndExplicitEndpoints},
            TestCase{"repeated conversion keeps handles and settings", repeatedConversionKeepsHandlesAndSettings},
            TestCase{"missing references preserve settings and relations", missingReferencesDoNotChangeSettingsOrRelations},
            TestCase{"non-finite geometry preserves settings and relations", nonFiniteGeometryDoesNotChangeSettingsOrRelations},
            TestCase{"conversion back retains stored continuity", conversionBackRetainsStoredContinuity},
            TestCase{"segment split initializes only the new vertex setting", splitSegmentInitializesNewVertexAndPreservesExistingChoices}
        };
        return tests;
    }
}
