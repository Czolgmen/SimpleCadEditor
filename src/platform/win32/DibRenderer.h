#pragma once

#include <cad_core/geometry/Vec2.h>
#include "BackBuffer.h"
#include "render/Bresenham.h"
#include "render/Wu.h"
#include <cstdint>
#include <span>
#include <vector>


namespace cad::win32
{
    struct WuScratch
    {
        std::vector<render::CoveragePixel> samples;
        std::vector<double> coverage;
        std::vector<std::size_t> touched;
    };

    // Accumulates coverage for one complete edge/polyline and blends each touched
    // pixel once against the existing DIB. Requires the same GDI flush as Bresenham.
    void drawWuPolyline(BackBuffer& buffer, std::span<const Vec2> screenPoints,
        std::uint32_t color, WuScratch& scratch, int strokeWidth = 1, const RECT* clipRect = nullptr);

    // Screen coordinates are clipped before rounding. Color is 0x00RRGGBB.
    // An optional clipping rectangle also bounds the square stroke brush.
    // The caller must flush pending GDI drawing to this DIB before CPU writes.
    void drawBresenhamLine(
        BackBuffer& buffer,
        Vec2 screenStart,
        Vec2 screenEnd,
        std::uint32_t color,
        std::vector<cad::render::PixelPoint>& scratch,
        int strokeWidth = 1,
        const RECT* clipRect = nullptr
    );
} // namespace cad::win32
