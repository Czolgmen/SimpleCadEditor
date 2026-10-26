#pragma once

#include <cad_core/geometry/Vec2.h>
#include "BackBuffer.h"
#include "render/Bresenham.h"
#include <cstdint>
#include <vector>


namespace cad::win32
{
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
