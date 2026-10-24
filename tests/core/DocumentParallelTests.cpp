#include <cad_core/model/Document.h>

#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using namespace cad;
    using constraint::ParallelConstraint;
    using cad::tests::require;

    static_assert(std::is_same_v<decltype(std::declval<const Document&>().getParallelConstraints()),
        std::span<const ParallelConstraint>>);

    struct Fixture
    {
        Document document;
        std::array<EdgeId, 6> edges;

        Fixture()
        {
            const std::array first{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
            const std::array second{Vec2{30, 0}, Vec2{36, 8}, Vec2{30, 10}};
            const auto* a = document.findPolygon(document.addPolygon(first));
            const auto* b = document.findPolygon(document.addPolygon(second));
            for (std::size_t i = 0; i < 3; ++i)
            {
                edges[i] = a->edges[i];
                edges[i + 3] = b->edges[i];
            }
        }
    };

    void requirePairs(const Document& document, std::span<const ParallelConstraint> expected)
    {
        const auto actual = document.getParallelConstraints();
        require(actual.size() == expected.size(), "Wrong parallel relation count");
        for (std::size_t i = 0; i < actual.size(); ++i)
        {
            const auto a = std::minmax(actual[i].first, actual[i].second);
            const auto b = std::minmax(expected[i].first, expected[i].second);
            require(a.first == b.first && a.second == b.second, "Unexpected pair or order of remaining relations");
        }
    }

    void requireGeometryAndLengths(const Document& actual, const Document& before)
    {
        require(actual.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* after = actual.findPolygon(id);
            require(after && after->id == polygon.id && after->vertices == polygon.vertices && after->edges == polygon.edges,
                "Relation operation changed topology");
            for (const VertexId vertexId : polygon.vertices)
            {
                const auto* a = actual.findVertex(vertexId);
                const auto* b = before.findVertex(vertexId);
                require(a && a->id == b->id && a->position.x == b->position.x && a->position.y == b->position.y,
                    "Relation operation moved a vertex");
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const auto* a = actual.findEdge(edgeId);
                const auto* b = before.findEdge(edgeId);
                require(a && a->id == b->id && a->start == b->start && a->end == b->end, "Relation operation changed an edge");
            }
        }
        const auto a = actual.getFixedLengthConstraints();
        const auto b = before.getFixedLengthConstraints();
        require(a.size() == b.size(), "Length relation count changed");
        for (std::size_t i = 0; i < a.size(); ++i)
            require(a[i].edge == b[i].edge && a[i].length == b[i].length, "Length relation data changed");
    }

    void addsWithinAndAcrossPolygonsWithoutSolving()
    {
        Fixture f;
        require(f.document.setFixedLength(f.edges[0], 10), "Fixture length failed");
        const Document before = f.document;
        const std::array pairs{ParallelConstraint{f.edges[0], f.edges[1]}, ParallelConstraint{f.edges[0], f.edges[3]}};
        for (const auto& pair : pairs)
        {
            const auto error = constraint::parallelError(f.document, pair);
            require(error && *error != 0, "Fixture directions must require correction");
            require(f.document.addParallel(pair.first, pair.second), "Valid pair was rejected");
        }
        requirePairs(f.document, pairs);
        requireGeometryAndLengths(f.document, before);

        // A computed residual of zero is a valid optional value, not a validation failure.
        for (const double sign : {-1.0, 1.0})
        {
            Fixture aligned;
            const auto* edge = aligned.document.findEdge(aligned.edges[3]);
            require(aligned.document.moveVertex(edge->end, {30 + sign * 10, 0}), "Aligned fixture move failed");
            const Document original = aligned.document;
            const ParallelConstraint relation{aligned.edges[0], aligned.edges[3]};
            const auto error = constraint::parallelError(aligned.document, relation);
            require(error && *error == 0, "Fixture must already be parallel or antiparallel");
            require(aligned.document.addParallel(relation.first, relation.second), "Already satisfied relation rejected");
            const std::array expected{relation};
            requirePairs(aligned.document, expected);
            requireGeometryAndLengths(aligned.document, original);
        }
    }

    void rejectsDuplicatesInEitherOrder()
    {
        Fixture f;
        require(f.document.addParallel(f.edges[3], f.edges[0]), "Initial pair rejected");
        require(!f.document.addParallel(f.edges[3], f.edges[0]), "Same-order duplicate accepted");
        require(!f.document.addParallel(f.edges[0], f.edges[3]), "Reversed duplicate accepted");
        const std::array expected{ParallelConstraint{f.edges[0], f.edges[3]}};
        requirePairs(f.document, expected);
    }

    void oneEdgeCanParticipateInSeveralPairs()
    {
        Fixture f;
        const std::array pairs{ParallelConstraint{f.edges[0], f.edges[1]}, ParallelConstraint{f.edges[0], f.edges[3]},
            ParallelConstraint{f.edges[0], f.edges[4]}};
        for (const auto& pair : pairs)
            require(f.document.addParallel(pair.first, pair.second), "Another pair sharing an edge was rejected");
        requirePairs(f.document, pairs);
    }

    void rejectsMissingIdsAndSelfRelations()
    {
        Document empty;
        require(!empty.addParallel(EdgeId{1}, EdgeId{2}) && empty.getParallelConstraints().empty(), "Empty document accepted pair");
        Fixture f;
        const std::array expected{ParallelConstraint{f.edges[1], f.edges[4]}};
        require(f.document.addParallel(expected[0].first, expected[0].second), "Fixture pair failed");
        const Document before = f.document;
        for (const EdgeId missing : {EdgeId{}, EdgeId{999999}, EdgeId{std::numeric_limits<std::uint64_t>::max()}})
        {
            require(!f.document.addParallel(missing, f.edges[3]), "Unknown first edge accepted");
            require(!f.document.addParallel(f.edges[0], missing), "Unknown second edge accepted");
        }
        require(!f.document.addParallel(f.edges[0], f.edges[0]), "Self-relation accepted");
        requirePairs(f.document, expected);
        requireGeometryAndLengths(f.document, before);
    }

    void rejectsZeroOrNonFiniteGeometryWithoutChangingStorage()
    {
        for (const bool firstEdge : {false, true})
        {
            Fixture f;
            const std::array expected{ParallelConstraint{f.edges[1], f.edges[4]}};
            require(f.document.addParallel(expected[0].first, expected[0].second), "Fixture pair failed");
            const Document before = f.document;
            const auto* edge = f.document.findEdge(f.edges[firstEdge ? 0 : 3]);
            const Vec2 originalEnd = f.document.findVertex(edge->end)->position;
            require(f.document.moveVertex(edge->end, f.document.findVertex(edge->start)->position), "Fixture move failed");
            require(!f.document.addParallel(f.edges[0], f.edges[3]), "Zero direction accepted");
            require(f.document.moveVertex(edge->end, originalEnd), "Fixture restoration failed");
            requirePairs(f.document, expected);
            requireGeometryAndLengths(f.document, before);
        }
        const double infinity = std::numeric_limits<double>::infinity();
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity})
        {
            for (std::size_t endpoint = 0; endpoint < 4; ++endpoint)
            {
                for (const bool changeX : {true, false})
                {
                    Fixture f;
                    const std::array expected{ParallelConstraint{f.edges[1], f.edges[4]}};
                    require(f.document.addParallel(expected[0].first, expected[0].second), "Fixture pair failed");
                    const Document before = f.document;
                    const auto* edge = f.document.findEdge(f.edges[endpoint < 2 ? 0 : 3]);
                    auto* vertex = const_cast<Vertex*>(f.document.findVertex(endpoint % 2 == 0 ? edge->start : edge->end));
                    const Vertex original = *vertex;
                    (changeX ? vertex->position.x : vertex->position.y) = invalid;
                    require(!f.document.addParallel(f.edges[0], f.edges[3]), "Non-finite coordinate accepted");
                    *vertex = original;
                    requirePairs(f.document, expected);
                    requireGeometryAndLengths(f.document, before);
                }
            }
        }
    }

    void rejectsOverflowingDirections()
    {
        const double maximum = std::numeric_limits<double>::max();
        for (const bool firstEdge : {false, true})
        {
            for (const bool overflowDifference : {false, true})
            {
                Fixture f;
                const Document before = f.document;
                const auto* edge = f.document.findEdge(f.edges[firstEdge ? 0 : 3]);
                const Vec2 originalStart = f.document.findVertex(edge->start)->position;
                const Vec2 originalEnd = f.document.findVertex(edge->end)->position;
                require(f.document.moveVertex(edge->start, overflowDifference ? Vec2{maximum, 0} : Vec2{}), "Fixture move failed");
                require(f.document.moveVertex(edge->end, overflowDifference ? Vec2{-maximum, 0} : Vec2{maximum, maximum}),
                    "Fixture move failed");
                require(!f.document.addParallel(f.edges[0], f.edges[3]), "Overflowing direction accepted");
                require(f.document.getParallelConstraints().empty(), "Rejected add left a relation");
                require(f.document.moveVertex(edge->start, originalStart) && f.document.moveVertex(edge->end, originalEnd),
                    "Fixture restoration failed");
                requireGeometryAndLengths(f.document, before);
            }
        }
    }

    void removesFirstMiddleAndLastPairsInEitherOrder()
    {
        for (std::size_t removed = 0; removed < 3; ++removed)
        {
            Fixture f;
            const std::array pairs{ParallelConstraint{f.edges[0], f.edges[1]}, ParallelConstraint{f.edges[0], f.edges[3]},
                ParallelConstraint{f.edges[0], f.edges[4]}};
            for (const auto& pair : pairs)
                require(f.document.addParallel(pair.first, pair.second), "Fixture pair failed");
            require(f.document.setFixedLength(f.edges[0], 10), "Fixture length failed");
            const Document before = f.document;
            const auto pair = pairs[removed];
            require(f.document.removeParallel(removed % 2 == 0 ? pair.first : pair.second,
                removed % 2 == 0 ? pair.second : pair.first), "Pair removal failed");
            std::array<ParallelConstraint, 2> remaining;
            std::size_t index = 0;
            for (std::size_t i = 0; i < pairs.size(); ++i)
                if (i != removed) remaining[index++] = pairs[i];
            requirePairs(f.document, remaining);
            require(!f.document.removeParallel(pair.second, pair.first), "Repeated removal succeeded");
            requirePairs(f.document, remaining);
            requireGeometryAndLengths(f.document, before);
            require(f.document.addParallel(pair.second, pair.first), "Could not re-add removed pair");
            require(f.document.getParallelConstraints().size() == 3, "Re-added pair is missing");
        }
    }

    void absentPairRemovalPreservesOtherPairs()
    {
        Document empty;
        require(!empty.removeParallel(EdgeId{}, EdgeId{1}), "Empty document accepted removal");
        Fixture f;
        const std::array pairs{ParallelConstraint{f.edges[0], f.edges[3]}, ParallelConstraint{f.edges[0], f.edges[4]}};
        for (const auto& pair : pairs)
            require(f.document.addParallel(pair.first, pair.second), "Fixture pair failed");
        const Document before = f.document;
        for (const auto& missing : std::array{ParallelConstraint{f.edges[0], f.edges[5]},
            ParallelConstraint{f.edges[1], f.edges[3]}, ParallelConstraint{f.edges[0], f.edges[0]},
            ParallelConstraint{EdgeId{}, f.edges[3]}, ParallelConstraint{f.edges[0], EdgeId{999999}}})
        {
            require(!f.document.removeParallel(missing.first, missing.second), "Absent pair removal succeeded");
            requirePairs(f.document, pairs);
            requireGeometryAndLengths(f.document, before);
        }
    }

    void removalDoesNotRequireValidGeometry()
    {
        for (int mode = 0; mode < 3; ++mode)
        {
            Fixture f;
            require(f.document.addParallel(f.edges[0], f.edges[3]), "Fixture pair failed");
            const Document before = f.document;
            auto* edge = const_cast<Edge*>(f.document.findEdge(f.edges[0]));
            const Edge originalEdge = *edge;
            auto* vertex = const_cast<Vertex*>(f.document.findVertex(edge->end));
            const Vertex originalVertex = *vertex;
            if (mode == 0) vertex->position = f.document.findVertex(edge->start)->position;
            if (mode == 1) edge->start = VertexId{999999};
            if (mode == 2) vertex->position.x = std::numeric_limits<double>::quiet_NaN();
            require(!constraint::parallelError(f.document, {f.edges[0], f.edges[3]}), "Fixture geometry should be invalid");
            require(f.document.removeParallel(f.edges[3], f.edges[0]), "Invalid geometry prevented removing an existing pair");
            require(f.document.getParallelConstraints().empty(), "Removed pair remains in storage");
            *edge = originalEdge;
            *vertex = originalVertex;
            requireGeometryAndLengths(f.document, before);
        }
    }

    struct TopologyFixture
    {
        Document document;
        std::array<EdgeId, 8> edges;
        std::array<VertexId, 8> vertices;

        TopologyFixture()
        {
            for (std::size_t figure = 0; figure < 2; ++figure)
            {
                const double offset = figure * 100.0;
                const std::array positions{Vec2{offset, 0}, Vec2{offset + 20, 0},
                    Vec2{offset + 20, 20}, Vec2{offset, 20}};
                const auto& polygon = *document.findPolygon(document.addPolygon(positions));
                for (std::size_t i = 0; i < 4; ++i)
                {
                    edges[figure * 4 + i] = polygon.edges[i];
                    vertices[figure * 4 + i] = polygon.vertices[i];
                    require(document.setFixedLength(polygon.edges[i], 20), "Topology length fixture failed");
                }
            }
            // Every edge occurs in many pairs, in both argument positions. Storage does not solve geometry.
            for (std::size_t i = 0; i < edges.size(); ++i)
                for (std::size_t j = i + 1; j < edges.size(); ++j)
                    require((i + j) % 2 == 0 ? document.addParallel(edges[i], edges[j])
                        : document.addParallel(edges[j], edges[i]), "Topology pair fixture failed");
        }
    };

    void requireRelationsAfterRemovingEdges(const Document& actual, const Document& before,
        std::span<const EdgeId> removed)
    {
        const auto affected = [removed](EdgeId edge)
        {
            return std::find(removed.begin(), removed.end(), edge) != removed.end();
        };
        std::vector<ParallelConstraint> expected;
        for (const auto& relation : before.getParallelConstraints())
            if (!affected(relation.first) && !affected(relation.second)) expected.push_back(relation);
        require(!expected.empty(), "Fixture must retain unrelated parallel pairs");
        requirePairs(actual, expected);
        for (const auto& relation : actual.getParallelConstraints())
            require(constraint::parallelError(actual, relation).has_value(), "Cleanup left a dangling or degenerate parallel pair");

        std::vector<constraint::FixedLengthConstraint> expectedLengths;
        for (const auto& relation : before.getFixedLengthConstraints())
            if (!affected(relation.edge)) expectedLengths.push_back(relation);
        const auto lengths = actual.getFixedLengthConstraints();
        require(lengths.size() == expectedLengths.size(), "Topology cleanup changed unrelated lengths");
        for (std::size_t i = 0; i < lengths.size(); ++i)
            require(lengths[i].edge == expectedLengths[i].edge && lengths[i].length == expectedLengths[i].length,
                "Topology cleanup lost or modified a retained length");
        require(actual.validate().empty(), "Topology cleanup left an invalid document");
    }

    void splittingAnyEdgeRemovesAllItsPairsAndNeitherHalfInherits()
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            TopologyFixture f;
            const Document before = f.document;
            const EdgeId edge = f.edges[index];
            const Edge oldEdge = *before.findEdge(edge);
            const auto midpoint = f.document.splitEdge(edge);
            require(midpoint.has_value(), "Constrained edge could not be split");
            require(f.document.findEdge(edge) && f.document.findEdge(edge)->end == *midpoint,
                "Original ID must remain on the first half");
            const std::array removed{edge};
            requireRelationsAfterRemovingEdges(f.document, before, removed);
            require(f.document.getParallelConstraints().size() == 21, "Not all seven pairs of the split edge were removed");
            bool foundSecondHalf = false;
            for (const auto& [id, polygon] : f.document.getPolygons())
            {
                (void)id;
                for (const EdgeId candidate : polygon.edges)
                {
                    const auto* segment = f.document.findEdge(candidate);
                    if (segment->start == *midpoint && segment->end == oldEdge.end)
                    {
                        require(candidate != edge, "Second half reused the original ID");
                        foundSecondHalf = true;
                        for (const auto& pair : f.document.getParallelConstraints())
                            require(pair.first != candidate && pair.second != candidate, "Second half inherited a parallel pair");
                    }
                }
            }
            require(foundSecondHalf, "Second half is missing");
        }
    }

    void deletingAnyVertexRemovesPairsOfBothIncidentEdges()
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            TopologyFixture f;
            const Document before = f.document;
            const std::size_t base = (index / 4) * 4;
            const std::size_t local = index % 4;
            const std::array removed{f.edges[base + (local + 3) % 4], f.edges[index]};
            require(f.document.deleteVertex(f.vertices[index]), "Constrained vertex could not be deleted");
            require(!f.document.findEdge(removed[0]) && !f.document.findEdge(removed[1]), "Incident edges remain after deletion");
            requireRelationsAfterRemovingEdges(f.document, before, removed);
            require(f.document.getParallelConstraints().size() == 15, "Incident pairs were missed or unrelated pairs removed");
        }
    }

    void rejectedSplitPreservesAllParallelPairs()
    {
        TopologyFixture f;
        const Document before = f.document;
        for (const EdgeId missing : {EdgeId{}, EdgeId{999999}})
        {
            require(!f.document.splitEdge(missing), "Missing edge split succeeded");
            requirePairs(f.document, before.getParallelConstraints());
            requireGeometryAndLengths(f.document, before);
        }
        // Fail after finding a real constrained edge, before committing any topology.
        auto* edge = const_cast<Edge*>(f.document.findEdge(f.edges[0]));
        const Edge original = *edge;
        edge->end = VertexId{999999};
        require(!f.document.splitEdge(f.edges[0]), "Broken constrained edge split succeeded");
        requirePairs(f.document, before.getParallelConstraints());
        require(edge->end == VertexId{999999}, "Rejected split modified the broken edge");
        *edge = original;
        requireGeometryAndLengths(f.document, before);
    }

    void rejectedDeletionPreservesAllParallelPairs()
    {
        TopologyFixture f;
        const Document before = f.document;
        for (const VertexId missing : {VertexId{}, VertexId{999999}})
        {
            require(!f.document.deleteVertex(missing), "Missing vertex deletion succeeded");
            requirePairs(f.document, before.getParallelConstraints());
            requireGeometryAndLengths(f.document, before);
        }
        auto* outgoing = const_cast<Edge*>(f.document.findEdge(f.edges[0]));
        const Edge original = *outgoing;
        outgoing->end = VertexId{999999};
        require(!f.document.deleteVertex(f.vertices[0]), "Deletion with a broken incident edge succeeded");
        requirePairs(f.document, before.getParallelConstraints());
        require(outgoing->end == VertexId{999999}, "Rejected deletion modified the broken edge");
        *outgoing = original;
        requireGeometryAndLengths(f.document, before);

        Fixture triangle;
        require(triangle.document.addParallel(triangle.edges[0], triangle.edges[1]) &&
            triangle.document.addParallel(triangle.edges[3], triangle.edges[0]), "Triangle pair fixture failed");
        const Document triangleBefore = triangle.document;
        const auto vertices = triangleBefore.getPolygons().begin()->second.vertices;
        for (const VertexId vertex : vertices)
        {
            require(!triangle.document.deleteVertex(vertex), "Triangle vertex deletion succeeded");
            requirePairs(triangle.document, triangleBefore.getParallelConstraints());
            requireGeometryAndLengths(triangle.document, triangleBefore);
        }
    }

    void documentCopiesOwnIndependentRelationStorage()
    {
        Fixture f;
        const std::array pairs{ParallelConstraint{f.edges[0], f.edges[3]}, ParallelConstraint{f.edges[1], f.edges[4]}};
        for (const auto& pair : pairs)
            require(f.document.addParallel(pair.first, pair.second), "Fixture pair failed");
        Document copy = f.document;
        require(copy.removeParallel(f.edges[3], f.edges[0]) && copy.addParallel(f.edges[2], f.edges[5]), "Copy edit failed");
        requirePairs(f.document, pairs);
        const std::array copyPairs{pairs[1], ParallelConstraint{f.edges[2], f.edges[5]}};
        requirePairs(copy, copyPairs);
        require(f.document.removeParallel(f.edges[1], f.edges[4]), "Original edit failed");
        requirePairs(copy, copyPairs);
        requireGeometryAndLengths(copy, f.document);
    }
}

