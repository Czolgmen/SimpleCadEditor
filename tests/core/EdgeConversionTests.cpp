#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    using namespace cad;
    using cad::tests::require;

    struct Scene
    {
        Document document;
        std::array<EdgeId, 8> edges;

        Scene()
        {
            const std::array first{Vec2{0, 0}, Vec2{12, 0}, Vec2{12, 9}, Vec2{0, 9}};
            const std::array second{Vec2{-30, -9}, Vec2{-12, 3}, Vec2{-24, 18}, Vec2{-42, 6}};
            const auto a = document.addPolygon(first);
            const auto b = document.addPolygon(second);
            for (std::size_t i = 0; i < 4; ++i)
            {
                edges[i] = document.findPolygon(a)->edges[i];
                edges[i + 4] = document.findPolygon(b)->edges[i];
            }
        }

        void constrainAll()
        {
            for (const EdgeId edge : edges)
                require(document.setFixedLength(edge, 100 + static_cast<double>(edge.value)), "Length fixture failed");
            for (std::size_t i = 0; i < edges.size(); ++i)
                for (std::size_t j = i + 1; j < edges.size(); ++j)
                    require((i + j) % 2 == 0 ? document.addParallel(edges[i], edges[j])
                        : document.addParallel(edges[j], edges[i]), "Parallel fixture failed");
        }
    };

    bool sameScalar(double a, double b)
    {
        return a == b || (std::isnan(a) && std::isnan(b));
    }

    void requirePoint(Vec2 actual, Vec2 expected)
    {
        require(sameScalar(actual.x, expected.x) && sameScalar(actual.y, expected.y), "Point changed unexpectedly");
    }

    void requireNear(Vec2 actual, Vec2 expected)
    {
        const double scale = (std::max)({1.0, std::abs(expected.x), std::abs(expected.y)});
        require(std::isfinite(actual.x) && std::isfinite(actual.y) &&
            std::abs(actual.x - expected.x) <= 1e-12 * scale && std::abs(actual.y - expected.y) <= 1e-12 * scale,
            "Initial Bezier controls are not at one and two thirds of the segment");
    }

    // Checks malformed fixtures too: comparison must not dereference broken edge endpoints.
    void requireDocument(const Document& actual, const Document& before,
        EdgeId changedGeometry = {}, EdgeId removedRelations = {})
    {
        require(actual.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* after = actual.findPolygon(id);
            require(after && after->id == polygon.id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Conversion changed polygon topology");
            for (const VertexId vertexId : polygon.vertices)
            {
                const auto* a = actual.findVertex(vertexId);
                const auto* b = before.findVertex(vertexId);
                require(a && b && a->id == b->id, "Conversion changed a vertex ID");
                requirePoint(a->position, b->position);
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const auto* a = actual.findEdge(edgeId);
                const auto* b = before.findEdge(edgeId);
                require(a && b && a->id == b->id && a->start == b->start && a->end == b->end,
                    "Conversion changed an edge ID or its endpoints");
                if (edgeId == changedGeometry) continue;
                require(a->geometry.index() == b->geometry.index(), "Unrelated edge type changed");
                if (const auto* bezier = std::get_if<CubicBezier>(&b->geometry))
                {
                    const auto& converted = std::get<CubicBezier>(a->geometry);
                    requirePoint(converted.control1, bezier->control1);
                    requirePoint(converted.control2, bezier->control2);
                }
            }
        }
        std::vector<constraint::FixedLengthConstraint> lengths;
        for (const auto& relation : before.getFixedLengthConstraints())
            if (relation.edge != removedRelations) lengths.push_back(relation);
        const auto actualLengths = actual.getFixedLengthConstraints();
        require(actualLengths.size() == lengths.size(), "Wrong retained length count");
        for (std::size_t i = 0; i < lengths.size(); ++i)
            require(actualLengths[i].edge == lengths[i].edge && actualLengths[i].length == lengths[i].length,
                "Unrelated length changed or order was lost");

        std::vector<constraint::ParallelConstraint> pairs;
        for (const auto& relation : before.getParallelConstraints())
            if (relation.first != removedRelations && relation.second != removedRelations) pairs.push_back(relation);
        const auto actualPairs = actual.getParallelConstraints();
        require(actualPairs.size() == pairs.size(), "Wrong retained parallel count");
        for (std::size_t i = 0; i < pairs.size(); ++i)
            require(actualPairs[i].first == pairs[i].first && actualPairs[i].second == pairs[i].second,
                "Unrelated parallel pair changed or order was lost");
    }

    void requireInitialControls(const Document& document, EdgeId id)
    {
        const auto* edge = document.findEdge(id);
        require(edge && std::holds_alternative<CubicBezier>(edge->geometry), "Converted edge is not a Bezier");
        const auto& curve = std::get<CubicBezier>(edge->geometry);
        const auto p0 = document.findVertex(edge->start)->position;
        const auto p3 = document.findVertex(edge->end)->position;
        // Independent weighted averages instead of the implementation's direction vector.
        requireNear(curve.control1, {(2 * p0.x + p3.x) / 3, (2 * p0.y + p3.y) / 3});
        requireNear(curve.control2, {(p0.x + 2 * p3.x) / 3, (p0.y + 2 * p3.y) / 3});
    }

    void convertsEveryEdgeWithCorrectControlsAndStableTopology()
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            Scene scene;
            const Document before = scene.document;
            const EdgeId edge = scene.edges[index];
            require(scene.document.convertEdgeToBezier(edge), "Valid edge conversion failed");
            requireInitialControls(scene.document, edge);
            requireDocument(scene.document, before, edge);
            require(scene.document.validate().empty(), "Conversion left invalid topology");
        }
    }

    void removesOnlyConvertedEdgesLengthsAndAllIncidentPairs()
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            Scene scene;
            scene.constrainAll();
            const Document before = scene.document;
            const EdgeId edge = scene.edges[index];
            require(scene.document.convertEdgeToBezier(edge), "Constrained conversion failed");
            requireInitialControls(scene.document, edge);
            requireDocument(scene.document, before, edge, edge);
            require(scene.document.getFixedLengthConstraints().size() == 7 && scene.document.getParallelConstraints().size() == 21,
                "Conversion did not remove exactly one length and seven incident pairs");
        }
    }

    void unconstrainedConversionPreservesOtherCurvesAndRelations()
    {
        Scene scene;
        require(scene.document.convertEdgeToBezier(scene.edges[6]), "Unrelated curve fixture failed");
        auto* unrelated = const_cast<Edge*>(scene.document.findEdge(scene.edges[6]));
        unrelated->geometry = CubicBezier{{-7.25, 40}, {-90, -11.5}};
        require(scene.document.setFixedLength(scene.edges[1], 9) && scene.document.setFixedLength(scene.edges[4], 15),
            "Unrelated length fixture failed");
        require(scene.document.addParallel(scene.edges[1], scene.edges[2]) && scene.document.addParallel(scene.edges[4], scene.edges[5]),
            "Unrelated pair fixture failed");
        const Document before = scene.document;
        require(scene.document.convertEdgeToBezier(scene.edges[0]), "Unconstrained conversion failed");
        requireDocument(scene.document, before, scene.edges[0]);
    }

    void repeatedBezierConversionPreservesEditedHandles()
    {
        Scene scene;
        const EdgeId id = scene.edges[0];
        require(scene.document.convertEdgeToBezier(id), "Bezier fixture failed");
        auto* edge = const_cast<Edge*>(scene.document.findEdge(id));
        edge->geometry = CubicBezier{{-4.25, 19}, {27, -8.5}};
        require(scene.document.moveVertex(edge->end, {21, 6}), "Edited endpoint fixture failed");
        const Document before = scene.document;
        require(scene.document.convertEdgeToBezier(id) && scene.document.convertEdgeToBezier(id), "Repeated conversion failed");
        requireDocument(scene.document, before);
    }

    void conversionBackChangesOnlySelectedGeometry()
    {
        Scene scene;
        require(scene.document.convertEdgeToBezier(scene.edges[0]) && scene.document.convertEdgeToBezier(scene.edges[5]),
            "Curve fixtures failed");
        const Document before = scene.document;
        require(scene.document.convertEdgeToSegment(scene.edges[0]), "Conversion back failed");
        require(std::holds_alternative<LineSegment>(scene.document.findEdge(scene.edges[0])->geometry), "Edge is not a segment");
        requireDocument(scene.document, before, scene.edges[0]);
    }

    void existingSegmentConversionPreservesItsConstraints()
    {
        Scene scene;
        scene.constrainAll();
        const Document before = scene.document;
        for (const EdgeId edge : scene.edges)
            require(scene.document.convertEdgeToSegment(edge), "Repeated segment conversion failed");
        requireDocument(scene.document, before);
    }

    void roundTripDoesNotRestoreRemovedRelations()
    {
        Scene scene;
        scene.constrainAll();
        const Document before = scene.document;
        const EdgeId id = scene.edges[0];
        require(scene.document.convertEdgeToBezier(id) && scene.document.convertEdgeToSegment(id), "Round trip failed");
        requireDocument(scene.document, before, {}, id);
        require(!scene.document.removeFixedLength(id), "Round trip restored an old length");
        require(!scene.document.removeParallel(id, scene.edges[1]), "Round trip restored an old parallel pair");
    }

    void lengthEvaluatorAndStorageRejectBezier()
    {
        Scene scene;
        require(scene.document.setFixedLength(scene.edges[1], 9) && scene.document.convertEdgeToBezier(scene.edges[0]),
            "Curve length fixture failed");
        const Document before = scene.document;
        require(!constraint::fixedLengthError(scene.document, {scene.edges[0], 12}), "Length evaluator accepted a straight-shaped Bezier");
        require(!scene.document.setFixedLength(scene.edges[0], 12), "Document assigned length to a Bezier");
        requireDocument(scene.document, before);
    }

    void parallelEvaluatorAndStorageRejectEitherBezierArgument()
    {
        for (int converted = 1; converted <= 3; ++converted)
        {
            Scene scene;
            require(scene.document.addParallel(scene.edges[4], scene.edges[5]), "Retained pair fixture failed");
            if (converted & 1) require(scene.document.convertEdgeToBezier(scene.edges[0]), "First curve fixture failed");
            if (converted & 2) require(scene.document.convertEdgeToBezier(scene.edges[1]), "Second curve fixture failed");
            const Document before = scene.document;
            for (const bool reverse : {false, true})
            {
                const EdgeId a = scene.edges[reverse ? 1 : 0], b = scene.edges[reverse ? 0 : 1];
                require(!constraint::parallelError(scene.document, {a, b}), "Parallel evaluator accepted a Bezier");
                require(!scene.document.addParallel(a, b), "Document assigned parallelism to a Bezier");
                requireDocument(scene.document, before);
            }
        }
    }

    void restoredSegmentAcceptsNewRelations()
    {
        Scene scene;
        const EdgeId id = scene.edges[0];
        require(scene.document.convertEdgeToBezier(id) && scene.document.convertEdgeToSegment(id), "Restored segment fixture failed");
        require(constraint::fixedLengthError(scene.document, {id, 12}).has_value(), "Restored segment length rejected");
        require(constraint::parallelError(scene.document, {id, scene.edges[2]}).has_value(), "Restored segment parallelism rejected");
        const Document before = scene.document;
        require(scene.document.setFixedLength(id, 12) && scene.document.addParallel(id, scene.edges[2]), "New relations rejected after round trip");
        Document expected = before;
        require(expected.setFixedLength(id, 12) && expected.addParallel(id, scene.edges[2]), "Expected relation fixture failed");
        requireDocument(scene.document, expected);
    }

    void missingIdsRejectBothConversionsWithoutChanges()
    {
        Document empty;
        require(!empty.convertEdgeToBezier(EdgeId{1}) && !empty.convertEdgeToSegment(EdgeId{1}), "Empty document accepted conversion");
        Scene scene;
        scene.constrainAll();
        const Document before = scene.document;
        for (const EdgeId missing : {EdgeId{}, EdgeId{999999}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!scene.document.convertEdgeToBezier(missing) && !scene.document.convertEdgeToSegment(missing), "Unknown edge accepted");
            requireDocument(scene.document, before);
        }
    }

    void missingEndpointsRejectWithoutRemovingRelations()
    {
        for (const bool start : {false, true})
        for (const VertexId missing : {VertexId{}, VertexId{999999}})
        {
            Scene scene;
            scene.constrainAll();
            auto* edge = const_cast<Edge*>(scene.document.findEdge(scene.edges[0]));
            (start ? edge->start : edge->end) = missing;
            const Document before = scene.document;
            require(!scene.document.convertEdgeToBezier(scene.edges[0]), "Missing endpoint accepted");
            requireDocument(scene.document, before);
        }
    }

    void nonFiniteEndpointsRejectWithoutRemovingRelations()
    {
        for (const bool start : {false, true})
        for (const bool x : {false, true})
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            Scene scene;
            scene.constrainAll();
            const auto* edge = scene.document.findEdge(scene.edges[0]);
            auto* vertex = const_cast<Vertex*>(scene.document.findVertex(start ? edge->start : edge->end));
            (x ? vertex->position.x : vertex->position.y) = invalid;
            const Document before = scene.document;
            require(!scene.document.convertEdgeToBezier(scene.edges[0]), "Non-finite endpoint accepted");
            requireDocument(scene.document, before);
        }
    }

    void overflowingControlsRejectWithoutRemovingRelations()
    {
        for (const bool x : {false, true})
        for (const double sign : {-1.0, 1.0})
        {
            Scene scene;
            scene.constrainAll();
            const auto* edge = scene.document.findEdge(scene.edges[0]);
            const double value = sign * std::numeric_limits<double>::max();
            require(scene.document.moveVertex(edge->start, x ? Vec2{value, 0} : Vec2{0, value}) &&
                scene.document.moveVertex(edge->end, x ? Vec2{-value, 0} : Vec2{0, -value}), "Overflow fixture failed");
            const Document before = scene.document;
            require(!scene.document.convertEdgeToBezier(scene.edges[0]), "Non-finite computed controls accepted");
            requireDocument(scene.document, before);
        }
    }

    void zeroLengthSegmentConvertsToConstantBezier()
    {
        Scene scene;
        scene.constrainAll();
        const EdgeId id = scene.edges[0];
        const auto* edge = scene.document.findEdge(id);
        const Vec2 position = scene.document.findVertex(edge->start)->position;
        require(scene.document.moveVertex(edge->end, position), "Zero-length fixture failed");
        const Document before = scene.document;
        require(scene.document.convertEdgeToBezier(id), "Constant Bezier conversion failed");
        const auto& curve = std::get<CubicBezier>(scene.document.findEdge(id)->geometry);
        requirePoint(curve.control1, position);
        requirePoint(curve.control2, position);
        requireDocument(scene.document, before, id, id);
    }

    void conversionsDoNotConsumeEntityIds()
    {
        Scene scene;
        Document expected = scene.document;
        for (const EdgeId id : scene.edges)
            require(scene.document.convertEdgeToBezier(id) && scene.document.convertEdgeToBezier(id) &&
                scene.document.convertEdgeToSegment(id), "ID round trip failed");
        require(!scene.document.convertEdgeToBezier(EdgeId{999999}), "Missing edge fixture accepted");
        const std::array triangle{Vec2{100, 100}, Vec2{110, 100}, Vec2{100, 110}};
        const auto actualId = scene.document.addPolygon(triangle);
        const auto expectedId = expected.addPolygon(triangle);
        require(actualId == expectedId, "Conversion consumed a polygon ID");
        requireDocument(scene.document, expected);
    }
}

