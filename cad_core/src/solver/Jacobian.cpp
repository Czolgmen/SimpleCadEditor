#include "SolverDetail.h"

#include <cad_core/model/Document.h>

#include <cmath>
#include <limits>

namespace cad::solver::detail
{
    std::optional<DenseMatrix> computeJacobian(
        const Document& document,
        std::span<const constraint::FixedLengthConstraint> constraints,
        std::span<const VertexId> variableVertices,
        std::span<const double> baseErrors,
        double geometryScale)
    {
        if (baseErrors.size() != constraints.size() || !std::isfinite(geometryScale) || geometryScale <= 0)
            return std::nullopt;
        for (const double error : baseErrors)
            if (!std::isfinite(error))
                return std::nullopt;

        const double step = std::sqrt(std::numeric_limits<double>::epsilon()) * geometryScale;
        if (!std::isfinite(step) || step <= 0)
            return std::nullopt;

        DenseMatrix jacobian(constraints.size(), std::vector<double>(2 * variableVertices.size(), 0.0));
        for (std::size_t vertexIndex = 0; vertexIndex < variableVertices.size(); ++vertexIndex)
        {
            const VertexId vertexId = variableVertices[vertexIndex];
            const auto* vertex = document.findVertex(vertexId);
            if (!vertex || !std::isfinite(vertex->position.x) || !std::isfinite(vertex->position.y))
                return std::nullopt;

            for (std::size_t axis = 0; axis < 2; ++axis)
            {
                Vec2 samplePosition = vertex->position;
                const double original = axis == 0 ? samplePosition.x : samplePosition.y;
                double shifted = original + step;
                // Addition can round back to the original coordinate at large offsets.
                if (shifted == original)
                    shifted = std::nextafter(original, std::numeric_limits<double>::infinity());
                const double actualStep = shifted - original;
                if (!std::isfinite(shifted) || !std::isfinite(actualStep) || actualStep <= 0)
                    return std::nullopt;

                (axis == 0 ? samplePosition.x : samplePosition.y) = shifted;
                // A fresh probe prevents the X sample from affecting the Y sample.
                Document probe = document;
                if (!probe.moveVertex(vertexId, samplePosition))
                    return std::nullopt;
                const auto sampleErrors = evaluateErrors(probe, constraints);
                if (!sampleErrors)
                    return std::nullopt;

                const std::size_t column = 2 * vertexIndex + axis;
                for (std::size_t row = 0; row < constraints.size(); ++row)
                {
                    const double derivative = ((*sampleErrors)[row] - baseErrors[row]) / actualStep;
                    if (!std::isfinite(derivative))
                        return std::nullopt;
                    jacobian[row][column] = derivative;
                }
            }
        }
        return jacobian;
    }
}
