#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <optional>

namespace cad::win32
{
    // Cancel/close or dialog creation failure returns nullopt and changes no model data.
    std::optional<double> askFixedLength(HINSTANCE instance, HWND owner, double currentLength);
    std::optional<int> askStrokeWidth(HINSTANCE instance, HWND owner, int currentWidth);
}
