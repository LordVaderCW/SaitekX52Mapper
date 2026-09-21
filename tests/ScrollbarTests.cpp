#include "ui/CustomScrollbars.hpp"
#include "ui/BufferedPaint.hpp"
#include <stdexcept>
#include <string>

namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct TestWindow { HWND value{}; ~TestWindow() { if (value) DestroyWindow(value); } };
SCROLLINFO Read(HWND window, bool vertical)
{
    SCROLLINFO info{sizeof(info), SIF_ALL};
    Check(GetScrollInfo(window, vertical ? SB_VERT : SB_HORZ, &info) != FALSE, "native scroll range available");
    return info;
}
void Pump(DWORD milliseconds = 220)
{
    const auto end = GetTickCount64() + milliseconds;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    } while (GetTickCount64() < end);
}
void BufferCheck(HWND content)
{
    RECT rect{}; GetClientRect(content, &rect);
    const auto screen = GetDC(content);
    const auto destination = CreateCompatibleDC(screen);
    const auto bitmap = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
    const auto previous = SelectObject(destination, bitmap);
    ReleaseDC(content, screen);
    const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    bool deferred = true;
    for (int i = 0; i < 50; ++i) {
        SetPixel(destination, 2, 2, RGB(0, 0, 0));
        {
            x52::PaintBuffer buffer(content, destination);
            SetDCBrushColor(buffer.Dc(), RGB(21, 42, 63));
            FillRect(buffer.Dc(), &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            deferred = deferred && GetPixel(destination, 2, 2) == RGB(0, 0, 0);
        }
    }
    const bool presented = GetPixel(destination, 2, 2) == RGB(21, 42, 63);
    const auto after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    SelectObject(destination, previous); DeleteObject(bitmap); DeleteDC(destination);
    Check(deferred && presented, "buffer presents complete frames only after drawing finishes");
    Check(before == after, "repeated buffered frames release their GDI objects");
}
void Exercise(HWND root, HWND content, bool vertical)
{
    const auto bar = FindWindowExW(root, nullptr, L"X52.CustomScrollbar", vertical ? L"Vertical scroll" : L"Horizontal scroll");
    Check(bar != nullptr, "custom scrollbar is an independent window");
    const auto point = [&](int coordinate) { return vertical ? MAKELPARAM(7, coordinate) : MAKELPARAM(coordinate, 7); };
    RECT bounds{}; GetClientRect(bar, &bounds);
    const auto pixels = vertical ? bounds.bottom : bounds.right;
    Check(pixels > 30, "scrollbar has usable extent");
    SendMessageW(bar, WM_KEYDOWN, VK_END, 0);
    const auto end = Read(content, vertical);
    const auto maximum = x52::ScrollGeometry::Calculate(end, pixels, 24).maximum;
    Check(end.nPos > 0 && std::abs(end.nPos - maximum) <= 1, "End reaches last content page");
    SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
    Check(Read(content, vertical).nPos == 0, "Home reaches start");
    SendMessageW(bar, WM_LBUTTONDOWN, MK_LBUTTON, point(pixels - 2));
    SendMessageW(bar, WM_LBUTTONUP, 0, point(pixels - 2));
    BOOL animation = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animation, 0);
    if (animation) Check(Read(content, vertical).nPos == 0, "animated page input does not jump immediately to destination");
    Pump();
    Check(Read(content, vertical).nPos > 0, "track click pages content");
    SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
    auto geometry = x52::ScrollGeometry::Calculate(Read(content, vertical), pixels, MulDiv(24, static_cast<int>(GetDpiForWindow(bar)), 96));
    SendMessageW(bar, WM_LBUTTONDOWN, MK_LBUTTON, point(geometry.length / 2));
    Check(GetCapture() == bar, "custom thumb captures drag");
    SendMessageW(bar, WM_MOUSEMOVE, MK_LBUTTON, point(pixels + 30));
    SendMessageW(bar, WM_LBUTTONUP, 0, point(pixels + 30));
    Check(GetCapture() != bar && Read(content, vertical).nPos > 0, "drag outside control clamps and releases capture");
    SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
    if (vertical) {
        SendMessageW(bar, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0);
        Pump();
        Check(Read(content, true).nPos > 0, "mouse wheel reaches native content through custom bar");
        const int single = Read(content, true).nPos;
        SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
        SendMessageW(content, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA / 2)), 0);
        Pump();
        Check(Read(content, true).nPos == 0, "partial wheel delta retained without rounding to a full notch");
        SendMessageW(content, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA / 2)), 0);
        Pump();
        Check(Read(content, true).nPos == single, "two partial deltas equal one wheel notch");
        SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
        for (int i = 0; i < 3; ++i)
            SendMessageW(content, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0);
        Pump();
        Check(Read(content, true).nPos == single * 3, "wheel bursts accumulate destinations without losing distance");
        SendMessageW(content, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0);
        SendMessageW(bar, WM_KEYDOWN, VK_HOME, 0);
        Pump();
        Check(Read(content, true).nPos == 0, "Home cancels pending animation without late movement");
        SendMessageW(content, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0);
        ShowWindow(content, SW_HIDE);
        const int hidden = Read(content, true).nPos;
        Pump();
        Check(Read(content, true).nPos == hidden, "hidden pages stop their animation timers");
        ShowWindow(content, SW_SHOWNA);
    }
    ShowWindow(content, SW_HIDE);
    Check(!(GetWindowLongPtrW(bar, GWL_STYLE) & WS_VISIBLE), "page hiding also hides its scrollbar");
    ShowWindow(content, SW_SHOWNA);
    Check((GetWindowLongPtrW(bar, GWL_STYLE) & WS_VISIBLE) != 0, "page restoring restores its scrollbar");
    RECT client{}, window{}; GetClientRect(content, &client); GetWindowRect(content, &window);
    POINT origin{}; ClientToScreen(content, &origin);
    const auto region = CreateRectRgn(0, 0, 0, 0);
    Check(GetWindowRgn(content, region) != ERROR, "native control has a clipping region");
    Check(!PtInRegion(region, vertical ? origin.x - window.left + client.right : 2,
        vertical ? 2 : origin.y - window.top + client.bottom), "native non-client scrollbar pixels excluded");
    DeleteObject(region);
}
}
void ScrollbarTests()
{
    using x52::ScrollGeometry;
    SCROLLINFO range{sizeof(range), SIF_ALL, 0, 999999, 100, 999900};
    const auto large = ScrollGeometry::Calculate(range, 300, 24);
    Check(large.start == large.travel && large.Position(large.travel) == 999900 && large.Position(-100) == 0,
        "thumb mapping retains full 32-bit range and clamps endpoints");
    range.nPage = 1000000;
    Check(ScrollGeometry::Calculate(range, 12, 24).travel == 0, "empty/small track has no negative thumb travel");
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&common);
    const auto module = GetModuleHandleW(nullptr);
    for (const auto kind : {0, 1, 2}) {
        TestWindow root{CreateWindowExW(0, L"STATIC", L"Scrollbar test", WS_OVERLAPPEDWINDOW, 0, 0, 500, 400, nullptr, nullptr, module, nullptr)};
        Check(root.value != nullptr, "test host created");
        const auto cls = kind == 0 ? L"EDIT" : kind == 1 ? WC_LISTVIEWW : L"LISTBOX";
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            (kind == 0 ? ES_MULTILINE | ES_AUTOHSCROLL | ES_READONLY : kind == 1 ? LVS_REPORT : LBS_NOINTEGRALHEIGHT);
        const auto content = CreateWindowExW(0, cls, L"", style, 10, 10, 320, 180, root.value, nullptr, module, nullptr);
        Check(content != nullptr, "native test content created");
        SendMessageW(content, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        if (kind == 0) {
            std::wstring text;
            for (int i = 0; i < 400; ++i) text += std::to_wstring(i) + std::wstring(150, L'X') + L"\r\n";
            SetWindowTextW(content, text.c_str());
        } else if (kind == 1) {
            LVCOLUMNW column{}; column.mask = LVCF_WIDTH; column.cx = 1600; ListView_InsertColumn(content, 0, &column);
            for (int i = 0; i < 100; ++i) {
                LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = i; item.pszText = const_cast<wchar_t*>(L"scroll row"); ListView_InsertItem(content, &item);
            }
        } else {
            for (int i = 0; i < 100; ++i) SendMessageW(content, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"scroll row"));
            SendMessageW(content, LB_SETHORIZONTALEXTENT, 1600, 0);
        }
        x52::CustomScrollbars bars(content);
        BufferCheck(content);
        Exercise(root.value, content, true);
        Exercise(root.value, content, false);
        MoveWindow(content, 25, 30, 280, 140, FALSE); bars.Sync();
        const auto vertical = FindWindowExW(root.value, nullptr, L"X52.CustomScrollbar", L"Vertical scroll");
        RECT bar{}, client{}; GetWindowRect(vertical, &bar); GetClientRect(content, &client);
        POINT origin{}; ClientToScreen(content, &origin);
        Check(bar.left == origin.x + client.right && bar.top == origin.y, "custom scrollbar tracks resizing and repositioning");
        DestroyWindow(content); // target-first destruction must release both sibling scrollbar windows
        Check(!IsWindow(vertical), "scrollbars destroyed with their content");
    }
}
