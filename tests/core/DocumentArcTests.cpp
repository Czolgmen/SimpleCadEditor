#include "../CurveTestSupport.h"

#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;

    struct Fixture : CurveTestScene
    {
        EdgeId arcId;

        explicit Fixture(std::size_t index = 0)
        {
            arcId = index < 4 ? first.edges[index] : other.edges[index - 4];
            document.removeFixedLength(arcId);
            document.removeParallelsForEdge(arcId);
            // Independent fixture until arc conversion is exposed by Document.
            edge().geometry = CircularArc{-1};
        }

        Edge& edge()
        {
            return *const_cast<Edge*>(document.findEdge(arcId));
        }

        Vertex& endpoint(bool end)
        {
            return *const_cast<Vertex*>(document.findVertex(end ? edge().end : edge().start));
        }
    };

    void requireSameStateAndCounters(const Document& actual, const Document& expected)
    {
        requireSameCurveDocument(actual, expected);
        // Observe all three private ID counters by allocating on disposable copies.
        Document actualProbe = actual, expectedProbe = expected;
        const std::array positions{Vec2{200, 200}, Vec2{220, 200}, Vec2{200, 220}};
        require(actualProbe.addPolygon(positions) == expectedProbe.addPolygon(positions),
            "setArcBulge consumed polygon IDs");
        requireSameCurveDocument(actualProbe, expectedProbe);
    }

    void requireRejected(Fixture& fixture, double bulge, EdgeId id)
    {
        const Document before = fixture.document;
        require(!fixture.document.setArcBulge(id, bulge), "Invalid arc update accepted");
        requireSameStateAndCounters(fixture.document, before);
    }

    void requireUpdated(Fixture& fixture, double bulge)
    {
        Document expected = fixture.document;
        std::get<CircularArc>(const_cast<Edge*>(expected.findEdge(fixture.arcId))->geometry).bulge = bulge;
        require(fixture.document.setArcBulge(fixture.arcId, bulge), "Valid arc update rejected");
        requireSameStateAndCounters(fixture.document, expected);
        require(fixture.document.validate().empty(), "Valid arc update left invalid model geometry");
    }

    void acceptsBothSemicircleSignsOnEveryEdge()
    {
        for (std::size_t index = 0; index < 8; ++index)
        for (const auto startSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        for (const auto endSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f(index);
            f.endpoint(false).continuity = {Continuity::G0, startSource};
            f.endpoint(true).continuity = {Continuity::G0, endSource};
            // Include repeated writes and both changes of side.
            for (const double bulge : {-1.0, 1.0, 1.0, -1.0})
                requireUpdated(f, bulge);
        }
    }

    void rejectsNonSemicirclesWithTwoG0Ends()
    {
        Fixture f;
        for (const double bulge : {-8.0, -2.0, -0.5, 0.125, 0.5, 2.0, 8.0,
            std::nextafter(1.0, 0.0), std::nextafter(1.0, 2.0),
            std::nextafter(-1.0, 0.0), std::nextafter(-1.0, -2.0)})
            requireRejected(f, bulge, f.arcId);
    }

    void acceptsGeneralBulgesWithOneG1AtEitherEnd()
    {
        for (const bool end : {false, true})
        for (const auto startSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        for (const auto endSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            f.endpoint(false).continuity = {end ? Continuity::G0 : Continuity::G1, startSource};
            f.endpoint(true).continuity = {end ? Continuity::G1 : Continuity::G0, endSource};
            for (const double bulge : {-8.0, -2.0, -1.0, -0.5, -0.125, 0.125, 0.5, 1.0, 2.0, 8.0})
                requireUpdated(f, bulge);
        }
    }

    void rejectsUnknownIdsInEmptyAndPopulatedDocuments()
    {
        for (const EdgeId id : {EdgeId{}, EdgeId{99999}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
        {
            Document empty;
            const Document before = empty;
            require(!empty.setArcBulge(id, 1), "Unknown edge in empty document accepted");
            requireSameStateAndCounters(empty, before);
            Fixture f;
            requireRejected(f, 1, id);
        }
    }

    void rejectsSegmentsAndBeziers()
    {
        Fixture f;
        for (const EdgeId id : {f.first.edges[1], f.first.edges[2], f.other.edges[0]})
            requireRejected(f, 1, id);
    }

    void rejectsMissingEndpoints()
    {
        for (int mask = 1; mask <= 3; ++mask)
        for (const VertexId missing : {VertexId{}, VertexId{99999}})
        {
            Fixture f;
            if (mask & 1) f.edge().start = missing;
            if (mask & 2) f.edge().end = missing;
            requireRejected(f, 1, f.arcId);
        }
    }

    void rejectsCoincidentPositionsOrIds()
    {
        for (const bool sameId : {false, true})
        {
            Fixture f;
            if (sameId) f.edge().end = f.edge().start;
            else f.endpoint(true).position = f.endpoint(false).position;
            requireRejected(f, 1, f.arcId);
        }
    }

    void rejectsNonfiniteEndpointCoordinates()
    {
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        for (const bool end : {false, true})
        for (const bool y : {false, true})
        {
            Fixture f;
            f.endpoint(false).continuity.value = Continuity::G1;
            auto& position = f.endpoint(end).position;
            (y ? position.y : position.x) = invalid;
            requireRejected(f, 0.5, f.arcId);
        }
    }

    void rejectsInvalidOrUnrepresentableBulges()
    {
        for (const bool end : {false, true})
        {
            Fixture f;
            f.endpoint(end).continuity.value = Continuity::G1;
            for (const double bulge : {0.0, -0.0, std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min(),
                std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()})
                requireRejected(f, bulge, f.arcId);
        }
    }

    void rejectsC1AndUnknownContinuityClasses()
    {
        for (const bool end : {false, true})
        for (const auto value : {Continuity::C1, static_cast<Continuity>(-1), static_cast<Continuity>(3)})
        for (const auto source : {ContinuitySource::Default, ContinuitySource::Explicit})
        for (const auto opposite : {Continuity::G0, Continuity::G1})
        {
            Fixture f;
            f.endpoint(end).continuity = {value, source};
            f.endpoint(!end).continuity.value = opposite;
            requireRejected(f, 1, f.arcId);
        }
    }

    void rejectsG1AtBothEndsRegardlessOfSource()
    {
        for (const auto startSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        for (const auto endSource : {ContinuitySource::Default, ContinuitySource::Explicit})
        {
            Fixture f;
            f.endpoint(false).continuity = {Continuity::G1, startSource};
            f.endpoint(true).continuity = {Continuity::G1, endSource};
            for (const double bulge : {-2.0, -1.0, 0.5, 1.0, 2.0})
                requireRejected(f, bulge, f.arcId);
        }
    }

    void rejectsUnrepresentableCircleParameters()
    {
        const double maximum = std::numeric_limits<double>::max();
        Fixture radius;
        radius.endpoint(false).position = {-maximum, -maximum};
        radius.endpoint(true).position = {maximum, maximum};
        for (const double bulge : {-1.0, 1.0})
            requireRejected(radius, bulge, radius.arcId);

        Fixture center;
        center.endpoint(false).continuity.value = Continuity::G1;
        center.endpoint(false).position = {-maximum * 0.75, maximum * 0.75};
        center.endpoint(true).position = {maximum * 0.75, maximum * 0.75};
        requireRejected(center, 0.125, center.arcId);
    }

    void changesOnlyTargetArcAndPreservesOtherGeometryAndRelations()
    {
        Fixture f;
        f.endpoint(false).continuity = {Continuity::G1, ContinuitySource::Explicit};
        const_cast<Edge*>(f.document.findEdge(f.other.edges[2]))->geometry = CircularArc{1};
        requireUpdated(f, 2);
        requireUpdated(f, -0.5);
    }

    void canReplaceInvalidStoredBulge()
    {
        for (const double previous : {0.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity()})
        {
            Fixture f;
            std::get<CircularArc>(f.edge().geometry).bulge = previous;
            requireRejected(f, 0.5, f.arcId);
            requireUpdated(f, 1);
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> documentArcTests()
    {
        static const std::array tests{
            TestCase{"G0 semicircles on every edge and repeated updates", acceptsBothSemicircleSignsOnEveryEdge},
            TestCase{"non-semicircles rejected at two G0 ends", rejectsNonSemicirclesWithTwoG0Ends},
            TestCase{"general bulges accepted with one G1 at either end", acceptsGeneralBulgesWithOneG1AtEitherEnd},
            TestCase{"unknown IDs rejected in empty and populated documents", rejectsUnknownIdsInEmptyAndPopulatedDocuments},
            TestCase{"segments and Beziers rejected", rejectsSegmentsAndBeziers},
            TestCase{"missing endpoints rejected", rejectsMissingEndpoints},
            TestCase{"coincident positions or IDs rejected", rejectsCoincidentPositionsOrIds},
            TestCase{"nonfinite endpoint coordinates rejected", rejectsNonfiniteEndpointCoordinates},
            TestCase{"invalid or unrepresentable bulges rejected", rejectsInvalidOrUnrepresentableBulges},
            TestCase{"C1 and unknown continuity classes rejected", rejectsC1AndUnknownContinuityClasses},
            TestCase{"G1 at both ends rejected regardless of source", rejectsG1AtBothEndsRegardlessOfSource},
            TestCase{"unrepresentable circle parameters rejected", rejectsUnrepresentableCircleParameters},
            TestCase{"only target arc changes with other geometry and relations preserved", changesOnlyTargetArcAndPreservesOtherGeometryAndRelations},
            TestCase{"invalid stored bulge can be replaced", canReplaceInvalidStoredBulge}
        };
        return tests;
    }
}
