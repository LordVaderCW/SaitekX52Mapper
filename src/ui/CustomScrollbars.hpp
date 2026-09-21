#pragma once
#include "../util/Windows.hpp"
#include <commctrl.h>
#include <cstdint>

namespace x52 {
struct ScrollGeometry {
    int start{}, length{}, travel{};
    std::int64_t minimum{}, maximum{};
    static ScrollGeometry Calculate(const SCROLLINFO& info, int pixels, int minimumThumb);
    int Position(int pixel) const;
};
// Owns two independent scrollbar HWNDs; the target remains the owner of its content/range.
// Its system non-client area is excluded with a window region, never overpainted.
class CustomScrollbars final {
public:
    explicit CustomScrollbars(HWND target);
    ~CustomScrollbars();
    CustomScrollbars(const CustomScrollbars&) = delete;
    CustomScrollbars& operator=(const CustomScrollbars&) = delete;
    void Sync();
private:
    struct Axis {
        CustomScrollbars* owner{}; // non-owning; contained in owner
        HWND window{};
        bool vertical{}, hot{}, dragging{};
        int grab{}, pageDirection{};
        int wheelRemainder{};
        bool animating{};
        double from{}, displayed{}, destination{};
        ULONGLONG started{};
        RECT placement{};
        SCROLLINFO info{sizeof(SCROLLINFO), SIF_ALL};
    };
    static LRESULT CALLBACK BarProc(HWND, UINT, WPARAM, LPARAM) noexcept;
    static LRESULT CALLBACK TargetProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR) noexcept;
    static ScrollGeometry Geometry(const Axis& axis);
    static int Coordinate(const Axis& axis, LPARAM point);
    void Scroll(Axis& axis, int position);
    void Step(Axis& axis, int direction, bool page);
    void Animate(Axis& axis, double position);
    void Tick(Axis& axis);
    static void Cancel(Axis& axis);
    bool Wheel(UINT message, WPARAM wParam);
    void Repeat(Axis& axis);
    static void Paint(Axis& axis, HDC dc);
    HWND target_{};
    Axis vertical_, horizontal_;
    RECT clipped_{};
    bool haveClip_{}, syncing_{}, shown_{}, applying_{};
    enum class Kind { ListView, Edit, ListBox } kind_{};
};
}
