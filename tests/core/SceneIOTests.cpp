#include "../TestSupport.h"
#include <cad_core/io/SceneIO.h>
#include <cad_core/demo/DemoScene.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    using namespace cad;
    using cad::tests::require;

    std::string encode(const Document& document)
    {
        std::ostringstream output;
        std::string error;
        require(io::writeScene(output, document, error), error);
        require(error.empty(), "Successful save left an error");
        return output.str();
    }

    io::SceneLoadResult decode(const std::string& text)
    {
        std::istringstream input(text);
        return io::readScene(input);
    }

    void rejected(const std::string& text)
    {
        const auto result = decode(text);
        require(!result.document && !result.error.empty(), "Invalid archive accepted or has no diagnostic");
    }

    std::string replace(std::string text, const std::string& before, const std::string& after)
    {
        const auto at = text.find(before);
        require(at != std::string::npos, "Test archive did not contain replacement target");
        text.replace(at, before.size(), after);
        return text;
    }

    std::string triangle()
    {
        return "CAD_SCENE 1\nNEXT_IDS 4 4 2\nVERTICES 3\n"
            "V 1 0 0 G0 DEFAULT\nV 2 10 0 G0 DEFAULT\nV 3 0 10 G0 DEFAULT\n"
            "EDGES 3\nE 1 1 2 LINE\nE 2 2 3 LINE\nE 3 3 1 LINE\n"
            "POLYGONS 1\nP 1 3 1 2 3 1 2 3\nLENGTHS 0\nPARALLELS 0\nEND\n";
    }

    void demoRoundTripPreservesAllPersistentState()
    {
        const auto source = createDemoScene();
        const auto text = encode(source);
        auto loaded = decode(text);
        require(loaded.document.has_value(), loaded.error);
        require(loaded.error.empty(), "Successful load left an error");
        require(encode(*loaded.document) == text, "Round trip changed persistent data");
        require(loaded.document->getPolygons().size() == 2, "Demo polygons were lost");
        require(loaded.document->getFixedLengthConstraints().size() == 3, "Lengths were lost");
        require(loaded.document->getParallelConstraints().size() == 2, "Parallel pairs were lost");
        bool bezier = false, arc = false, explicitC1 = false, defaultG1 = false;
        for (const auto& [id, polygon] : source.getPolygons())
        {
            for (const auto vertexId : polygon.vertices)
            {
                const auto* before = source.findVertex(vertexId);
                const auto* after = loaded.document->findVertex(vertexId);
                require(after && before->position == after->position && before->continuity == after->continuity,
                    "Vertex position or continuity/source changed");
                explicitC1 |= after->continuity == ContinuitySetting{Continuity::C1, ContinuitySource::Explicit};
                defaultG1 |= after->continuity == ContinuitySetting{Continuity::G1, ContinuitySource::Default};
            }
            for (const auto edgeId : polygon.edges)
            {
                const auto* before = source.findEdge(edgeId);
                const auto* after = loaded.document->findEdge(edgeId);
                require(after && before->start == after->start && before->end == after->end, "Edge IDs changed");
                if (const auto* curve = std::get_if<CubicBezier>(&before->geometry))
                {
                    const auto* actual = std::get_if<CubicBezier>(&after->geometry);
                    require(actual && actual->control1 == curve->control1 && actual->control2 == curve->control2,
                        "Bezier controls changed");
                    bezier = true;
                }
                if (const auto* curve = std::get_if<CircularArc>(&before->geometry))
                {
                    const auto* actual = std::get_if<CircularArc>(&after->geometry);
                    require(actual && actual->bulge == curve->bulge, "Arc bulge changed");
                    arc = true;
                }
            }
        }
        require(bezier && arc && explicitC1 && defaultG1, "Test did not cover all geometry and continuity kinds");
    }

    void emptySceneRoundTrip()
    {
        auto loaded = decode(encode(Document{}));
        require(loaded.document && loaded.document->getPolygons().empty(), "Empty scene failed");
        const std::array points{Vec2{0,0}, Vec2{10,0}, Vec2{0,10}};
        require(loaded.document->addPolygon(points) == PolygonId{1}, "Empty scene counter changed");
    }

    void removedIdsAndFutureAllocationsArePreserved()
    {
        Document source;
        const std::array points{Vec2{0,0}, Vec2{10,0}, Vec2{10,10}, Vec2{0,10}};
        const auto polygonId = source.addPolygon(points);
        const auto extra = source.splitEdge(source.findPolygon(polygonId)->edges[0]);
        require(extra && source.deleteVertex(*extra), "Could not create ID gaps");
        auto loaded = decode(encode(source));
        require(loaded.document.has_value(), loaded.error);
        source.addPolygon(points);
        loaded.document->addPolygon(points);
        require(encode(source) == encode(*loaded.document), "Allocation counters were not preserved");
        const auto edge = source.findPolygon(polygonId)->edges[0];
        require(source.splitEdge(edge) == loaded.document->splitEdge(edge), "Split reused an ID differently");
    }

    void doublesAndNegativeArcRoundTripExactly()
    {
        Document document;
        const std::array points{Vec2{std::nextafter(0.1, 1.0), -123.12345678901234}, Vec2{20, 0}, Vec2{0, 20}};
        const auto id = document.addPolygon(points);
        const auto edge = document.findPolygon(id)->edges[0];
        require(document.convertEdgeToArc(edge) && document.setArcBulge(edge, -1), "Could not create negative arc");
        auto loaded = decode(encode(document));
        require(loaded.document.has_value(), loaded.error);
        require(loaded.document->findVertex(document.findPolygon(id)->vertices[0])->position == points[0], "Double precision lost");
        require(std::get<CircularArc>(loaded.document->findEdge(edge)->geometry).bulge == -1, "Negative bulge lost");
    }

    void streamLocaleDoesNotAffectFormat()
    {
        struct Comma : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
        const std::locale comma(std::locale::classic(), new Comma);
        const auto document = createDemoScene();
        std::ostringstream output;
        output.imbue(comma);
        std::string error;
        require(io::writeScene(output, document, error), error);
        require(output.str() == encode(document), "Save depends on caller locale");
        std::istringstream input(output.str());
        input.imbue(comma);
        const auto result = io::readScene(input);
        require(result.document.has_value(), result.error);
        require(output.getloc() == comma && input.getloc() == comma, "Codec changed caller locale");
    }

    void badHeaderVersionAndTrailingDataAreRejected()
    {
        rejected("");
        rejected(replace(triangle(), "CAD_SCENE 1", "OTHER 1"));
        rejected(replace(triangle(), "CAD_SCENE 1", "CAD_SCENE 3"));
        rejected(triangle() + "garbage");
        rejected(replace(triangle(), "END", "STOP"));
    }

    void everyTruncatedTokenPrefixIsRejected()
    {
        const auto text = triangle();
        std::istringstream tokens(text);
        std::string item, prefix;
        while (tokens >> item)
        {
            if (item == "END") break;
            prefix += item + " ";
            rejected(prefix);
        }
        require(decode(text + "\r\n  ").document.has_value(), "Trailing whitespace rejected");
    }

    void unknownKindsAreRejected()
    {
        rejected(replace(triangle(), "G0 DEFAULT", "G2 DEFAULT"));
        rejected(replace(triangle(), "G0 DEFAULT", "G0 OTHER"));
        rejected(replace(triangle(), "E 1 1 2 LINE", "E 1 1 2 SPLINE"));
    }

    void invalidNumbersAndCountersAreRejected()
    {
        for (const auto* value : {"nan", "inf", "1e999", "1,2", "abc"})
            rejected(replace(triangle(), "V 1 0 0", std::string("V 1 ") + value + " 0"));
        rejected(replace(triangle(), "V 1 0 0", "V -1 0 0"));
        rejected(replace(triangle(), "NEXT_IDS 4 4 2", "NEXT_IDS 3 4 2"));
        rejected(replace(triangle(), "NEXT_IDS 4 4 2", "NEXT_IDS 0 4 2"));
        rejected(replace(triangle(), "NEXT_IDS 4 4 2", "NEXT_IDS 18446744073709551615 4 2"));
        rejected(replace(triangle(), "VERTICES 3", "VERTICES 100001"));
        rejected(replace(triangle(), "VERTICES 3", "VERTICES -3"));
    }

    void duplicateIdsMissingReferencesAndBadCyclesAreRejected()
    {
        rejected(replace(triangle(), "V 3 0 10", "V 2 0 10"));
        rejected(replace(triangle(), "E 3 3 1", "E 2 3 1"));
        rejected(replace(triangle(), "E 3 3 1", "E 3 3 99"));
        rejected(replace(triangle(), "P 1 3 1 2 3 1 2 3", "P 1 3 1 2 3 1 2 2"));
        rejected(replace(triangle(), "P 1 3 1 2 3 1 2 3", "P 1 3 1 3 2 1 2 3"));
        rejected(replace(triangle(), "POLYGONS 1\nP 1 3 1 2 3 1 2 3", "POLYGONS 0"));
        rejected(replace(triangle(), "P 1 3", "P 1 100000"));
    }

    void invalidCurveGeometryAndContinuityAreRejected()
    {
        rejected(replace(triangle(), "E 1 1 2 LINE", "E 1 1 2 ARC 0"));
        rejected(replace(triangle(), "E 1 1 2 LINE", "E 1 1 2 ARC 0.5"));
        rejected(replace(triangle(), "E 1 1 2 LINE", "E 1 1 2 BEZIER nan 0 1 0"));
        auto badJoin = replace(triangle(), "E 1 1 2 LINE", "E 1 1 2 BEZIER 3 0 7 0");
        rejected(replace(badJoin, "V 1 0 0 G0 DEFAULT", "V 1 0 0 G1 EXPLICIT"));
    }

    void invalidLengthsAndParallelsAreRejected()
    {
        for (const auto* relation : {"L 1 -1", "L 99 10", "L 1 11"})
            rejected(replace(triangle(), "LENGTHS 0", std::string("LENGTHS 1\n") + relation));
        rejected(replace(triangle(), "LENGTHS 0", "LENGTHS 2\nL 1 10\nL 1 10"));
        for (const auto* relation : {"R 1 2", "R 1 1", "R 1 99"})
            rejected(replace(triangle(), "PARALLELS 0", std::string("PARALLELS 1\n") + relation));
        auto valid = createDemoScene();
        const auto text = encode(valid);
        const auto first = valid.getParallelConstraints().front();
        const auto duplicate = "R " + std::to_string(first.second.value) + " " + std::to_string(first.first.value) + "\nEND";
        rejected(replace(replace(text, "PARALLELS 2", "PARALLELS 3"), "END", duplicate));
    }

    void invalidDocumentIsNotWritten()
    {
        auto document = createDemoScene();
        const auto relation = document.getFixedLengthConstraints().front();
        require(document.setFixedLength(relation.edge, relation.length + 20), "Could not prepare inconsistent model");
        std::ostringstream output;
        output << "existing";
        std::string error;
        require(!io::writeScene(output, document, error) && !error.empty(), "Inconsistent model was saved");
        require(output.str() == "existing", "Validation failure wrote partial content");
    }

    void streamFailuresAreReported()
    {
        std::istringstream input(triangle());
        input.setstate(std::ios::badbit);
        require(!io::readScene(input).document, "Read failure ignored");
        std::ostringstream output;
        output.setstate(std::ios::badbit);
        std::string error;
        require(!io::writeScene(output, Document{}, error) && !error.empty(), "Write failure ignored");
    }

    void oversizedFileIsRejected()
    {
        rejected(std::string(16 * 1024 * 1024 + 1, ' '));
    }
}

