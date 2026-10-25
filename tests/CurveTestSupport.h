#pragma once

#include "TestSupport.h"
#include <cad_core/model/Document.h>
#include <cad_core/solver/ConstraintSolver.h>

#include <array>
#include <cmath>

namespace cad::tests
{
    struct CurveTestScene
    {
        Document document;
        Polygon first;
        Polygon other;

        CurveTestScene()
        {
            const std::array a{Vec2{0, 0}, Vec2{30, 0}, Vec2{30, 30}, Vec2{0, 30}};
            const std::array b{Vec2{100, 100}, Vec2{130, 100}, Vec2{130, 130}, Vec2{100, 130}};
            first = *document.findPolygon(document.addPolygon(a));
            other = *document.findPolygon(document.addPolygon(b));
            // Independent fixture data: moving a handle is itself under test.
            const_cast<Edge*>(document.findEdge(first.edges[0]))->geometry = CubicBezier{{10, -10}, {20, -10}};
            const_cast<Edge*>(document.findEdge(first.edges[2]))->geometry = CubicBezier{{20, 40}, {10, 40}};
            const_cast<Edge*>(document.findEdge(other.edges[0]))->geometry = CubicBezier{{110, 90}, {120, 90}};
            require(document.setFixedLength(first.edges[1], 30) && document.setFixedLength(other.edges[1], 30) &&
                document.addParallel(first.edges[1], other.edges[1]), "Curve editing relation fixture failed");
        }

        ControlPointRef control(BezierControlPoint which = BezierControlPoint::First) const
        {
            return {first.edges[0], which};
        }
    };

    // Two curved polygons coupled by lengths and two cross-polygon parallels.
    // Coordinates approximate the scene reported for slow Debug handle dragging.
    struct LinkedCurveTestScene
    {
        Document document;
        Polygon left;
        Polygon right;

        LinkedCurveTestScene()
        {
            const std::array a{Vec2{142, 214}, Vec2{373, 381}, Vec2{327, 462}, Vec2{123, 441}};
            const std::array b{Vec2{540, 211}, Vec2{748, 362}, Vec2{857, 168}};
            left = *document.findPolygon(document.addPolygon(a));
            right = *document.findPolygon(document.addPolygon(b));
            require(document.convertEdgeToBezier(left.edges[2]) && document.convertEdgeToBezier(right.edges[2]),
                "Linked curve conversion failed");
            require(document.moveControlPoint({left.edges[2], BezierControlPoint::First}, {309, 489}) &&
                document.moveControlPoint({left.edges[2], BezierControlPoint::Second}, {116, 516}) &&
                document.moveControlPoint({right.edges[2], BezierControlPoint::First}, {892, 102}) &&
                document.moveControlPoint({right.edges[2], BezierControlPoint::Second}, {470, 161}),
                "Linked curve control setup failed");
            require(document.setFixedLength(left.edges[1], 93.129) &&
                document.setFixedLength(right.edges[0], 257.155) &&
                document.addParallel(left.edges[1], right.edges[1]) &&
                document.addParallel(left.edges[0], right.edges[0]), "Linked curve relations failed");
            require(solver::solveConstraints(document, document.getFixedLengthConstraints(),
                document.getParallelConstraints(), left.vertices[0]).status == solver::SolveStatus::Converged,
                "Linked curve initial state failed to converge");
        }
    };

    inline Vec2 curveHandlePosition(const Document& document, ControlPointRef control)
    {
        const auto& curve = std::get<CubicBezier>(document.findEdge(control.edge)->geometry);
        return control.point == BezierControlPoint::First ? curve.control1 : curve.control2;
    }

    inline Document withMovedCurveControl(Document document, ControlPointRef control, Vec2 position)
    {
        auto& curve = std::get<CubicBezier>(const_cast<Edge*>(document.findEdge(control.edge))->geometry);
        (control.point == BezierControlPoint::First ? curve.control1 : curve.control2) = position;
        return document;
    }

    inline void requireSameCurvePoint(Vec2 actual, Vec2 expected)
    {
        const auto same = [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); };
        require(same(actual.x, expected.x) && same(actual.y, expected.y), "Unexpected vertex or handle position");
    }

    inline void requireSameCurveDocument(const Document& actual, const Document& before,
        PolygonId translated = {}, Vec2 delta = {})
    {
        require(actual.getPolygons().size() == before.getPolygons().size(), "Polygon count changed");
        for (const auto& [id, polygon] : before.getPolygons())
        {
            const auto* found = actual.findPolygon(id);
            require(found && found->id == polygon.id && found->vertices == polygon.vertices && found->edges == polygon.edges,
                "Curve edit changed topology");
            for (const VertexId vertexId : polygon.vertices)
            {
                const auto* a = actual.findVertex(vertexId);
                const auto* b = before.findVertex(vertexId);
                if (!b) { require(!a, "Missing vertex appeared"); continue; }
                require(a && a->id == b->id, "Vertex ID changed");
                require(a->continuity == b->continuity, "Continuity value or source changed");
                requireSameCurvePoint(a->position, id == translated ? b->position + delta : b->position);
            }
            for (const EdgeId edgeId : polygon.edges)
            {
                const auto* a = actual.findEdge(edgeId);
                const auto* b = before.findEdge(edgeId);
                if (!b) { require(!a, "Missing edge appeared"); continue; }
                require(a && a->id == b->id && a->start == b->start && a->end == b->end &&
                    a->geometry.index() == b->geometry.index(), "Edge ID, endpoints or geometry type changed");
                if (const auto* curve = std::get_if<CubicBezier>(&b->geometry))
                {
                    const auto& result = std::get<CubicBezier>(a->geometry);
                    requireSameCurvePoint(result.control1, id == translated ? curve->control1 + delta : curve->control1);
                    requireSameCurvePoint(result.control2, id == translated ? curve->control2 + delta : curve->control2);
                }
            }
        }
        const auto a = actual.getFixedLengthConstraints(), b = before.getFixedLengthConstraints();
        require(a.size() == b.size(), "Length count changed");
        for (std::size_t i = 0; i < a.size(); ++i)
            require(a[i].edge == b[i].edge && a[i].length == b[i].length, "Length relation changed");
        const auto p = actual.getParallelConstraints(), q = before.getParallelConstraints();
        require(p.size() == q.size(), "Parallel count changed");
        for (std::size_t i = 0; i < p.size(); ++i)
            require(p[i].first == q[i].first && p[i].second == q[i].second, "Parallel relation changed");
    }
}
