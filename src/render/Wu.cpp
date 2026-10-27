#include "Wu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace
{
    // Poszerzony obrys jest sumą geometrycznych kapsuł (odcinek + okrągłe końce).
    // Pokrycie zależy od odległości prostopadłej, a nie wybranej osi rasteryzacji.
    // Nie przełączamy kształtu ani sposobu mierzenia grubości przy 45 stopniach.
    void rasterizeRoundStroke(cad::Vec2 start, cad::Vec2 end, double width,
        std::vector<cad::render::CoveragePixel>& pixels)
    {
        const double radius = width * 0.5;
        const double support = radius + 0.5;
        const double dx = end.x - start.x, dy = end.y - start.y;
        const double length = std::hypot(dx, dy);
        const double ux = length == 0 ? 1.0 : dx / length;
        const double uy = length == 0 ? 0.0 : dy / length;
        const bool steep = std::abs(dy) > std::abs(dx);

        // Przechodzimy po osi dominującej tylko dla wydajności. Kandydaci w danej
        // kolumnie obejmują cały promień śladu i oba końce, również poza odcinkiem.
        double a = steep ? start.y : start.x, b = steep ? end.y : end.x;
        double minorA = steep ? start.x : start.y, minorB = steep ? end.x : end.y;
        if (a > b) { std::swap(a,b); std::swap(minorA,minorB); }
        const double gradient = b == a ? 0.0 : (minorB-minorA)/(b-a);
        const auto first = static_cast<int>(std::ceil(a-support));
        const auto last = static_cast<int>(std::floor(b+support));
        const double reach = support * std::hypot(1.0, gradient);
        const double innerSquared = (std::max)(0.0,radius-0.5) * (std::max)(0.0,radius-0.5);
        const double outerSquared = support*support;
        for (int major = first; major <= last; ++major)
        {
            const double minor = minorA + gradient*(std::clamp(static_cast<double>(major),a,b)-a);
            const auto low = static_cast<int>(std::ceil(minor-reach));
            const auto high = static_cast<int>(std::floor(minor+reach));
            for (int secondary = low; secondary <= high; ++secondary)
            {
                const int x = steep ? secondary : major;
                const int y = steep ? major : secondary;
                const double px = x-start.x, py = y-start.y;
                const double t = std::clamp(px*ux+py*uy,0.0,length);
                const double distanceX = px-t*ux, distanceY = py-t*uy;
                const double squared = distanceX*distanceX + distanceY*distanceY;
                if (squared >= outerSquared) continue;
                const double coverage = squared <= innerSquared ? 1.0 : support-std::sqrt(squared);
                pixels.push_back({x,y,coverage});
            }
        }
    }
}

namespace cad::render
{
    bool rasterizeWuLine(Vec2 start, Vec2 end, std::vector<CoveragePixel>& pixels, double strokeWidth)
    {
        if (!std::isfinite(strokeWidth) || strokeWidth <= 0)
            return false;
        const double half = strokeWidth * 0.5;
        // Candidate spans use up to sqrt(2) times the perpendicular radius.
        const double low = static_cast<double>((std::numeric_limits<int>::min)()) + 2.0*half + 3.0;
        const double high = static_cast<double>((std::numeric_limits<int>::max)()) - 2.0*half - 3.0;
        for (const double value : {start.x, start.y, end.x, end.y})
            if (!std::isfinite(value) || value < low || value > high)
                return false;

        if (strokeWidth > 1.0)
        {
            rasterizeRoundStroke(start,end,strokeWidth,pixels);
            return true;
        }

        // Długość zerowa nie ma nachylenia. Rysujemy kwadratowy punkt z pokryciem
        // równym polu przecięcia piksela i kwadratu, także dla pozycji ułamkowej.
        if (start == end)
        {
            const auto x0 = static_cast<int>(std::ceil(start.x - half - 0.5));
            const auto x1 = static_cast<int>(std::floor(start.x + half + 0.5));
            const auto y0 = static_cast<int>(std::ceil(start.y - half - 0.5));
            const auto y1 = static_cast<int>(std::floor(start.y + half + 0.5));
            for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
            {
                const double cx = (std::min)(start.x + half, x + 0.5) - (std::max)(start.x - half, x - 0.5);
                const double cy = (std::min)(start.y + half, y + 0.5) - (std::max)(start.y - half, y - 0.5);
                if (cx > 0 && cy > 0) pixels.push_back({x, y, cx * cy});
            }
            return true;
        }

        // Algorytm Xiaolina Wu: po zamianie osi dla stromych linii przechodzimy
        // zawsze wzdłuż X. Dla szerokości 1 pokrycia dwóch sąsiadów wynoszą
        // 1-frac(y) i frac(y). Szersze obrysy obsłużyła wcześniej ścieżka geometryczna.
        const bool steep = std::abs(end.y - start.y) > std::abs(end.x - start.x);
        if (steep)
        {
            std::swap(start.x, start.y);
            std::swap(end.x, end.y);
        }
        if (start.x > end.x) std::swap(start, end);
        const double gradient = (end.y - start.y) / (end.x - start.x);
        const auto first = static_cast<int>(std::floor(start.x + 0.5));
        const auto last = static_cast<int>(std::floor(end.x + 0.5));
        for (int x = first; x <= last; ++x)
        {
            // Ułamkowe końce: odpowiednik xgap w klasycznym Wu. Dla odcinka
            // krótszego od jednej kolumny liczymy wspólny przedział tylko raz.
            const double gap = (std::min)(end.x, x + 0.5) - (std::max)(start.x, x - 0.5);
            if (gap <= 0) continue;
            const double y = first == last ? (start.y + end.y) * 0.5 : start.y + gradient * (x - start.x);
            const auto bottom = static_cast<int>(std::ceil(y - half - 0.5));
            const auto top = static_cast<int>(std::floor(y + half + 0.5));
            for (int row = bottom; row <= top; ++row)
            {
                const double coverage = gap * ((std::min)(y + half, row + 0.5) - (std::max)(y - half, row - 0.5));
                if (coverage > 0)
                    pixels.push_back({steep ? row : x, steep ? x : row, (std::min)(1.0, coverage)});
            }
        }
        return true;
    }
}
