#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>

namespace cad::win32
{
    class BackBuffer
    {
    public:
        BackBuffer() = default;
        ~BackBuffer();

        BackBuffer(const BackBuffer&) = delete;
        BackBuffer& operator=(const BackBuffer&) = delete;
        BackBuffer(BackBuffer&&) = delete;
        BackBuffer& operator=(BackBuffer&&) = delete;

        // Reuses an existing buffer of the same size. Nonpositive sizes release it.
        bool resize(HDC referenceDc, int width, int height) noexcept;
        void reset() noexcept;

        // Borrowed drawing DC; ownership stays with BackBuffer.
        HDC dc() const noexcept;
        bool present(HDC destinationDc) const noexcept;

        std::uint32_t* pixels() noexcept;
        int width() const noexcept;
        int height() const noexcept;

    private:
        HDC dc_ = nullptr;
        HBITMAP bitmap_ = nullptr;
        HGDIOBJ previousBitmap_ = nullptr;
        std::uint32_t* pixels_ = nullptr;
        int width_ = 0;
        int height_ = 0;
    };
}
