#pragma once
#include "../util/Windows.hpp"
#include <commctrl.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <optional>
#include "CustomScrollbars.hpp"
#include <memory>
#include <vector>

namespace x52 {
class BattlefieldTheme final {
public:
    static constexpr COLORREF Ink = RGB(235, 241, 241), Muted = RGB(162, 183, 187);
    static constexpr COLORREF Panel = RGB(22, 38, 43), Line = RGB(67, 87, 92), Selected = RGB(240, 244, 243);
    BattlefieldTheme() = default;
    ~BattlefieldTheme();
    BattlefieldTheme(const BattlefieldTheme&) = delete;
    BattlefieldTheme& operator=(const BattlefieldTheme&) = delete;
    void Attach(HWND window);
    void Resize();
    void Paint();
    void Background(HDC dc, HWND child);
    void Style(HWND child);
    LRESULT Color(HDC dc, HWND child, UINT message);
    bool Draw(const DRAWITEMSTRUCT& item);
    void Command(WPARAM wParam, LPARAM lParam);
    std::optional<LRESULT> Notify(NMHDR* header);
private:
    static LRESULT CALLBACK ControlProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR) noexcept;
    void PaintControl(HWND child, HDC dc);
    void GdiBackground(HDC dc, const RECT& rect);
    HWND window_{};
    int width_{}, height_{};
    HBRUSH backdrop_{}, panel_{}, line_{};
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> texture_;
    std::vector<std::unique_ptr<CustomScrollbars>> scrollbars_;
};
}
