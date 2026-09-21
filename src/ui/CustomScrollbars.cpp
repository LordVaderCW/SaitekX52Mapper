#include "CustomScrollbars.hpp"
#include "BattlefieldTheme.hpp"
#include "BufferedPaint.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace x52 {
namespace {
constexpr wchar_t BarClass[] = L"X52.CustomScrollbar";
constexpr UINT_PTR SubclassId = 0x52B;
constexpr UINT SyncMessage = WM_APP + 0x52;
constexpr UINT_PTR AnimationTimer = 2;
constexpr double AnimationMs = 140.0;
int Bounded(std::int64_t value)
{
    return static_cast<int>(std::clamp(value, static_cast<std::int64_t>(INT_MIN), static_cast<std::int64_t>(INT_MAX)));
}
void Fill(HDC dc, RECT rect, COLORREF color)
{
    SetDCBrushColor(dc, color); FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
}
ScrollGeometry ScrollGeometry::Calculate(const SCROLLINFO& info, int pixels, int minimumThumb)
{
    ScrollGeometry out;
    pixels = std::max(0, pixels);
    out.minimum = info.nMin;
    out.maximum = std::max(out.minimum, static_cast<std::int64_t>(info.nMax) - std::max<std::int64_t>(0, static_cast<std::int64_t>(info.nPage) - 1));
    const auto extent = std::max<std::int64_t>(1, static_cast<std::int64_t>(info.nMax) - info.nMin + 1);
    out.length = out.maximum == out.minimum ? pixels : static_cast<int>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(pixels) * info.nPage / extent, std::min(pixels, std::max(0, minimumThumb)), pixels));
    out.travel = pixels - out.length;
    if (out.maximum > out.minimum)
        out.start = static_cast<int>((std::clamp<std::int64_t>(info.nPos, out.minimum, out.maximum) - out.minimum) * out.travel / (out.maximum - out.minimum));
    return out;
}
int ScrollGeometry::Position(int pixel) const
{
    if (!travel) return Bounded(minimum);
    return Bounded(minimum + (static_cast<std::int64_t>(std::clamp(pixel, 0, travel)) * (maximum - minimum) + travel / 2) / travel);
}
CustomScrollbars::CustomScrollbars(HWND target) : target_(target)
{
    shown_ = (GetWindowLongPtrW(target, GWL_STYLE) & WS_VISIBLE) != 0;
    wchar_t cls[64]{}; GetClassNameW(target, cls, 64);
    kind_ = std::wstring_view(cls) == WC_LISTVIEWW ? Kind::ListView : std::wstring_view(cls) == L"Edit" ? Kind::Edit : Kind::ListBox;
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = BarProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = BarClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw WindowsException("Register custom scrollbar", GetLastError());
    vertical_.owner = horizontal_.owner = this; vertical_.vertical = true;
    for (auto* axis : {&vertical_, &horizontal_}) {
        axis->window = CreateWindowExW(0, BarClass, axis->vertical ? L"Vertical scroll" : L"Horizontal scroll",
            WS_CHILD | WS_TABSTOP, 0, 0, 1, 1, GetParent(target), nullptr, wc.hInstance, axis);
        if (!axis->window) {
            if (vertical_.window) DestroyWindow(vertical_.window);
            throw WindowsException("Create custom scrollbar", GetLastError());
        }
    }
    if (!SetWindowSubclass(target_, TargetProc, SubclassId, reinterpret_cast<DWORD_PTR>(this))) {
        DestroyWindow(vertical_.window); DestroyWindow(horizontal_.window);
        throw WindowsException("Subclass scroll content", GetLastError());
    }
    Sync();
}
CustomScrollbars::~CustomScrollbars()
{
    if (target_) RemoveWindowSubclass(target_, TargetProc, SubclassId);
    for (auto* axis : {&vertical_, &horizontal_}) if (axis->window) DestroyWindow(axis->window);
}
ScrollGeometry CustomScrollbars::Geometry(const Axis& axis)
{
    RECT rect{}; GetClientRect(axis.window, &rect);
    auto geometry = ScrollGeometry::Calculate(axis.info, axis.vertical ? rect.bottom : rect.right,
        MulDiv(24, static_cast<int>(GetDpiForWindow(axis.window)), 96));
    if (axis.animating && geometry.maximum > geometry.minimum)
        geometry.start = static_cast<int>(std::lround((std::clamp(axis.displayed, static_cast<double>(geometry.minimum),
            static_cast<double>(geometry.maximum)) - geometry.minimum) * geometry.travel / (geometry.maximum - geometry.minimum)));
    return geometry;
}
int CustomScrollbars::Coordinate(const Axis& axis, LPARAM point)
{
    return axis.vertical ? GET_Y_LPARAM(point) : GET_X_LPARAM(point);
}
void CustomScrollbars::Sync()
{
    if (!target_ || syncing_) return;
    syncing_ = true;
    RECT bounds{}, client{}; GetWindowRect(target_, &bounds); GetClientRect(target_, &client);
    POINT origin{}; ClientToScreen(target_, &origin);
    RECT clip{origin.x - bounds.left, origin.y - bounds.top, origin.x - bounds.left + client.right, origin.y - bounds.top + client.bottom};
    if (!haveClip_ || !EqualRect(&clip, &clipped_)) {
        const auto region = CreateRectRgnIndirect(&clip);
        if (region) {
            if (SetWindowRgn(target_, region, TRUE)) { clipped_ = clip; haveClip_ = true; }
            else DeleteObject(region); // ownership transfers only on success
        }
    }
    const auto style = GetWindowLongPtrW(target_, GWL_STYLE);
    for (auto* axis : {&vertical_, &horizontal_}) {
        if (!axis->window) continue;
        SCROLLINFO info{sizeof(info), SIF_ALL};
        const bool range = GetScrollInfo(target_, axis->vertical ? SB_VERT : SB_HORZ, &info) != FALSE;
        const bool changed = info.nMin != axis->info.nMin || info.nMax != axis->info.nMax || info.nPage != axis->info.nPage || info.nPos != axis->info.nPos;
        if (info.nMin != axis->info.nMin || info.nMax != axis->info.nMax || info.nPage != axis->info.nPage ||
            (!applying_ && info.nPos != axis->info.nPos)) Cancel(*axis);
        axis->info = info;
        if (!axis->animating) axis->displayed = info.nPos;
        RECT rect = axis->vertical ? RECT{origin.x + client.right, origin.y, bounds.right, origin.y + client.bottom} :
            RECT{origin.x, origin.y + client.bottom, origin.x + client.right, bounds.bottom};
        MapWindowPoints(HWND_DESKTOP, GetParent(target_), reinterpret_cast<POINT*>(&rect), 2);
        const bool visible = range && shown_ && (style & (axis->vertical ? WS_VSCROLL : WS_HSCROLL)) &&
            rect.right > rect.left && rect.bottom > rect.top;
        if (!visible && GetCapture() == axis->window) ReleaseCapture();
        if (!visible) Cancel(*axis);
        if (!EqualRect(&rect, &axis->placement) || visible != ((GetWindowLongPtrW(axis->window, GWL_STYLE) & WS_VISIBLE) != 0)) {
            SetWindowPos(axis->window, HWND_TOP, rect.left, rect.top, std::max(1L, rect.right - rect.left),
                std::max(1L, rect.bottom - rect.top), SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
            axis->placement = rect;
        }
        if (changed) InvalidateRect(axis->window, nullptr, FALSE);
    }
    syncing_ = false;
}
void CustomScrollbars::Scroll(Axis& axis, int position)
{
    if (!target_) return;
    const auto geometry = Geometry(axis);
    position = Bounded(std::clamp<std::int64_t>(position, geometry.minimum, geometry.maximum));
    if (position == axis.info.nPos) return;
    applying_ = true;
    if (kind_ == Kind::ListView) {
        if (axis.vertical) {
            RECT row{}; const auto top = ListView_GetTopIndex(target_);
            if (ListView_GetItemRect(target_, top, &row, LVIR_BOUNDS))
                ListView_Scroll(target_, 0, Bounded(static_cast<std::int64_t>(position - top) * (row.bottom - row.top)));
        } else ListView_Scroll(target_, position - axis.info.nPos, 0);
    } else if (kind_ == Kind::Edit && axis.vertical) {
        const auto first = SendMessageW(target_, EM_GETFIRSTVISIBLELINE, 0, 0);
        SendMessageW(target_, EM_LINESCROLL, 0, position - first);
    } else if (kind_ == Kind::ListBox && axis.vertical) {
        SendMessageW(target_, LB_SETTOPINDEX, static_cast<WPARAM>(position), 0);
    } else {
        // These native horizontal controls use WM_HSCROLL's 16-bit thumb position.
        // For larger edit ranges, EM_LINESCROLL moves in average character widths.
        if (position <= USHRT_MAX) SendMessageW(target_, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, position), 0);
        else if (kind_ == Kind::Edit) {
            const auto dc = GetDC(target_); const auto font = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(target_, WM_GETFONT, 0, 0)));
            TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics); SelectObject(dc, font); ReleaseDC(target_, dc);
            SendMessageW(target_, EM_LINESCROLL, static_cast<WPARAM>((position - axis.info.nPos) / std::max(1L, metrics.tmAveCharWidth)), 0);
        }
    }
    Sync();
    applying_ = false;
    // Native controls may use scroll-copy optimizations; repaint their completed
    // frame through the buffer rather than leaving exposed strips behind.
    InvalidateRect(target_, nullptr, FALSE);
}
void CustomScrollbars::Cancel(Axis& axis)
{
    axis.animating = false;
    axis.displayed = axis.info.nPos;
    if (axis.window) KillTimer(axis.window, AnimationTimer);
}
void CustomScrollbars::Animate(Axis& axis, double position)
{
    const auto geometry = Geometry(axis);
    position = std::clamp(position, static_cast<double>(geometry.minimum), static_cast<double>(geometry.maximum));
    BOOL enabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    if (!enabled) { Cancel(axis); Scroll(axis, static_cast<int>(std::lround(position))); return; }
    axis.from = axis.animating ? axis.displayed : axis.info.nPos;
    axis.destination = position;
    axis.started = GetTickCount64();
    axis.animating = std::abs(axis.destination - axis.from) > 0.001;
    if (!axis.animating) { Cancel(axis); return; }
    if (!SetTimer(axis.window, AnimationTimer, 16, nullptr)) {
        Cancel(axis); Scroll(axis, static_cast<int>(std::lround(position)));
    }
}
void CustomScrollbars::Tick(Axis& axis)
{
    if (!axis.animating || !target_) return;
    const double t = std::min(1.0, static_cast<double>(GetTickCount64() - axis.started) / AnimationMs);
    const double remaining = 1.0 - t;
    axis.displayed = axis.from + (axis.destination - axis.from) * (1.0 - remaining * remaining * remaining);
    Scroll(axis, static_cast<int>(std::lround(axis.displayed)));
    if (t >= 1.0) Cancel(axis);
    InvalidateRect(axis.window, nullptr, FALSE);
}
bool CustomScrollbars::Wheel(UINT message, WPARAM wParam)
{
    if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) return false;
    const bool horizontal = message == WM_MOUSEHWHEEL || (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT);
    auto& axis = horizontal ? horizontal_ : vertical_;
    if (!(GetWindowLongPtrW(axis.window, GWL_STYLE) & WS_VISIBLE)) return false;
    UINT amount = 3;
    SystemParametersInfoW(horizontal ? SPI_GETWHEELSCROLLCHARS : SPI_GETWHEELSCROLLLINES, 0, &amount, 0);
    if (!amount) return true;
    axis.wheelRemainder += GET_WHEEL_DELTA_WPARAM(wParam);
    const int notches = axis.wheelRemainder / WHEEL_DELTA;
    axis.wheelRemainder %= WHEEL_DELTA;
    if (!notches) return true;
    double step = amount == WHEEL_PAGESCROLL ? std::max(1u, axis.info.nPage) : amount;
    if (horizontal && amount != WHEEL_PAGESCROLL) step *= MulDiv(16, static_cast<int>(GetDpiForWindow(target_)), 96);
    Animate(axis, (axis.animating ? axis.destination : axis.info.nPos) +
        (message == WM_MOUSEHWHEEL ? notches : -notches) * step);
    return true;
}
void CustomScrollbars::Step(Axis& axis, int direction, bool page)
{
    if (!target_) return;
    const double amount = page ? std::max(1u, axis.info.nPage) : axis.vertical ? 1 : MulDiv(16, static_cast<int>(GetDpiForWindow(target_)), 96);
    Animate(axis, (axis.animating ? axis.destination : axis.info.nPos) + direction * amount);
}
void CustomScrollbars::Repeat(Axis& axis)
{
    POINT cursor{}; GetCursorPos(&cursor); ScreenToClient(axis.window, &cursor);
    RECT rect{}; GetClientRect(axis.window, &rect);
    if (!PtInRect(&rect, cursor)) return;
    const auto geometry = Geometry(axis); const auto coordinate = axis.vertical ? cursor.y : cursor.x;
    if ((axis.pageDirection < 0 && coordinate < geometry.start) || (axis.pageDirection > 0 && coordinate >= geometry.start + geometry.length))
        Step(axis, axis.pageDirection, true);
}
void CustomScrollbars::Paint(Axis& axis, HDC dc)
{
    const auto saved = SaveDC(dc);
    RECT rect{}; GetClientRect(axis.window, &rect); Fill(dc, rect, BattlefieldTheme::Panel);
    const auto geometry = Geometry(axis);
    if (geometry.maximum > geometry.minimum) {
        const auto dip = [&](int value) { return std::max(1, MulDiv(value, static_cast<int>(GetDpiForWindow(axis.window)), 96)); };
        const bool focused = GetFocus() == axis.window;
        const bool active = axis.dragging || axis.hot || focused;
        const auto cross = axis.vertical ? rect.right : rect.bottom;
        const auto center = cross / 2;
        const auto thickness = std::min(cross, static_cast<LONG>(dip(active ? 8 : 6)));
        const auto railWidth = std::min(cross, static_cast<LONG>(dip(2)));
        const auto padding = std::min(dip(3), geometry.length / 4);
        RECT rail = rect;
        if (axis.vertical) {
            rail.left = center - railWidth / 2; rail.right = rail.left + railWidth;
            rail.top += padding; rail.bottom -= padding;
        } else {
            rail.top = center - railWidth / 2; rail.bottom = rail.top + railWidth;
            rail.left += padding; rail.right -= padding;
        }
        Fill(dc, rail, active ? RGB(57, 80, 85) : RGB(35, 54, 59));
        RECT thumb = rect;
        if (axis.vertical) {
            thumb.top = geometry.start + padding; thumb.bottom = geometry.start + geometry.length - padding;
            thumb.left = center - thickness / 2; thumb.right = thumb.left + thickness;
        } else {
            thumb.left = geometry.start + padding; thumb.right = geometry.start + geometry.length - padding;
            thumb.top = center - thickness / 2; thumb.bottom = thumb.top + thickness;
        }
        // Focus is indicated on the thumb, not by a full-height XOR rectangle.
        // The original full-width hit target and scroll geometry are unchanged.
        SetDCBrushColor(dc, axis.dragging ? BattlefieldTheme::Selected : focused ? RGB(191, 226, 228) :
            axis.hot ? RGB(204, 218, 220) : RGB(130, 156, 161));
        SelectObject(dc, GetStockObject(DC_BRUSH)); SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, thickness, thickness);
    }
    RestoreDC(dc, saved);
}
LRESULT CALLBACK CustomScrollbars::BarProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    auto* axis = reinterpret_cast<Axis*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        axis = static_cast<Axis*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        axis->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(axis));
    }
    if (!axis) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_PAINT: { PAINTSTRUCT paint{}; const auto dc = BeginPaint(window, &paint);
        { PaintBuffer buffer(window, dc); Paint(*axis, buffer.Dc()); }
        EndPaint(window, &paint); return 0; }
    case WM_PRINTCLIENT: Paint(*axis, reinterpret_cast<HDC>(wParam)); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        Cancel(*axis);
        const auto geometry = Geometry(*axis); if (geometry.maximum <= geometry.minimum) return 0;
        SetFocus(window); SetCapture(window);
        const auto coordinate = Coordinate(*axis, lParam);
        axis->dragging = coordinate >= geometry.start && coordinate < geometry.start + geometry.length;
        axis->grab = coordinate - geometry.start;
        if (!axis->dragging) {
            axis->pageDirection = coordinate < geometry.start ? -1 : 1;
            axis->owner->Step(*axis, axis->pageDirection, true); SetTimer(window, 1, 350, nullptr);
        }
        InvalidateRect(window, nullptr, FALSE); return 0;
    }
    case WM_MOUSEMOVE:
        if (!axis->hot) { axis->hot = true; TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0}; TrackMouseEvent(&tracking); InvalidateRect(window, nullptr, FALSE); }
        if (axis->dragging && GetCapture() == window) axis->owner->Scroll(*axis, Geometry(*axis).Position(Coordinate(*axis, lParam) - axis->grab));
        return 0;
    case WM_MOUSELEAVE: axis->hot = false; InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: if (GetCapture() == window) ReleaseCapture(); return 0;
    case WM_CANCELMODE: if (GetCapture() == window) ReleaseCapture(); [[fallthrough]];
    case WM_CAPTURECHANGED: axis->dragging = false; axis->pageDirection = 0; KillTimer(window, 1); InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_TIMER:
        if (wParam == 1) { SetTimer(window, 1, 50, nullptr); axis->owner->Repeat(*axis); }
        else if (wParam == AnimationTimer) axis->owner->Tick(*axis);
        return 0;
    case WM_GETDLGCODE: return DLGC_WANTARROWS;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_UP: case VK_LEFT: axis->owner->Step(*axis, -1, false); break;
        case VK_DOWN: case VK_RIGHT: axis->owner->Step(*axis, 1, false); break;
        case VK_PRIOR: axis->owner->Step(*axis, -1, true); break;
        case VK_NEXT: axis->owner->Step(*axis, 1, true); break;
        case VK_HOME: Cancel(*axis); axis->owner->Scroll(*axis, Bounded(Geometry(*axis).minimum)); break;
        case VK_END: Cancel(*axis); axis->owner->Scroll(*axis, Bounded(Geometry(*axis).maximum)); break;
        default: return DefWindowProcW(window, message, wParam, lParam);
        } return 0;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        if (axis->owner->target_) SendMessageW(axis->owner->target_, message, wParam, lParam); return 0;
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_SIZE: InvalidateRect(window, nullptr, FALSE); return 0;
    case WM_NCDESTROY: KillTimer(window, 1); Cancel(*axis); axis->window = nullptr; SetWindowLongPtrW(window, GWLP_USERDATA, 0); break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
