#include <render/Wu.h>
#include "../TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace
{
    using namespace cad;
    using namespace cad::render;
    using cad::tests::require;
    using Weights = std::map<std::pair<int,int>, double>;

    Weights raster(Vec2 a, Vec2 b, double width = 1)
    {
        std::vector<CoveragePixel> samples;
        require(rasterizeWuLine(a,b,samples,width), "Valid Wu input rejected");
        Weights result;
        for (const auto& sample : samples)
        {
            require(std::isfinite(sample.coverage) && sample.coverage > 0 && sample.coverage <= 1,
                "Coverage outside (0,1]");
            require(result.emplace(std::pair{sample.x,sample.y},sample.coverage).second, "Duplicate pixel in a single segment");
        }
        return result;
    }

    void close(double a, double b) { require(std::abs(a-b) < 1e-10, "Unexpected Wu coverage"); }
    double at(const Weights& weights, int x, int y)
    {
        const auto it = weights.find({x,y});
        return it == weights.end() ? 0.0 : it->second;
    }
    double sum(const Weights& weights)
    {
        double result = 0;
        for (const auto& [point, weight] : weights) result += weight;
        return result;
    }
    void same(const Weights& a, const Weights& b)
    {
        require(a.size() == b.size(), "Different pixel footprints");
        for (const auto& [point,weight] : a)
        {
            const auto found = b.find(point);
            require(found != b.end(), "Missing transformed pixel");
            close(weight,found->second);
        }
    }

    void horizontalHasHalfEndpointsAndSolidInterior()
    {
        const auto pixels = raster({0,3},{4,3});
        require(pixels.size() == 5, "Wrong horizontal footprint");
        for (int x = 0; x <= 4; ++x) close(at(pixels,x,3), x == 0 || x == 4 ? 0.5 : 1.0);
    }
    void verticalHasHalfEndpointsAndSolidInterior()
    {
        const auto pixels = raster({-2,-2},{-2,2});
        require(pixels.size() == 5, "Wrong vertical footprint");
        for (int y = -2; y <= 2; ++y) close(at(pixels,-2,y), y == -2 || y == 2 ? 0.5 : 1.0);
    }
    void knownSlopeSplitsCoverageBetweenNeighbours()
    {
        const auto pixels = raster({0,0},{4,2});
        require(pixels.size() == 7, "Wrong shallow-line footprint");
        close(at(pixels,0,0),0.5);
        close(at(pixels,1,0),0.5); close(at(pixels,1,1),0.5);
        close(at(pixels,2,1),1);
        close(at(pixels,3,1),0.5); close(at(pixels,3,2),0.5);
        close(at(pixels,4,2),0.5);
    }
    void allOctantsAndReversedEndpointsAgree()
    {
        const Vec2 a{0.25,-0.125}, b{5.75,2.125};
        const auto base = raster(a,b);
        for (const bool swap : {false,true})
        for (const int sx : {-1,1})
        for (const int sy : {-1,1})
        {
            const auto transform = [&](Vec2 p) { return Vec2{sx*(swap ? p.y : p.x),sy*(swap ? p.x : p.y)}; };
            Weights expected;
            for (const auto& [point,weight] : base)
            {
                const auto p = transform({static_cast<double>(point.first),static_cast<double>(point.second)});
                expected[{static_cast<int>(p.x),static_cast<int>(p.y)}] = weight;
            }
            same(raster(transform(a),transform(b)),expected);
            same(raster(transform(b),transform(a)),expected);
        }
    }
    void fractionalEndpointsAndPositionArePreserved()
    {
        const auto pixels = raster({0.25,0.25},{2.75,0.25});
        require(pixels.size() == 8, "Fractional line was rounded to integers");
        for (int x = 0; x <= 3; ++x)
        {
            const double gap = x == 0 || x == 3 ? 0.25 : 1.0;
            close(at(pixels,x,0),0.75*gap);
            close(at(pixels,x,1),0.25*gap);
        }
        close(sum(pixels),2.5);
    }
    void negativeCoordinatesUseFloorRatherThanTruncation()
    {
        const auto pixels = raster({-0.25,-0.25},{2.25,-0.25});
        for (int x = 0; x <= 2; ++x)
        {
            const double gap = x == 1 ? 1.0 : 0.75;
            close(at(pixels,x,-1),0.25*gap);
            close(at(pixels,x,0),0.75*gap);
        }
        close(sum(pixels),2.5);
    }
    void zeroLengthDotsHaveFiniteBilinearCoverage()
    {
        const auto integer = raster({2,3},{2,3});
        require(integer.size() == 1, "Integer dot did not emit one pixel");
        close(at(integer,2,3),1);
        const auto fractional = raster({0.25,0.75},{0.25,0.75});
        require(fractional.size() == 4, "Fractional dot did not cover four neighbours");
        close(at(fractional,0,0),0.1875); close(at(fractional,1,0),0.0625);
        close(at(fractional,0,1),0.5625); close(at(fractional,1,1),0.1875);
        close(sum(fractional),1);
        const auto round = raster({2,3},{2,3},2);
        close(at(round,2,3),1);
        close(at(round,3,3),0.5);
        close(at(round,3,4),1.5-std::sqrt(2.0));
    }
    void shortSegmentsAndHalfPixelBoundariesDoNotDoubleEndpoints()
    {
        const auto pixels = raster({0.1,0.25},{0.2,0.25});
        require(pixels.size() == 2, "Short segment duplicated endpoints");
        close(at(pixels,0,0),0.075); close(at(pixels,0,1),0.025);
        for (const double start : {-1.5,0.5})
        {
            const auto boundary = raster({start,2.5},{start+1,2.5});
            require(boundary.size() == 2, "Half-pixel endpoint emitted zero-weight samples");
            close(sum(boundary),1);
        }
    }
    void widthsTwoAndFourHaveSmoothOuterEdges()
    {
        for (const int width : {2,4})
        {
            const auto pixels = raster({0,10},{4,10},width);
            require(at(pixels,-width/2,10) > 0 && at(pixels,4+width/2,10) > 0,
                "Wide stroke lost its round caps");
            close(at(pixels,2,10),1);
            close(at(pixels,2,10-width/2),0.5);
            close(at(pixels,2,10+width/2),0.5);
            close(at(pixels,2,9-width/2),0);
        }
    }
    void wideStrokeHasConstantPerpendicularWidthAtEveryAngle()
    {
        constexpr double pi = 3.14159265358979323846;
        for (const double width : {4.0,12.0,32.0})
        for (const double degrees : {0.0,15.0,30.0,44.9,45.0,45.1,60.0,75.0,90.0,135.0,179.0})
        {
            const double angle = degrees*pi/180.0;
            const auto pixels = raster({20.125,20.25},{20.125+120*std::cos(angle),20.25+120*std::sin(angle)},width);
            // A capsule's area is length*width + pi*radius^2. Pixel-centre AA
            // introduces a small sampling error, but must not lose ~30% at 45 degrees.
            const double area = 120*width + pi*width*width/4;
            require(std::abs(sum(pixels)-area) < area*0.025, "Width depends on line angle");
        }
    }

    void wideCoverageIsContinuousAcrossFortyFiveDegrees()
    {
        constexpr double pi = 3.14159265358979323846;
        const double before = (45.0-0.00001)*pi/180.0, after = (45.0+0.00001)*pi/180.0;
        const auto a = raster({0.25,0.125},{0.25+100*std::cos(before),0.125+100*std::sin(before)},12);
        const auto b = raster({0.25,0.125},{0.25+100*std::cos(after),0.125+100*std::sin(after)},12);
        for (const auto& [p,w] : a) require(std::abs(w-at(b,p.first,p.second)) < 0.001, "Coverage jumps at octant boundary");
        for (const auto& [p,w] : b) require(std::abs(w-at(a,p.first,p.second)) < 0.001, "Coverage jumps at octant boundary");
    }

    void gridMatchesIndependentImplicitLineWeights()
    {
        for (int ax = -2; ax <= 2; ++ax)
        for (int ay = -2; ay <= 2; ++ay)
        for (int bx = -2; bx <= 2; ++bx)
        for (int by = -2; by <= 2; ++by)
        {
            const auto actual = raster({static_cast<double>(ax),static_cast<double>(ay)},
                {static_cast<double>(bx),static_cast<double>(by)});
            Weights expected;
            const int dx = bx-ax, dy = by-ay;
            const bool steep = std::abs(dy) > std::abs(dx);
            const int length = (std::max)(std::abs(dx),std::abs(dy));
            if (length == 0) expected[{ax,ay}] = 1;
            else
            {
                for (int x = -3; x <= 3; ++x)
                for (int y = -3; y <= 3; ++y)
                {
                    const int major = steep ? y : x, a = steep ? ay : ax, b = steep ? by : bx;
                    if (major < (std::min)(a,b) || major > (std::max)(a,b)) continue;
                    const double distance = std::abs(dx*(y-ay)-dy*(x-ax))/static_cast<double>(length);
                    const double gap = major == a || major == b ? 0.5 : 1.0;
                    const double weight = gap*(std::max)(0.0,1-distance);
                    if (weight > 0) expected[{x,y}] = weight;
                }
            }
            same(actual,expected);
        }
    }
    void invalidInputsDoNotAppendAnything()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
        std::vector<CoveragePixel> pixels{{7,8,0.25}};
        for (const double width : {0.0,-1.0,nan,inf,1e100})
            require(!rasterizeWuLine({0,0},{1,1},pixels,width), "Invalid width accepted");
        for (const Vec2 point : {Vec2{nan,0},Vec2{0,inf},Vec2{1e100,0},Vec2{0,-1e100}})
        {
            require(!rasterizeWuLine(point,{0,0},pixels), "Invalid start accepted");
            require(!rasterizeWuLine({0,0},point,pixels), "Invalid end accepted");
        }
        require(pixels.size() == 1 && pixels.front().x == 7 && pixels.front().coverage == 0.25, "Invalid input changed output");
    }
    void appendsAndWorksNearIntegerBounds()
    {
        std::vector<CoveragePixel> pixels{{7,8,0.25}};
        const double high = (std::numeric_limits<int>::max)() - 8.0;
        const double low = (std::numeric_limits<int>::min)() + 8.0;
        require(rasterizeWuLine({high-2,low},{high,low+1},pixels), "Short line near int limits rejected");
        require(pixels.front().x == 7 && pixels.front().y == 8 && pixels.front().coverage == 0.25, "Append lost prefix");
        double weight = 0;
        for (std::size_t i = 1; i < pixels.size(); ++i) weight += pixels[i].coverage;
        close(weight,2);
        const auto base = raster({0.25,0.125},{4.75,2.375});
        Weights translated;
        for (const auto& [p,w] : base) translated[{p.first+10,p.second-20}] = w;
        same(raster({10.25,-19.875},{14.75,-17.625}),translated);
    }
}

