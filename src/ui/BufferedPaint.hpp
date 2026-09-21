#pragma once
#include "../util/Windows.hpp"

namespace x52 {
// Present a complete client frame in one blit. Failure falls back to direct paint.
// Keep ownership local so resize, device changes and window destruction cannot
// leave a selected bitmap or a stale-sized buffer behind.
class PaintBuffer final {
public:
    PaintBuffer(HWND window, HDC destination) : destination_(destination)
    {
        GetClientRect(window, &rect_);
        if (rect_.right <= 0 || rect_.bottom <= 0) return;
        memory_ = CreateCompatibleDC(destination);
        if (memory_) bitmap_ = CreateCompatibleBitmap(destination, rect_.right, rect_.bottom);
        if (bitmap_) old_ = SelectObject(memory_, bitmap_);
    }
    ~PaintBuffer()
    {
        if (old_) {
            BitBlt(destination_, 0, 0, rect_.right, rect_.bottom, memory_, 0, 0, SRCCOPY);
            SelectObject(memory_, old_);
        }
        if (bitmap_) DeleteObject(bitmap_);
        if (memory_) DeleteDC(memory_);
    }
    PaintBuffer(const PaintBuffer&) = delete;
    PaintBuffer& operator=(const PaintBuffer&) = delete;
    HDC Dc() const { return old_ ? memory_ : destination_; }
private:
    HDC destination_{}, memory_{};
    HBITMAP bitmap_{};
    HGDIOBJ old_{};
    RECT rect_{};
};
}
