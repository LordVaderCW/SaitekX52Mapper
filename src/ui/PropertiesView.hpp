#pragma once
#include "BattlefieldTheme.hpp"
#include "../device/DeadzoneSettings.hpp"
#include "../input/State.hpp"

namespace x52 {
class PropertiesView final {
public:
    ~PropertiesView();
    void Attach(HWND window, BattlefieldTheme& theme, bool deadzones);
    void Inputs(const X52State& state, bool connected);
    void Settings(const std::array<AxisDeadzone,9>& axes);
    const std::array<AxisDeadzone,9>& Settings() const { return axes_; }
    static int MoveLimit(AxisDeadzone& axis, int handle, int position);
private:
    static LRESULT CALLBACK Proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR) noexcept;
    void Paint(HDC dc);
    RECT Row(int axis) const;
    void Move(int x);
    HWND window_{};
    BattlefieldTheme* theme_{};
    bool deadzones_{}, connected_{}, dragging_{};
    int selectedAxis_{}, selectedHandle_{1}, grabOffset_{};
    std::array<AxisDeadzone,9> axes_{};
    std::array<int,9> live_{};
    std::array<int,35> buttons_{};
    int hat_{};
};
}