LRESULT CALLBACK CustomScrollbars::TargetProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) noexcept
{
    auto* self = reinterpret_cast<CustomScrollbars*>(data);
    if ((message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) && self->Wheel(message, wParam)) return 0;
    if (message == WM_SHOWWINDOW) self->shown_ = wParam != 0;
    if (message == WM_WINDOWPOSCHANGING || message == WM_WINDOWPOSCHANGED) {
        const auto flags = reinterpret_cast<WINDOWPOS*>(lParam)->flags;
        if (flags & SWP_HIDEWINDOW) self->shown_ = false;
        if (flags & SWP_SHOWWINDOW) self->shown_ = true;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, TargetProc, SubclassId); self->target_ = nullptr;
        for (auto* axis : {&self->vertical_, &self->horizontal_}) if (axis->window) DestroyWindow(axis->window);
        return DefSubclassProc(window, message, wParam, lParam);
    }
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_PAINT || message == WM_SIZE || message == WM_WINDOWPOSCHANGED || message == WM_SHOWWINDOW ||
        message == WM_STYLECHANGED || message == WM_SETFONT || message == WM_SETTEXT || message == WM_VSCROLL ||
        message == WM_HSCROLL || message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL || message == WM_KEYDOWN ||
        message == SyncMessage || message == EM_LINESCROLL || message == LB_SETTOPINDEX || message == LVM_SCROLL ||
        message == LVM_INSERTITEMW || message == LVM_DELETEALLITEMS || message == LVM_DELETEITEM ||
        message == LB_ADDSTRING || message == LB_RESETCONTENT || message == LB_SETHORIZONTALEXTENT)
        self->Sync();
    if (message == WM_STYLECHANGED) PostMessageW(window, SyncMessage, 0, 0);
    return result;
}
}
