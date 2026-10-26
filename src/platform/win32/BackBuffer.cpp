#include <platform/win32/BackBuffer.h>

namespace cad::win32
{
    BackBuffer::~BackBuffer()
    {
        reset();
    }

    bool BackBuffer::resize(HDC referenceDc, int width, int height) noexcept
    {
        if (width <= 0 || height <= 0)
        {
            reset();
            return false;
        }

        if (!referenceDc)
            return false;

        if (dc_ && width == width_ && height == height_)
            return true;

        HDC replacementDc = CreateCompatibleDC(referenceDc);
        if (!replacementDc)
            return false;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        HBITMAP replacementBitmap = CreateDIBSection(
            referenceDc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!replacementBitmap)
        {
            DeleteDC(replacementDc);
            return false;
        }

        HGDIOBJ previousBitmap = SelectObject(replacementDc, replacementBitmap);
        if (!previousBitmap || previousBitmap == HGDI_ERROR)
        {
            DeleteObject(replacementBitmap);
            DeleteDC(replacementDc);
            return false;
        }

        // Commit only after all replacement resources have been created.
        reset();
        dc_ = replacementDc;
        bitmap_ = replacementBitmap;
        previousBitmap_ = previousBitmap;
        pixels_ = static_cast<std::uint32_t*>(pixels);
        width_ = width;
        height_ = height;
        return true;
    }

    void BackBuffer::reset() noexcept
    {
        if (dc_ && previousBitmap_)
            SelectObject(dc_, previousBitmap_);
        if (bitmap_)
            DeleteObject(bitmap_);
        if (dc_)
            DeleteDC(dc_);

        dc_ = nullptr;
        bitmap_ = nullptr;
        pixels_ = nullptr;
        previousBitmap_ = nullptr;
        width_ = 0;
        height_ = 0;
    }

    HDC BackBuffer::dc() const noexcept
    {
        return dc_;
    }

    bool BackBuffer::present(HDC destinationDc) const noexcept
    {
        if (!dc_ || !destinationDc)
            return false;

        return BitBlt(destinationDc, 0, 0, width_, height_, dc_, 0, 0, SRCCOPY) != FALSE;
    }
    std::uint32_t *BackBuffer::pixels() noexcept
    {
        return pixels_;
    }
    int BackBuffer::width() const noexcept
    {
        return width_;
    }
    int BackBuffer::height() const noexcept
    {
        return height_;
    }
}
