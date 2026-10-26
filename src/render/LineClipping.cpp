#include "LineClipping.h"
#include <cmath>

namespace 
{
    enum OutCode : unsigned
    {
        Inside = 0,
        Left = 1,
        Right = 2,
        Top = 4,
        Bottom = 8
    };
} // namespace 


namespace cad::render
{
    std::optional<ClippedSegment> clipLineToViewport(Vec2 start, Vec2 end, int width, int height)
    {
        if (width <= 0 || height <= 0 ||
            !std::isfinite(start.x) || !std::isfinite(start.y) ||
            !std::isfinite(end.x) || !std::isfinite(end.y))
        {
            return std::nullopt;
        }

        const double xmin = 0;
        const double ymin = 0;
        const double xmax = width -1;
        const double ymax = height - 1;

        auto determinePosition = [&](Vec2 point)
        {
            unsigned code = Inside;
            if (point.x < xmin) code |= Left;
            if (point.x > xmax) code |= Right;
            if (point.y < ymin) code |= Top;
            if (point.y > ymax) code |= Bottom;

            return code;
        };

        unsigned startCode = determinePosition(start);
        unsigned endCode = determinePosition(end);

        while (true)
        {
            if ((startCode | endCode) == 0)
                return ClippedSegment{start, end};

            if ((startCode & endCode) != 0)
                return std::nullopt;

            const bool replaceStart = startCode != Inside;
            const unsigned outsideCode = replaceStart ? startCode : endCode;

            Vec2 intersection{};

            if (outsideCode & Top)
            {
                const double t = (ymin - start.y) / (end.y - start.y);
                intersection = {
                    start.x + t * (end.x - start.x),
                    ymin
                };
            }
            else if (outsideCode & Bottom)
            {
                const double t = (ymax - start.y) / (end.y - start.y);
                intersection = {
                    start.x + t * (end.x - start.x),
                    ymax
                };
            }
            else if (outsideCode & Right)
            {
                const double t = (xmax - start.x) / (end.x - start.x);
                intersection = {
                    xmax,
                    start.y + t * (end.y - start.y)
                };
            }
            else // Left
            {
                const double t = (xmin - start.x) / (end.x - start.x);
                intersection = {
                    xmin,
                    start.y + t * (end.y - start.y)
                };
            }

            if (!std::isfinite(intersection.x) || !std::isfinite(intersection.y))
                return std::nullopt;

            if (replaceStart)
            {
                start = intersection;
                startCode = determinePosition(start);
            }
            else
            {
                end = intersection;
                endCode = determinePosition(end);
            }
        }


    }

} // namespace cad::render
