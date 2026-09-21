#include "ui/BattlefieldTheme.hpp"
#include <stdexcept>

namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Host {
    x52::BattlefieldTheme theme;
    HWND window{};
    int clicks{};
    ~Host() { if (window) DestroyWindow(window); }
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<Host*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Host*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self) {
            if (message == WM_DRAWITEM && self->theme.Draw(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam))) return TRUE;
            if (message == WM_NOTIFY) if (const auto result = self->theme.Notify(reinterpret_cast<NMHDR*>(lParam))) return *result;
            if (message == WM_COMMAND) { self->theme.Command(wParam, lParam); ++self->clicks; return 0; }
            if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN || message == WM_CTLCOLORLISTBOX)
                return self->theme.Color(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam), message);
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
};
struct Surface {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    Surface(HWND window)
    {
        RECT rect{}; GetClientRect(window, &rect);
        const auto screen = GetDC(window);
        dc = CreateCompatibleDC(screen); bitmap = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
        previous = SelectObject(dc, bitmap); ReleaseDC(window, screen);
    }
    ~Surface() { SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc); }
};
void BackdropMatches(Host& host, HWND child)
{
    Surface parent(host.window), control(child);
    host.theme.Background(parent.dc, host.window);
    SendMessageW(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(control.dc), PRF_CLIENT);
    POINT origin{}; MapWindowPoints(child, host.window, &origin, 1);
    Check(GetPixel(control.dc, 2, 2) == GetPixel(parent.dc, origin.x + 2, origin.y + 2),
        "transparent control samples the current parent backdrop at its actual position");
}
}
void ThemeTests()
{
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES}; InitCommonControlsEx(&common);
    const auto module = GetModuleHandleW(nullptr);
    WNDCLASSW cls{}; cls.lpfnWndProc = Host::Proc; cls.hInstance = module; cls.lpszClassName = L"X52.ThemeRegression";
    Check(RegisterClassW(&cls) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "register theme test host");
    Host host;
    host.window = CreateWindowExW(0, cls.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 650, 480, nullptr, nullptr, module, &host);
    Check(host.window != nullptr, "theme test host created");
    host.theme.Attach(host.window);
    const auto add = [&](const wchar_t* type, DWORD style, int id, int y) {
        const auto child = CreateWindowExW(0, type, L"", WS_CHILD | WS_VISIBLE | style, 350, y, 200, 36,
            host.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), module, nullptr);
        Check(child != nullptr, "themed test control created"); host.theme.Style(child); return child;
    };
    const auto check = add(L"BUTTON", BS_AUTOCHECKBOX, 1, 30);
    const auto button = add(L"BUTTON", BS_PUSHBUTTON, 2, 80);
    const auto slider = add(TRACKBAR_CLASSW, TBS_NOTICKS, 3, 130);
    const auto combo = add(L"COMBOBOX", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 4, 180);
    const auto label = add(L"STATIC", 0, 5, 230);
    {
        Surface surface(slider);
        NMCUSTOMDRAW draw{}; draw.hdr.hwndFrom = slider; draw.hdr.code = NM_CUSTOMDRAW;
        draw.dwDrawStage = CDDS_PREPAINT; draw.hdc = surface.dc;
        Check(SendMessageW(host.window, WM_NOTIFY, 3, reinterpret_cast<LPARAM>(&draw)) == CDRF_SKIPDEFAULT,
            "native trackbar custom draw cannot fall through to default styling");
    }
    Check((GetWindowLongPtrW(check, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW &&
        (GetWindowLongPtrW(button, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW,
        "all synchronous button drawing uses owner renderer");
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Clock 1"));
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    SendMessageW(check, BM_SETCHECK, BST_CHECKED, 0);
    Check(SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED, "themed checkbox retains programmatic state");
    for (const auto child : {check, button, slider, combo, label}) ValidateRect(child, nullptr);
    for (int i = 0; i < 200; ++i) {
        SendMessageW(check, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
        SendMessageW(slider, TBM_SETPOS, TRUE, 0);
    }
    for (const auto child : {check, button, slider, combo, label})
        Check(!GetUpdateRect(child, nullptr, FALSE), "unchanged settings do not invalidate controls");
    SendMessageW(check, BM_CLICK, 0, 0);
    Check(host.clicks == 1 && SendMessageW(check, BM_GETCHECK, 0, 0) == BST_UNCHECKED,
        "owner-drawn checkbox toggles exactly once on native activation");
    SendMessageW(check, BM_CLICK, 0, 0);
    Check(host.clicks == 2 && SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED,
        "second activation restores checkbox state");
    EnableWindow(check, FALSE);
    SendMessageW(check, BM_CLICK, 0, 0);
    Check(SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED, "disabled checkbox cannot toggle");
    EnableWindow(check, TRUE);
    for (const auto child : {check, slider, label}) BackdropMatches(host, child);
    for (const auto size : {SIZE{1900, 1000}, SIZE{780, 600}, SIZE{1600, 950}}) {
        SetWindowPos(host.window, nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        host.theme.Resize();
        for (const auto child : {check, slider, label}) BackdropMatches(host, child);
    }
}
