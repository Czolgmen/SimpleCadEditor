#include "../TestSupport.h"
#include <cad_core/io/SceneIO.h>
#include <cad_core/demo/DemoScene.hpp>
#include <editor/EditorSession.h>

#include <algorithm>
#include <array>
#include <sstream>

namespace
{
    using namespace cad;
    using cad::tests::require;

    Document square()
    {
        Document document;
        const std::array points{Vec2{0,0},Vec2{100,0},Vec2{100,100},Vec2{0,100}};
        document.addPolygon(points);
        return document;
    }
    std::string encode(const Document& document)
    {
        std::ostringstream output;
        std::string error;
        require(io::writeScene(output,document,error),error);
        return output.str();
    }
    io::SceneLoadResult decode(const std::string& text)
    {
        std::istringstream input(text);
        return io::readScene(input);
    }
    void changesOnlyOneEdgeWithoutMovingPoints()
    {
        auto document = createDemoScene();
        const auto original = document;
        const auto edge = document.getPolygons().begin()->second.edges[0];
        require(document.setEdgeStrokeWidth(edge,32), "Valid width rejected");
        for (const auto& [id,polygon] : document.getPolygons())
        {
            for (const auto vertex : polygon.vertices)
                require(document.findVertex(vertex)->position == original.findVertex(vertex)->position, "Style moved a vertex");
            for (const auto other : polygon.edges)
                require(document.findEdge(other)->strokeWidth == (other == edge ? 32 : 2), "Style changed another edge");
        }
        require(document.setEdgeStrokeWidth(edge,1) && document.findEdge(edge)->strokeWidth == 1, "Minimum width rejected");
        require(document.getFixedLengthConstraints().size() == original.getFixedLengthConstraints().size() &&
            document.getParallelConstraints().size() == original.getParallelConstraints().size(), "Style changed relations");
    }
    void invalidWidthsAndIdsDoNotMutateAndAreValidated()
    {
        auto document = square();
        const auto edge = document.getPolygons().begin()->second.edges[0];
        const auto before = encode(document);
        for (const int width : {-10,0,33,1000000}) require(!document.setEdgeStrokeWidth(edge,width), "Invalid width accepted");
        require(!document.setEdgeStrokeWidth(EdgeId{},3) && !document.setEdgeStrokeWidth(EdgeId{9999},3), "Missing edge accepted");
        require(encode(document) == before, "Rejected width mutated model");
        const_cast<Edge*>(document.findEdge(edge))->strokeWidth = 0;
        const auto issues = document.validate();
        require(std::any_of(issues.begin(),issues.end(),[](const auto& issue) { return issue.code == ValidationError::InvalidStrokeWidth; }),
            "Validator missed corrupt style");
        std::ostringstream output;
        std::string error;
        require(!io::writeScene(output,document,error) && output.str().empty(), "Corrupt style was saved");
    }
    void allSplitGeometriesInheritWidth()
    {
        for (int kind = 0; kind < 3; ++kind)
        {
            auto document = square();
            const auto polygon = document.getPolygons().begin()->second;
            const auto edge = polygon.edges[0];
            if (kind == 1) require(document.convertEdgeToBezier(edge), "Could not prepare Bezier");
            if (kind == 2) require(document.convertEdgeToArc(edge), "Could not prepare arc");
            require(document.setEdgeStrokeWidth(edge,11), "Could not set width");
            require(document.splitEdge(edge).has_value(), "Split failed");
            const auto& result = *document.findPolygon(polygon.id);
            require(document.findEdge(result.edges[0])->strokeWidth == 11 && document.findEdge(result.edges[1])->strokeWidth == 11,
                "Split halves lost width");
            require(document.findEdge(result.edges[2])->strokeWidth == 2, "Split changed neighbour width");
        }
    }
    void conversionAndDeletionHaveDefinedStyleInheritance()
    {
        auto document = square();
        const auto polygon = document.getPolygons().begin()->second;
        const auto edge = polygon.edges[0];
        require(document.setEdgeStrokeWidth(edge,13) && document.setEdgeStrokeWidth(polygon.edges[1],7), "Width setup failed");
        require(document.convertEdgeToBezier(edge) && document.findEdge(edge)->strokeWidth == 13, "Bezier conversion lost width");
        require(document.convertEdgeToSegment(edge), "Segment conversion failed");
        for (const auto vertex : polygon.vertices) require(document.setContinuity(vertex,Continuity::G0), "G0 setup failed");
        require(document.convertEdgeToArc(edge) && document.findEdge(edge)->strokeWidth == 13, "Arc conversion lost width");
        require(document.convertEdgeToSegment(edge) && document.findEdge(edge)->strokeWidth == 13, "Segment conversion lost width");
        require(document.deleteVertex(polygon.vertices[1]), "Delete failed");
        const auto& result = *document.findPolygon(polygon.id);
        require(document.findEdge(result.edges[0])->strokeWidth == 13, "Merged edge did not inherit incoming width");
    }
    void sessionChangesStyleAndPreservesGeometryAndSelection()
    {
        auto document = square();
        const auto polygon = document.getPolygons().begin()->second;
        require(document.setFixedLength(polygon.edges[0],100), "Could not prepare length");
        editor::EditorSession session(document);
        require(!session.selectedEdgeStrokeWidth() && !session.setSelectedEdgeStrokeWidth(8), "No selection accepted width");
        require(session.selectAt({50,0},0) && session.beginParallelSelection(), "Could not select edge");
        require(session.selectedEdgeStrokeWidth() == 2 && !session.setSelectedEdgeStrokeWidth(33), "Bad session width handling");
        require(session.pendingParallelEdge().has_value(), "Rejected style cancelled interaction");
        require(session.setSelectedEdgeStrokeWidth(8) && session.selectedEdgeStrokeWidth() == 8, "Session style failed");
        require(session.selection().edge == polygon.edges[0] && !session.pendingParallelEdge(), "Successful style lost selection or retained pending relation");
        for (const auto vertex : polygon.vertices)
            require(session.document().findVertex(vertex)->position == document.findVertex(vertex)->position, "Style solved or moved geometry");
        require(session.document().getFixedLengthConstraints()[0].length == 100, "Style changed length constraint");
        require(session.selectAt({0,0},0) && !session.setSelectedEdgeStrokeWidth(3), "Vertex selection accepted edge style");
    }
    void versionOneScenesDefaultToWidthTwo()
    {
        const auto loaded = decode("CAD_SCENE 1\nNEXT_IDS 4 4 2\nVERTICES 3\n"
            "V 1 0 0 G0 DEFAULT\nV 2 10 0 G0 DEFAULT\nV 3 0 10 G0 DEFAULT\n"
            "EDGES 3\nE 1 1 2 LINE\nE 2 2 3 LINE\nE 3 3 1 LINE\n"
            "POLYGONS 1\nP 1 3 1 2 3 1 2 3\nLENGTHS 0\nPARALLELS 0\nEND\n");
        require(loaded.document.has_value(), loaded.error);
        for (const auto edge : loaded.document->getPolygons().begin()->second.edges)
            require(loaded.document->findEdge(edge)->strokeWidth == 2, "Legacy scene has wrong default width");
        require(encode(*loaded.document).starts_with("CAD_SCENE 2\n"), "Save did not upgrade archive version");
    }
    void versionTwoRoundTripPreservesIndependentWidths()
    {
        auto document = createDemoScene();
        int width = 1;
        for (const auto& [id,polygon] : document.getPolygons())
            for (const auto edge : polygon.edges) require(document.setEdgeStrokeWidth(edge,width++), "Could not set widths");
        const auto text = encode(document);
        const auto loaded = decode(text);
        require(loaded.document.has_value(), loaded.error);
        require(encode(*loaded.document) == text, "Round trip changed widths or other scene data");
        for (const auto& [id,polygon] : document.getPolygons())
            for (const auto edge : polygon.edges)
                require(loaded.document->findEdge(edge)->strokeWidth == document.findEdge(edge)->strokeWidth, "Edge width lost on load");
    }
    void invalidVersionTwoWidthsAreRejected()
    {
        const auto text = encode(square());
        const auto pos = text.find("E 1 1 2 2 LINE");
        require(pos != std::string::npos, "Missing version 2 record");
        for (const auto* width : {"0","-1","33","1.5","nan","999999999999999999999999"})
        {
            auto bad = text;
            bad.replace(pos,std::string("E 1 1 2 2 LINE").size(), std::string("E 1 1 2 ")+width+" LINE");
            const auto result = decode(bad);
            require(!result.document && !result.error.empty(), "Invalid persisted width accepted");
        }
    }
}

namespace cad::tests
{
    std::span<const TestCase> edgeStyleTests()
    {
        static const std::array tests{
            TestCase{"width changes only one edge without moving points",changesOnlyOneEdgeWithoutMovingPoints},
            TestCase{"invalid widths and IDs preserve state and validate",invalidWidthsAndIdsDoNotMutateAndAreValidated},
            TestCase{"split segments Beziers and arcs inherit width",allSplitGeometriesInheritWidth},
            TestCase{"conversion and deletion preserve defined style",conversionAndDeletionHaveDefinedStyleInheritance},
            TestCase{"session style preserves geometry and selection",sessionChangesStyleAndPreservesGeometryAndSelection},
            TestCase{"version 1 scenes default to width two",versionOneScenesDefaultToWidthTwo},
            TestCase{"version 2 preserves independent widths",versionTwoRoundTripPreservesIndependentWidths},
            TestCase{"invalid persisted widths are rejected",invalidVersionTwoWidthsAreRejected}
        };
        return tests;
    }
}
