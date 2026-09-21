#include "BattlefieldTheme.hpp"
#include "BufferedPaint.hpp"
#include <uxtheme.h>
#include <dwmapi.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace x52 {
namespace {
std::wstring Class(HWND window) { wchar_t name[64]{}; GetClassNameW(window, name, 64); return name; }
std::wstring Caption(HWND window)
{
    std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(window)) + 1, L'\0');
    text.resize(static_cast<std::size_t>(GetWindowTextW(window, text.data(), static_cast<int>(text.size())))); return text;
}
void Fill(HDC dc, RECT rect, COLORREF color)
{
    SetDCBrushColor(dc, color); FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
void Edge(HDC dc, RECT rect, COLORREF color)
{
    SetDCBrushColor(dc, color); FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
int Dip(HWND window, int value) { return MulDiv(value, static_cast<int>(GetDpiForWindow(window)), 96); }
}
BattlefieldTheme::~BattlefieldTheme()
{
    for (const auto brush : {backdrop_, panel_, line_}) if (brush) DeleteObject(brush);
}
void BattlefieldTheme::Attach(HWND window)
{
    window_ = window;
    panel_ = CreateSolidBrush(Panel); line_ = CreateSolidBrush(Line);
    if (!panel_ || !line_) throw WindowsException("Create theme brushes", GetLastError());
    const BOOL dark = TRUE;
    (void)DwmSetWindowAttribute(window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    // Default Direct2D target uses hardware acceleration when available; GDI remains a fallback.
    (void)D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf());
    Resize();
}
void BattlefieldTheme::Resize()
{
    if (!window_) return;
    RECT rect{}; GetClientRect(window_, &rect);
    if (rect.right <= 0 || rect.bottom <= 0 || (rect.right == width_ && rect.bottom == height_ && backdrop_)) return;
    width_ = rect.right; height_ = rect.bottom;
    constexpr int w = 320, h = 200;
    std::array<std::uint32_t, w * h> pixels{};
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const double u = static_cast<double>(x) / w, v = static_cast<double>(y) / h;
        const auto glow = [](double dx, double dy, double spread) { return std::exp(-(dx * dx + dy * dy) / spread); };
        const double haze = glow(u - 0.89, v - 0.70, 0.17);
        const double ember = glow(u - 0.15, v - 0.48, 0.012);
        const double cool = glow(u - 0.55, v - 0.2, 0.14);
        const auto r = static_cast<unsigned>(17 + 51 * haze + 49 * ember + 5 * cool);
        const auto g = static_cast<unsigned>(30 + 64 * haze + 9 * ember + 12 * cool);
        const auto b = static_cast<unsigned>(35 + 67 * haze + 5 * ember + 14 * cool);
        pixels[static_cast<std::size_t>(y * w + x)] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
    const auto dc = GetDC(window_);
    const auto memory = CreateCompatibleDC(dc);
    BITMAPINFO full{}; full.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    full.bmiHeader.biWidth = width_; full.bmiHeader.biHeight = -height_;
    full.bmiHeader.biPlanes = 1; full.bmiHeader.biBitCount = 32; full.bmiHeader.biCompression = BI_RGB;
    void* fullPixels{};
    const auto bitmap = CreateDIBSection(dc, &full, DIB_RGB_COLORS, &fullPixels, nullptr, 0);
    if (!dc || !memory || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (dc) ReleaseDC(window_, dc);
        throw WindowsException("Create theme background", GetLastError());
    }
    const auto old = SelectObject(memory, bitmap);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(memory, HALFTONE); SetBrushOrgEx(memory, 0, 0, nullptr);
    StretchDIBits(memory, 0, 0, width_, height_, 0, 0, w, h, pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    GdiFlush(); // Direct2D and child brushes consume the exact same raster.
    const auto replacement = CreatePatternBrush(bitmap);
    if (!replacement) {
        const auto error = GetLastError();
        SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window_, dc);
        throw WindowsException("Create backdrop brush", error);
    }
    if (backdrop_) DeleteObject(backdrop_);
    backdrop_ = replacement;
    if (factory_) {
        if (!target_) {
            (void)factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(window_, D2D1::SizeU(static_cast<UINT>(width_), static_cast<UINT>(height_))), &target_);
        } else if (FAILED(target_->Resize(D2D1::SizeU(static_cast<UINT>(width_), static_cast<UINT>(height_))))) {
            texture_.Reset(); target_.Reset();
        }
        texture_.Reset();
        if (target_) (void)target_->CreateBitmap(D2D1::SizeU(static_cast<UINT>(width_), static_cast<UINT>(height_)), fullPixels, static_cast<UINT>(width_) * 4,
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), &texture_);
    }
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window_, dc);
    RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void BattlefieldTheme::GdiBackground(HDC dc, const RECT& rect)
{
    FillRect(dc, &rect, backdrop_ ? backdrop_ : panel_);
}
void BattlefieldTheme::Paint()
{
    PAINTSTRUCT paint{}; const auto dc = BeginPaint(window_, &paint);
    if (target_ && texture_) {
        target_->BeginDraw();
        const auto size = target_->GetSize();
        target_->DrawBitmap(texture_.Get(), D2D1::RectF(0, 0, size.width, size.height), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        if (FAILED(target_->EndDraw())) { texture_.Reset(); target_.Reset(); PaintBuffer buffer(window_, dc); Background(buffer.Dc(), window_); }
    } else { PaintBuffer buffer(window_, dc); Background(buffer.Dc(), window_); }
    EndPaint(window_, &paint);
}
void BattlefieldTheme::Background(HDC dc, HWND child)
{
    RECT rect{}; GetClientRect(child, &rect);
    POINT origin{}; MapWindowPoints(child, window_, &origin, 1);
    SetBrushOrgEx(dc, -origin.x, -origin.y, nullptr);
    GdiBackground(dc, rect);
}
LRESULT BattlefieldTheme::Color(HDC dc, HWND child, UINT message)
{
    SetTextColor(dc, IsWindowEnabled(child) ? Ink : Muted); SetBkColor(dc, Panel);
    if (message == WM_CTLCOLORSTATIC && Class(child) == L"Static") {
        POINT origin{}; MapWindowPoints(child, window_, &origin, 1);
        SetBrushOrgEx(dc, -origin.x, -origin.y, nullptr); SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(backdrop_ ? backdrop_ : panel_);
    }
    return reinterpret_cast<LRESULT>(panel_);
}
void BattlefieldTheme::Style(HWND child)
{
    const auto cls = Class(child);
    SetWindowTheme(child, L"", L"");
    if (cls == L"Button") {
        const auto style = GetWindowLongPtrW(child, GWL_STYLE);
        if ((style & BS_TYPEMASK) == BS_AUTOCHECKBOX) {
            SetPropW(child, L"X52.Checkbox", reinterpret_cast<HANDLE>(1));
            if (SendMessageW(child, BM_GETCHECK, 0, 0) == BST_CHECKED)
                SetPropW(child, L"X52.Checked", reinterpret_cast<HANDLE>(1));
        }
        // Button state messages can draw synchronously, outside WM_PAINT.
        // Owner drawing routes those paths through our renderer too.
        SetWindowLongPtrW(child, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
    }
    if (cls == WC_LISTVIEWW) {
        ListView_SetExtendedListViewStyleEx(child, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(child, Panel); ListView_SetTextBkColor(child, Panel); ListView_SetTextColor(child, Ink);
        const auto header = ListView_GetHeader(child);
        SetWindowTheme(header, L"", L"");
        SetWindowSubclass(header, ControlProc, 1, reinterpret_cast<DWORD_PTR>(this));
    }
    if (cls == L"Button" || cls == L"ComboBox" || cls == TRACKBAR_CLASSW || cls == WC_LISTVIEWW || cls == L"Edit" || cls == L"ListBox" || cls == L"Static")
        if (!SetWindowSubclass(child, ControlProc, 1, reinterpret_cast<DWORD_PTR>(this)))
            throw WindowsException("Theme control subclass", GetLastError());
    if (cls == WC_LISTVIEWW || cls == L"ListBox" || (cls == L"Edit" && (GetWindowLongPtrW(child, GWL_STYLE) & ES_MULTILINE)))
        scrollbars_.push_back(std::make_unique<CustomScrollbars>(child));
}
void BattlefieldTheme::PaintControl(HWND child, HDC dc)
{
    const auto saved = SaveDC(dc);
    RECT rect{}; GetClientRect(child, &rect);
    const auto cls = Class(child);
    const bool enabled = IsWindowEnabled(child), focus = GetFocus() == child;
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, enabled ? Ink : Muted);
    auto label = Caption(child);
    if (cls == L"Button") {
        const bool check = GetPropW(child, L"X52.Checkbox") != nullptr;
        const bool selected = GetPropW(child, L"X52.Selected") != nullptr;
        const bool hot = GetPropW(child, L"X52.Hot") != nullptr;
        const bool pressed = (SendMessageW(child, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
        if (check) {
            Background(dc, child);
            RECT box{0, (rect.bottom - Dip(child, 15)) / 2, Dip(child, 15), (rect.bottom + Dip(child, 15)) / 2};
            Edge(dc, box, focus || hot ? Ink : Muted);
            if (SendMessageW(child, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                InflateRect(&box, -Dip(child, 3), -Dip(child, 3)); Fill(dc, box, enabled ? Ink : Muted);
            }
            rect.left += Dip(child, 24);
        } else {
            const bool nav = GetPropW(child, L"X52.Navigation") != nullptr;
            if (nav && !selected && !hot && !pressed) Background(dc, child);
            else Fill(dc, rect, selected || (enabled && pressed) ? Selected : hot && enabled ? RGB(51, 70, 76) : Panel);
            if (nav && !focus) { auto rule = rect; rule.top = rule.bottom - 1; Fill(dc, rule, Line); }
            else Edge(dc, rect, focus ? Ink : Line);
            if (selected || (enabled && pressed)) SetTextColor(dc, Panel);
            if (nav) rect.left += Dip(child, 16);
            if (!label.empty()) CharUpperBuffW(label.data(), static_cast<DWORD>(label.size()));
            DrawTextW(dc, label.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (nav ? DT_LEFT : DT_CENTER));
        }
        if (check) DrawTextW(dc, label.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    } else if (cls == L"ComboBox") {
        Fill(dc, rect, Panel); Edge(dc, rect, focus ? Ink : Line);
        rect.left += Dip(child, 10); rect.right -= Dip(child, 30);
        DrawTextW(dc, label.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        RECT arrow{rect.right, 0, rect.right + Dip(child, 29), rect.bottom};
        DrawTextW(dc, L"\x2304", 1, &arrow, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    } else if (cls == TRACKBAR_CLASSW) {
        Background(dc, child);
        RECT track{}, thumb{}; SendMessageW(child, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&track));
        SendMessageW(child, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
        track.top = (thumb.top + thumb.bottom) / 2; track.bottom = track.top + Dip(child, 2);
        Fill(dc, track, Line);
        auto amount = track; amount.right = (thumb.left + thumb.right) / 2; Fill(dc, amount, enabled ? Ink : Muted);
        Fill(dc, thumb, enabled ? Ink : Muted);
        if (focus) { InflateRect(&rect, -1, -1); DrawFocusRect(dc, &rect); }
    } else if (cls == WC_HEADERW) {
        Fill(dc, rect, Panel);
        for (int i = 0; i < Header_GetItemCount(child); ++i) {
            RECT cell{}; Header_GetItemRect(child, i, &cell);
            wchar_t text[128]{}; HDITEMW item{}; item.mask = HDI_TEXT; item.pszText = text; item.cchTextMax = 128;
            Header_GetItem(child, i, &item);
            RECT edge = cell; edge.top = edge.bottom - 1; Fill(dc, edge, Line);
            cell.left += Dip(child, 8); SetTextColor(dc, Muted);
            DrawTextW(dc, text, -1, &cell, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
    }
    RestoreDC(dc, saved);
}
bool BattlefieldTheme::Draw(const DRAWITEMSTRUCT& item)
{
    if (item.CtlType == ODT_BUTTON) {
        PaintBuffer buffer(item.hwndItem, item.hDC);
        PaintControl(item.hwndItem, buffer.Dc());
        return true;
    }
    if (item.CtlType == ODT_STATIC) { Fill(item.hDC, item.rcItem, Line); return true; }
    if (item.CtlType != ODT_COMBOBOX && item.CtlType != ODT_LISTBOX) return false;
    const auto saved = SaveDC(item.hDC);
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    Fill(item.hDC, item.rcItem, selected ? Selected : Panel);
    SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, selected ? Panel : (item.itemState & ODS_DISABLED) ? Muted : Ink);
    SelectObject(item.hDC, reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0)));
    if (item.itemID != static_cast<UINT>(-1)) {
        const bool combo = item.CtlType == ODT_COMBOBOX;
        const auto length = SendMessageW(item.hwndItem, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, item.itemID, 0);
        if (length >= 0 && length < 65536) {
            std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
            SendMessageW(item.hwndItem, combo ? CB_GETLBTEXT : LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
            RECT rect = item.rcItem; rect.left += Dip(item.hwndItem, 10);
            DrawTextW(item.hDC, text.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
    }
    if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &item.rcItem);
    RestoreDC(item.hDC, saved); return true;
}
void BattlefieldTheme::Command(WPARAM wParam, LPARAM lParam)
{
    const auto child = reinterpret_cast<HWND>(lParam);
    if (child && HIWORD(wParam) == BN_CLICKED && IsWindowEnabled(child) && GetPropW(child, L"X52.Checkbox"))
        SendMessageW(child, BM_SETCHECK, SendMessageW(child, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
}
std::optional<LRESULT> BattlefieldTheme::Notify(NMHDR* header)
{
    if (header->code != NM_CUSTOMDRAW) return {};
    const auto cls = Class(header->hwndFrom);
    if (cls == TRACKBAR_CLASSW) {
        auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(header);
        if (draw->dwDrawStage == CDDS_PREPAINT) {
            PaintBuffer buffer(header->hwndFrom, draw->hdc);
            PaintControl(header->hwndFrom, buffer.Dc());
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }
    if (cls != WC_LISTVIEWW) return {};
    auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        const bool selected = (ListView_GetItemState(header->hwndFrom, static_cast<int>(draw->nmcd.dwItemSpec), LVIS_SELECTED) & LVIS_SELECTED) != 0;
        draw->clrText = selected ? Panel : Ink;
        draw->clrTextBk = selected ? Selected : (draw->nmcd.dwItemSpec % 2 ? RGB(26, 43, 48) : Panel);
        draw->nmcd.uItemState &= ~CDIS_SELECTED;
        return CDRF_NEWFONT;
    }
    return CDRF_DODEFAULT;
}
LRESULT CALLBACK BattlefieldTheme::ControlProc(HWND child, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) noexcept
{
    auto* theme = reinterpret_cast<BattlefieldTheme*>(data);
    try {
        const auto cls = Class(child);
        const bool customPaint = cls == L"Button" || cls == L"ComboBox" || cls == TRACKBAR_CLASSW || cls == WC_HEADERW;
        const bool bufferedNative = cls == L"Edit" || cls == L"ListBox" || cls == L"Static";
        if (GetPropW(child, L"X52.Checkbox")) {
            if (message == BM_GETCHECK) return GetPropW(child, L"X52.Checked") ? BST_CHECKED : BST_UNCHECKED;
            if (message == BM_SETCHECK) {
                const bool checked = wParam == BST_CHECKED;
                if (checked != (GetPropW(child, L"X52.Checked") != nullptr)) {
                    if (checked) SetPropW(child, L"X52.Checked", reinterpret_cast<HANDLE>(1));
                    else RemovePropW(child, L"X52.Checked");
                    InvalidateRect(child, nullptr, FALSE);
                }
                return 0;
            }
        }
        if (message == CB_SETCURSEL && SendMessageW(child, CB_GETCURSEL, 0, 0) == static_cast<LRESULT>(wParam)) return wParam;
        if (message == TBM_SETPOS && SendMessageW(child, TBM_GETPOS, 0, 0) == lParam) return 0;
        if (message == WM_NCDESTROY) {
            RemovePropW(child, L"X52.Hot"); RemovePropW(child, L"X52.Selected"); RemovePropW(child, L"X52.Navigation");
            RemovePropW(child, L"X52.Checkbox"); RemovePropW(child, L"X52.Checked");
            RemoveWindowSubclass(child, ControlProc, 1);
        } else if (message == WM_MOUSEMOVE && !GetPropW(child, L"X52.Hot")) {
            SetPropW(child, L"X52.Hot", reinterpret_cast<HANDLE>(1));
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, child, 0}; TrackMouseEvent(&tracking); InvalidateRect(child, nullptr, FALSE);
        } else if (message == WM_MOUSELEAVE) {
            RemovePropW(child, L"X52.Hot"); InvalidateRect(child, nullptr, FALSE);
        } else if ((message == WM_PAINT || message == WM_PRINTCLIENT) && customPaint) {
            PAINTSTRUCT paint{};
            const auto dc = message == WM_PAINT ? BeginPaint(child, &paint) : reinterpret_cast<HDC>(wParam);
            { PaintBuffer buffer(child, dc); theme->PaintControl(child, buffer.Dc()); }
            if (message == WM_PAINT) EndPaint(child, &paint);
            return 0;
        } else if (message == WM_PAINT && bufferedNative) {
            PAINTSTRUCT paint{}; const auto dc = BeginPaint(child, &paint);
            {
                PaintBuffer buffer(child, dc);
                if (cls == L"Static") theme->Background(buffer.Dc(), child);
                else { RECT rect{}; GetClientRect(child, &rect); Fill(buffer.Dc(), rect, Panel); }
                DefSubclassProc(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(buffer.Dc()), PRF_CLIENT);
            }
            EndPaint(child, &paint); return 0;
        } else if (message == WM_ERASEBKGND && (customPaint || bufferedNative)) return 1;
        else if (message == WM_NOTIFY) if (const auto result = theme->Notify(reinterpret_cast<NMHDR*>(lParam))) return *result;
        const auto result = DefSubclassProc(child, message, wParam, lParam);
        if (message == WM_ENABLE || message == WM_SETFOCUS || message == WM_KILLFOCUS || message == BM_SETCHECK ||
            message == BM_SETSTATE || message == WM_SETTEXT || message == CB_SETCURSEL || message == TBM_SETPOS)
            InvalidateRect(child, nullptr, FALSE);
        return result;
    } catch (...) { return DefSubclassProc(child, message, wParam, lParam); }
}
}
