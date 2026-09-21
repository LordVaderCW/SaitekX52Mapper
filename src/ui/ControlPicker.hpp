#pragma once
#include "../app/InspectorService.hpp"

namespace x52 {
// Native modal dialog; the HID worker continues reading while it is open.
void ShowControlPicker(HWND owner, HINSTANCE instance, InspectorService& service, const std::string& id);
}