namespace cad::tests
{
    std::span<const TestCase> edgeConversionTests()
    {
        static const std::array tests{
            TestCase{"every edge converts with thirds and stable topology", convertsEveryEdgeWithCorrectControlsAndStableTopology},
            TestCase{"remove exactly the converted edge's length and incident pairs", removesOnlyConvertedEdgesLengthsAndAllIncidentPairs},
            TestCase{"unconstrained conversion preserves other curves and relations", unconstrainedConversionPreservesOtherCurvesAndRelations},
            TestCase{"repeated Bezier conversion preserves edited handles", repeatedBezierConversionPreservesEditedHandles},
            TestCase{"conversion back changes only selected geometry", conversionBackChangesOnlySelectedGeometry},
            TestCase{"existing segment conversion preserves its constraints", existingSegmentConversionPreservesItsConstraints},
            TestCase{"round trip does not restore removed relations", roundTripDoesNotRestoreRemovedRelations},
            TestCase{"length evaluation and storage reject a Bezier", lengthEvaluatorAndStorageRejectBezier},
            TestCase{"parallel evaluation and storage reject either Bezier argument", parallelEvaluatorAndStorageRejectEitherBezierArgument},
            TestCase{"restored segment accepts new relations", restoredSegmentAcceptsNewRelations},
            TestCase{"missing IDs reject without changes", missingIdsRejectBothConversionsWithoutChanges},
            TestCase{"missing endpoints reject without relation cleanup", missingEndpointsRejectWithoutRemovingRelations},
            TestCase{"non-finite endpoints reject without relation cleanup", nonFiniteEndpointsRejectWithoutRemovingRelations},
            TestCase{"overflowing controls reject without relation cleanup", overflowingControlsRejectWithoutRemovingRelations},
            TestCase{"zero-length segment converts to constant Bezier", zeroLengthSegmentConvertsToConstantBezier},
            TestCase{"conversions preserve entity ID counters", conversionsDoNotConsumeEntityIds}
        };
        return tests;
    }
}