namespace cad::tests
{
    std::span<const TestCase> sceneIOTests()
    {
        static const std::array tests{
            TestCase{"demo exact round trip with all persistent state", demoRoundTripPreservesAllPersistentState},
            TestCase{"empty scene round trip", emptySceneRoundTrip},
            TestCase{"ID gaps and future allocations survive loading", removedIdsAndFutureAllocationsArePreserved},
            TestCase{"double precision and negative arc preserved", doublesAndNegativeArcRoundTripExactly},
            TestCase{"format is independent of stream locale", streamLocaleDoesNotAffectFormat},
            TestCase{"bad header version and trailing data rejected", badHeaderVersionAndTrailingDataAreRejected},
            TestCase{"all truncated token prefixes rejected", everyTruncatedTokenPrefixIsRejected},
            TestCase{"unknown geometry and continuity rejected", unknownKindsAreRejected},
            TestCase{"invalid numbers counters and counts rejected", invalidNumbersAndCountersAreRejected},
            TestCase{"duplicates missing references and bad cycles rejected", duplicateIdsMissingReferencesAndBadCyclesAreRejected},
            TestCase{"invalid curves and continuity rejected", invalidCurveGeometryAndContinuityAreRejected},
            TestCase{"invalid length and parallel constraints rejected", invalidLengthsAndParallelsAreRejected},
            TestCase{"invalid document produces no partial output", invalidDocumentIsNotWritten},
            TestCase{"stream failures reported", streamFailuresAreReported},
            TestCase{"oversized file rejected", oversizedFileIsRejected}
        };
        return tests;
    }
}