namespace cad::tests
{
    std::span<const TestCase> documentParallelTests()
    {
        static const std::array tests{
            TestCase{"add within and across polygons without solving", addsWithinAndAcrossPolygonsWithoutSolving},
            TestCase{"duplicate pairs in either order", rejectsDuplicatesInEitherOrder},
            TestCase{"one edge participates in several pairs", oneEdgeCanParticipateInSeveralPairs},
            TestCase{"missing IDs and self-relations", rejectsMissingIdsAndSelfRelations},
            TestCase{"zero and non-finite geometry preserve storage", rejectsZeroOrNonFiniteGeometryWithoutChangingStorage},
            TestCase{"overflowing directions reject", rejectsOverflowingDirections},
            TestCase{"first, middle and last removal in either order", removesFirstMiddleAndLastPairsInEitherOrder},
            TestCase{"absent pair removal preserves other pairs", absentPairRemovalPreservesOtherPairs},
            TestCase{"existing pair can be removed after geometry becomes invalid", removalDoesNotRequireValidGeometry},
            TestCase{"document copies own independent relation storage", documentCopiesOwnIndependentRelationStorage},
            TestCase{"split any edge removes every incident pair without inheritance", splittingAnyEdgeRemovesAllItsPairsAndNeitherHalfInherits},
            TestCase{"delete any vertex removes pairs of both incident edges", deletingAnyVertexRemovesPairsOfBothIncidentEdges},
            TestCase{"rejected split preserves all parallel pairs", rejectedSplitPreservesAllParallelPairs},
            TestCase{"rejected deletion preserves all parallel pairs", rejectedDeletionPreservesAllParallelPairs}
        };
        return tests;
    }
}