namespace cad::tests
{
    std::span<const TestCase> wuTests()
    {
        static const std::array tests{
            TestCase{"horizontal endpoint and interior weights",horizontalHasHalfEndpointsAndSolidInterior},
            TestCase{"vertical endpoint and interior weights",verticalHasHalfEndpointsAndSolidInterior},
            TestCase{"known slope splits neighbour coverage",knownSlopeSplitsCoverageBetweenNeighbours},
            TestCase{"all octants and endpoint reversal",allOctantsAndReversedEndpointsAgree},
            TestCase{"fractional endpoints and subpixel position",fractionalEndpointsAndPositionArePreserved},
            TestCase{"negative coordinates use floor",negativeCoordinatesUseFloorRatherThanTruncation},
            TestCase{"zero length dots have finite coverage",zeroLengthDotsHaveFiniteBilinearCoverage},
            TestCase{"short segments and half pixel boundaries",shortSegmentsAndHalfPixelBoundariesDoNotDoubleEndpoints},
            TestCase{"two and four pixel strokes have AA boundaries",widthsTwoAndFourHaveSmoothOuterEdges},
            TestCase{"wide strokes keep perpendicular width at every angle",wideStrokeHasConstantPerpendicularWidthAtEveryAngle},
            TestCase{"wide coverage is continuous across 45 degrees",wideCoverageIsContinuousAcrossFortyFiveDegrees},
            TestCase{"grid matches implicit line reference",gridMatchesIndependentImplicitLineWeights},
            TestCase{"invalid inputs leave output unchanged",invalidInputsDoNotAppendAnything},
            TestCase{"append prefix translation and int bounds",appendsAndWorksNearIntegerBounds}
        };
        return tests;
    }
}
