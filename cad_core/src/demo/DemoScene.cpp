#include <cad_core/demo/DemoScene.hpp>
#include <cad_core/solver/ConstraintSolver.h>

#include <array>
#include <stdexcept>

namespace
{
    void requireDemoSetup(bool success, const char* message)
    {
        if (!success)
            throw std::logic_error(message);
    }
}

namespace cad
{
    Document createDemoScene()
    {
        Document document;

        // Edges follow the array order, including the closing last -> first.
        // Add another figure with addPolygon and keep its IDs for configuration.
        const std::array leftOutline{
            Vec2{60, 60}, Vec2{240, 60}, Vec2{240, 180},
            Vec2{140, 260}, Vec2{60, 180}};
        const std::array rightOutline{
            Vec2{340, 60}, Vec2{500, 60}, Vec2{500, 180},
            Vec2{420, 260}, Vec2{340, 180}};

        const Polygon left = *document.findPolygon(document.addPolygon(leftOutline));
        const Polygon right = *document.findPolygon(document.addPolygon(rightOutline));

        // Upper-left cubic: C1 with the incoming 120-unit vertical segment,
        // default G1 with the outgoing diagonal. Handles match both tangents.
        requireDemoSetup(document.convertEdgeToBezier(left.edges[2]) &&
            document.moveControlPoint({left.edges[2], BezierControlPoint::First}, {240, 220}) &&
            document.moveControlPoint({left.edges[2], BezierControlPoint::Second}, {180, 300}) &&
            document.setContinuity(left.vertices[2], Continuity::C1),
            "Could not configure the demo Bezier");

        // Right figure: a G1 arc continues the vertical side at its start;
        // the next arc has G0 at both ends and is therefore a semicircle.
        requireDemoSetup(document.setContinuity(right.vertices[2], Continuity::G1) &&
            document.convertEdgeToArc(right.edges[2]) &&
            document.convertEdgeToArc(right.edges[3]),
            "Could not configure the demo arcs");

        // Several relations on one edge, plus two pairs spanning both figures.
        requireDemoSetup(document.setFixedLength(left.edges[0], 180) &&
            document.setFixedLength(left.edges[1], 120) &&
            document.setFixedLength(right.edges[0], 160) &&
            document.addParallel(left.edges[0], right.edges[0]) &&
            document.addParallel(left.edges[1], right.edges[1]),
            "Could not configure the demo edge constraints");

        // Uses the same fitting and constraint checks as interactive editing.
        // The selected coordinates already satisfy length/parallel/Bezier joins;
        // the solver analytically fits the single-G1 arc before returning.
        const auto result = solver::solveConstraints(document,
            document.getFixedLengthConstraints(), document.getParallelConstraints(), left.vertices[0]);
        requireDemoSetup(result.status == solver::SolveStatus::Converged,
            "The predefined demo constraints could not be satisfied");

        if (!document.validate().empty())
            throw std::logic_error("The predefined demo scene is invalid");

        return document;
    }

} // namespace cad
