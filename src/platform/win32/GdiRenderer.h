#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace cad
{
    class Document;
}

namespace cad::win32
{
    void drawDocument(HDC dc, const cad::Document& document, const RECT& clientRect);
}
