#include "ControlPicker.hpp"
#include "ControlPhoto.hpp"
#include "../input/PhysicalControls.hpp"
#include "../util/Text.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <memory>
#include <array>
#include <sstream>

namespace x52 {
namespace {
enum : int { Unit = 501, Kind, Physical, Part, NeutralValue, PhotoView, PhotoCanvas, SaveLink, ClearLink, Live, Hint, Result, Current };
const wchar_t* KindName(ControlKind kind)
{
    return kind == ControlKind::Button ? L"HID button" : kind == ControlKind::Hat ? L"HID hat value" : L"HID scalar / axis";
}
struct StreamRelease { void operator()(IStream* stream) const { if (stream) stream->Release(); } };
struct Photo {
    std::unique_ptr<IStream, StreamRelease> stream;
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    Photo(HINSTANCE instance, int resource)
    {
        const auto found = FindResourceW(instance, MAKEINTRESOURCEW(resource), RT_RCDATA);
        if (!found) throw WindowsException("FindResourceW (X52 reference photo)", GetLastError());
        const auto loaded = LoadResource(instance, found);
        const auto size = SizeofResource(instance, found);
        if (!loaded || !size) throw WindowsException("LoadResource (X52 reference photo)", GetLastError());
        const auto* bytes = static_cast<const BYTE*>(LockResource(loaded));
        if (!bytes) throw std::runtime_error("Cannot access embedded X52 reference image");
        stream.reset(SHCreateMemStream(bytes, size));
        if (!stream) throw std::runtime_error("Cannot allocate image stream");
        bitmap = std::make_unique<Gdiplus::Bitmap>(stream.get());
        if (bitmap->GetLastStatus() != Gdiplus::Ok) throw std::runtime_error("Cannot decode X52 reference PNG");
    }
};
std::wstring ReadText(HWND window)
{
    std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(window)) + 1, L'\0');
    const auto count = GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    text.resize(static_cast<std::size_t>(count));
    return text;
}
struct Picker {
    InspectorService& service;
    std::string id;
    HINSTANCE instance{};
    HWND window{};
    Control input;
    Snapshot snapshot;
    std::vector<PhysicalChoice> choices;
    std::array<std::unique_ptr<Photo>, 5> photos;
    ULONG_PTR gdiplus{};
    HFONT font{};
    double scale{1};
    std::int64_t minimum{}, maximum{};
    std::uint64_t pending{};
    std::wstring failure;
    explicit Picker(InspectorService& source, std::string controlId, HINSTANCE module)
        : service(source), id(std::move(controlId)), instance(module), snapshot(service.Read())
    {
        const auto found = snapshot.physical.controls.find(id);
        if (found == snapshot.physical.controls.end()) throw std::runtime_error("Wait until this HID input has been decoded");
        input = found->second; minimum = maximum = input.raw;
        Gdiplus::GdiplusStartupInput startup;
        if (Gdiplus::GdiplusStartup(&gdiplus, &startup, nullptr) != Gdiplus::Ok)
            throw std::runtime_error("Unable to initialize native image rendering");
    }
    ~Picker()
    {
        for (auto& photo : photos) photo.reset();
        if (font) DeleteObject(font);
        if (gdiplus) Gdiplus::GdiplusShutdown(gdiplus);
    }
    HWND Item(int value) const { return GetDlgItem(window, value); }
    int Px(int value) const { return static_cast<int>(std::lround(value * scale)); }
    void Text(int item, const std::wstring& value) const
    {
        if (ReadText(Item(item)) != value && !SetWindowTextW(Item(item), value.c_str())) throw WindowsException("SetWindowTextW", GetLastError());
    }
    HWND Add(const wchar_t* cls, const wchar_t* value, int item, int x, int y, int width, int height, DWORD style = 0)
    {
        const auto child = CreateWindowExW(std::wstring_view(cls) == L"EDIT" ? WS_EX_CLIENTEDGE : 0,
            cls, value, WS_CHILD | WS_VISIBLE | style, Px(x), Px(y), Px(width), Px(height), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(item)), instance, nullptr);
        if (!child) throw WindowsException("CreateWindowExW (control picker)", GetLastError());
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return child;
    }
    void Combo(int item, const std::vector<std::wstring>& values)
    {
        SendMessageW(Item(item), CB_RESETCONTENT, 0, 0);
        for (const auto& value : values)
            if (SendMessageW(Item(item), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str())) == CB_ERRSPACE)
                throw std::runtime_error("Unable to populate control selector");
        SendMessageW(Item(item), CB_SETCURSEL, 0, 0);
    }
    const PhysicalChoice* SelectedChoice() const
    {
        const auto index = SendMessageW(Item(Physical), CB_GETCURSEL, 0, 0);
        return index > 0 && static_cast<std::size_t>(index) <= choices.size() ? &choices[static_cast<std::size_t>(index - 1)] : nullptr;
    }
    const PhysicalControl* Selected() const
    {
        const auto* choice = SelectedChoice();
        return choice ? choice->control : nullptr;
    }
    std::string SelectedPart() const
    {
        if (const auto* choice = SelectedChoice(); choice && !choice->fixedPart.empty()) return choice->fixedPart;
        const auto* selected = Selected();
        const auto index = SendMessageW(Item(Part), CB_GETCURSEL, 0, 0);
        return selected && index > 0 && static_cast<std::size_t>(index) <= selected->parts.size() ? selected->parts[static_cast<std::size_t>(index - 1)] : "";
    }
    void Filter()
    {
        const auto unit = SendMessageW(Item(Unit), CB_GETCURSEL, 0, 0);
        const auto kind = SendMessageW(Item(Kind), CB_GETCURSEL, 0, 0);
        choices = PhysicalChoices(static_cast<InputGroup>(unit), kind > 0 ?
            std::optional{static_cast<PhysicalKind>(kind - 1)} : std::nullopt);
        std::vector<std::wstring> names{L"-- Select the physical control you operated --"};
        for (const auto& choice : choices) {
            names.push_back((choice.control->group == InputGroup::Stick ? L"Stick: " : L"Throttle: ") + Wide(choice.name));
        }
        Combo(Physical, names);
        SelectionChanged();
    }
    void SelectionChanged()
    {
        failure.clear();
        const auto* selected = Selected();
        const auto* choice = SelectedChoice();
        const bool fixed = choice && !choice->fixedPart.empty();
        std::vector<std::wstring> parts{selected && selected->parts.empty() ? L"Whole control" : L"-- Select direction / position --"};
        if (fixed) parts = {Wide(choice->fixedPart)};
        else if (selected) for (const auto& part : selected->parts) parts.push_back(Wide(part));
        Combo(Part, parts);
        EnableWindow(Item(Part), selected && !fixed && !selected->parts.empty());
        if (selected) {
            const auto* annotation = FindControlPhoto(selected->id);
            SendMessageW(Item(PhotoView), CB_SETCURSEL, annotation ? annotation->preferred : selected->group == InputGroup::Stick ? 0 : 2, 0);
        }
        PhotoChanged();
        Validate();
    }
    void PhotoChanged()
    {
        const auto* selected = Selected();
        std::wstring hint = selected ? Wide(selected->location) : L"Choose a control from the original X52 hardware. Nothing is linked until you save.";
        if (selected) {
            const auto* annotation = FindControlPhoto(selected->id);
            const auto view = std::clamp(static_cast<int>(SendMessageW(Item(PhotoView), CB_GETCURSEL, 0, 0)), 0, 4);
            if (!annotation || annotation->spots[static_cast<std::size_t>(view)].radius == 0)
                hint += L"\r\nControl hidden in this view; choose another photo.";
            else if (annotation->areaOnly)
                hint += L"\r\nCircle marks the area; control is on the far side.";
        }
        Text(Hint, hint);
        // Also repaint when the selection is cleared, removing the previous ring.
        InvalidateRect(Item(PhotoCanvas), nullptr, TRUE);
    }
    bool Validate()
    {
        const auto* selected = Selected();
        bool valid = selected && CanLink(*selected, SelectedPart(), input.kind);
        std::wstring message;
        if (selected && !valid) message = input.kind == ControlKind::Button ?
            L"A HID button needs a button, hat direction, or switch position. Select its direction above." :
            L"A HID value needs an axis or whole hat/selector. Direction buttons are separate inputs.";
        if (valid) for (const auto& [otherId, learned] : snapshot.assignments) {
            if (otherId != id && learned.physicalId == selected->id && learned.part == SelectedPart()) {
                valid = false; message = L"Already linked to " + Wide(otherId) + L". Clear that row's link before reassigning it.";
            }
        }
        EnableWindow(Item(SaveLink), valid && !pending);
        EnableWindow(Item(ClearLink), snapshot.assignments.contains(id) && !pending);
        if (!pending) Text(Result, !failure.empty() ? failure : message.empty() ? L"Links are saved as observations, never automatically verified." : message);
        return valid;
    }
    void Init(HWND dialog)
    {
        window = dialog;
        const auto owner = GetParent(window);
        MONITORINFO monitor{sizeof(monitor)};
        if (!GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor)) throw WindowsException("GetMonitorInfoW", GetLastError());
        scale = std::min({static_cast<double>(GetDpiForWindow(owner)) / 96.0,
            (monitor.rcWork.right - monitor.rcWork.left - 48) / 980.0,
            (monitor.rcWork.bottom - monitor.rcWork.top - 60) / 740.0});
        font = CreateFontW(-Px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font) throw WindowsException("CreateFontW", GetLastError());
        SetWindowTextW(window, L"Identify physical X52 control");
        RECT bounds{0, 0, Px(960), Px(680)};
        if (!AdjustWindowRectExForDpi(&bounds, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE, 0, GetDpiForWindow(owner)))
            throw WindowsException("AdjustWindowRectExForDpi", GetLastError());
        const auto width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        if (!SetWindowPos(window, nullptr, monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
            monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2, width, height, SWP_NOZORDER))
            throw WindowsException("SetWindowPos", GetLastError());
        Add(L"STATIC", L"Which physical control produces this HID input?", 0, 24, 18, 900, 30);
        Add(L"STATIC", (Wide(id) + L"   |   " + KindName(input.kind) + L"   |   logical range " + std::to_wstring(input.minimum) + L".." + std::to_wstring(input.maximum)).c_str(), 0, 24, 56, 900, 26);
        Add(L"COMBOBOX", L"", PhotoView, 24, 96, 382, 200, CBS_DROPDOWNLIST | WS_TABSTOP);
        Combo(PhotoView, {L"Joystick - front", L"Joystick - angled", L"Throttle - side and base", L"Complete non-Pro X52", L"Joystick - trigger / pinkie side"});
        Add(L"STATIC", L"Manufacturer X52 photograph", PhotoCanvas, 24, 140, 382, 400, SS_OWNERDRAW);
        Add(L"STATIC", L"Reference photos: Logitech / Saitek X52 (not Pro).\r\nNames describe hardware; they do not assume HID numbers.", 0, 24, 552, 385, 55);
        Add(L"STATIC", L"Hardware unit", 0, 438, 96, 205, 22);
        Add(L"COMBOBOX", L"", Unit, 438, 122, 230, 150, CBS_DROPDOWNLIST | WS_TABSTOP);
        Add(L"STATIC", L"Control type", 0, 686, 96, 235, 22);
        Add(L"COMBOBOX", L"", Kind, 686, 122, 250, 230, CBS_DROPDOWNLIST | WS_TABSTOP);
        Combo(Unit, {L"Both units", L"Joystick", L"Throttle"});
        Combo(Kind, {L"All control types", L"Axis", L"Button", L"Hat", L"Switch", L"Mouse / scroll"});
        Add(L"STATIC", L"Physical control", 0, 438, 172, 480, 22);
        Add(L"COMBOBOX", L"", Physical, 438, 198, 498, 340, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        SendMessageW(Item(Physical), CB_SETMINVISIBLE, 12, 0);
        Add(L"STATIC", L"Direction / switch position", 0, 438, 244, 480, 22);
        Add(L"COMBOBOX", L"", Part, 438, 270, 498, 250, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        Add(L"STATIC", L"", Hint, 438, 316, 498, 62);
        Add(L"STATIC", L"Axis failsafe neutral (-1 to +1)", 0, 438, 391, 340, 24);
        Add(L"EDIT", L"0", NeutralValue, 814, 384, 122, 30, WS_TABSTOP | ES_AUTOHSCROLL);
        SendMessageW(Item(NeutralValue), EM_SETLIMITTEXT, 24, 0);
        EnableWindow(Item(NeutralValue), input.kind == ControlKind::Axis);
        Add(L"STATIC", L"", Live, 438, 435, 498, 62);
        Add(L"STATIC", L"", Current, 438, 507, 498, 50);
        Add(L"STATIC", L"", Result, 438, 561, 498, 56);
        Add(L"BUTTON", L"Clear existing link", ClearLink, 24, 627, 180, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Save link", SaveLink, 664, 627, 130, 32, WS_TABSTOP | BS_DEFPUSHBUTTON);
        Add(L"BUTTON", L"Cancel", IDCANCEL, 806, 627, 130, 32, WS_TABSTOP);
        Filter();
        if (const auto found = snapshot.assignments.find(id); found != snapshot.assignments.end()) {
            Text(NeutralValue, std::to_wstring(found->second.neutral));
            for (std::size_t i = 0; i < choices.size(); ++i) if (choices[i].control->id == found->second.physicalId &&
                (choices[i].fixedPart.empty() || choices[i].fixedPart == found->second.part)) {
                SendMessageW(Item(Physical), CB_SETCURSEL, i + 1, 0); SelectionChanged();
                const auto& parts = choices[i].control->parts;
                const auto part = std::find(parts.begin(), parts.end(), found->second.part);
                if (choices[i].fixedPart.empty() && part != parts.end())
                    SendMessageW(Item(Part), CB_SETCURSEL, static_cast<WPARAM>(part - parts.begin() + 1), 0);
                break;
            }
        }
        if (!SetTimer(window, 1, 100, nullptr)) throw WindowsException("SetTimer (control picker)", GetLastError());
        Poll();
    }
    void Poll()
    {
        snapshot = service.Read();
        const auto found = snapshot.physical.controls.find(id);
        if (found != snapshot.physical.controls.end()) {
            input = found->second;
            minimum = std::min(minimum, input.raw); maximum = std::max(maximum, input.raw);
            std::wstring text = L"Live raw: " + std::to_wstring(input.raw) + L"     Observed here: " + std::to_wstring(minimum) + L".." + std::to_wstring(maximum);
            const auto evidence = snapshot.controlEvidence.find(id);
            if (evidence != snapshot.controlEvidence.end()) text += L"\r\nLast change: " + Wide(evidence->second.value("utc", "")) +
                L"   " + std::to_wstring(evidence->second.at("raw_before").get<std::int64_t>()) + L" -> " + std::to_wstring(evidence->second.at("raw_after").get<std::int64_t>());
            if (!snapshot.connected) text += L"\r\nDevice disconnected; showing last observation.";
            Text(Live, text);
        }
        const auto assigned = snapshot.assignments.find(id);
        Text(Current, assigned == snapshot.assignments.end() ? L"Current link: unassigned. Operate one control and watch its value." :
            L"Current link: " + Wide(assigned->second.name) + L"\r\nEvidence status: " + Wide(assigned->second.status));
        if (pending && snapshot.assignmentRequest == pending) {
            pending = 0;
            EnableWindow(Item(IDCANCEL), TRUE);
            if (snapshot.assignmentError.empty()) { EndDialog(window, IDOK); return; }
            failure = Wide(snapshot.assignmentError); Validate();
        } else if (!pending) Validate();
    }
    void Save(bool clear)
    {
        if (pending || (!clear && !Validate())) return;
        failure.clear();
        Command command{clear ? CommandType::ClearAssignment : CommandType::AssignControl};
        command.id = id;
        command.request = static_cast<std::uint64_t>(Qpc());
        if (!clear) {
            command.learned.physicalId = Selected()->id;
            command.learned.part = SelectedPart();
            if (input.kind == ControlKind::Axis) {
                const auto text = ReadText(Item(NeutralValue)); std::size_t used{};
                command.learned.neutral = std::stod(text, &used);
                if (used != text.size() || !std::isfinite(command.learned.neutral) || command.learned.neutral < -1 || command.learned.neutral > 1)
                    throw std::runtime_error("Axis neutral must be a finite number between -1 and +1");
            }
        }
        service.Send(command); pending = command.request;
        EnableWindow(Item(SaveLink), FALSE); EnableWindow(Item(ClearLink), FALSE);
        EnableWindow(Item(IDCANCEL), FALSE);
        Text(Result, L"Saving link...");
    }
    void Paint(const DRAWITEMSTRUCT& draw)
    {
        const auto index = std::clamp(static_cast<int>(SendMessageW(Item(PhotoView), CB_GETCURSEL, 0, 0)), 0, 4);
        auto& photo = photos[static_cast<std::size_t>(index)];
        if (!photo) photo = std::make_unique<Photo>(instance, 201 + index);
        Gdiplus::Graphics graphics(draw.hDC);
        graphics.Clear(Gdiplus::Color(255, 246, 248, 250));
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        const auto width = static_cast<float>(draw.rcItem.right - draw.rcItem.left);
        const auto height = static_cast<float>(draw.rcItem.bottom - draw.rcItem.top);
        const auto imageWidth = static_cast<float>(photo->bitmap->GetWidth()), imageHeight = static_cast<float>(photo->bitmap->GetHeight());
        // Source viewports trim the manufacturer's empty margins without changing
        // the downloaded assets. The throttle view shows the left-hand device.
        const std::array<Gdiplus::RectF, 5> regions{{
            {0.29f, 0.02f, 0.43f, 0.94f}, {0.22f, 0.02f, 0.56f, 0.94f},
            {0.0f, 0.0f, 0.60f, 1.0f}, {0.12f, 0.01f, 0.77f, 0.96f},
            {0.22f, 0.02f, 0.56f, 0.94f}}};
        const auto& region = regions[static_cast<std::size_t>(index)];
        const auto sourceWidth = imageWidth * region.Width, sourceHeight = imageHeight * region.Height;
        const auto fit = std::min(width / sourceWidth, height / sourceHeight);
        const Gdiplus::RectF destination((width - sourceWidth * fit) / 2, (height - sourceHeight * fit) / 2, sourceWidth * fit, sourceHeight * fit);
        if (graphics.DrawImage(photo->bitmap.get(), destination, imageWidth * region.X, imageHeight * region.Y, sourceWidth, sourceHeight, Gdiplus::UnitPixel) != Gdiplus::Ok)
            throw std::runtime_error("Unable to render reference image");
        const auto* selected = Selected();
        const auto* annotation = selected ? FindControlPhoto(selected->id) : nullptr;
        if (annotation) {
            const auto& spot = annotation->spots[static_cast<std::size_t>(index)];
            if (spot.radius > 0) {
                const auto x = destination.X + (spot.x - imageWidth * region.X) * fit;
                const auto y = destination.Y + (spot.y - imageHeight * region.Y) * fit;
                const auto radius = spot.radius * fit;
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                // Outline only: no brush/fill, so the real control stays visible.
                Gdiplus::Pen outline(Gdiplus::Color(255, 255, 0, 0), 3.0f);
                if (graphics.DrawEllipse(&outline, x - radius, y - radius, radius * 2, radius * 2) != Gdiplus::Ok)
                    throw std::runtime_error("Unable to draw physical-control highlight");
            }
        }
    }
};
INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    auto* picker = reinterpret_cast<Picker*>(GetWindowLongPtrW(window, DWLP_USER));
    try {
        if (message == WM_INITDIALOG) {
            picker = reinterpret_cast<Picker*>(lParam);
            SetWindowLongPtrW(window, DWLP_USER, lParam);
            picker->Init(window);
            return TRUE;
        }
        if (!picker) return FALSE;
        switch (message) {
        case WM_TIMER: picker->Poll(); return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDCANCEL: if (!picker->pending) EndDialog(window, IDCANCEL); return TRUE;
            case IDOK: case SaveLink: if (HIWORD(wParam) == BN_CLICKED) picker->Save(false); return TRUE;
            case ClearLink: if (HIWORD(wParam) == BN_CLICKED) picker->Save(true); return TRUE;
            case Unit: case Kind: if (HIWORD(wParam) == CBN_SELCHANGE) picker->Filter(); return TRUE;
            case Physical: if (HIWORD(wParam) == CBN_SELCHANGE) picker->SelectionChanged(); return TRUE;
            case Part: if (HIWORD(wParam) == CBN_SELCHANGE) picker->Validate(); return TRUE;
            case PhotoView: if (HIWORD(wParam) == CBN_SELCHANGE) picker->PhotoChanged(); return TRUE;
            }
            break;
        case WM_DRAWITEM:
            if (wParam == PhotoCanvas) { picker->Paint(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE; }
            break;
        case WM_CTLCOLORSTATIC:
            SetBkColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
        case WM_ERASEBKGND: {
            RECT rect{}; GetClientRect(window, &rect); FillRect(reinterpret_cast<HDC>(wParam), &rect, GetSysColorBrush(COLOR_WINDOW)); return TRUE;
        }
        case WM_CLOSE: if (!picker->pending) EndDialog(window, IDCANCEL); return TRUE;
        case WM_DESTROY: KillTimer(window, 1); return TRUE;
        }
    } catch (const std::exception& error) {
        if (message == WM_INITDIALOG || message == WM_DRAWITEM) {
            MessageBoxW(window, Wide(error.what()).c_str(), L"Control picker", MB_OK | MB_ICONERROR);
            EndDialog(window, IDCANCEL);
        } else if (picker) { picker->failure = Wide(error.what()); SetWindowTextW(picker->Item(Result), picker->failure.c_str()); }
    }
    return FALSE;
}
}
void ShowControlPicker(HWND owner, HINSTANCE instance, InspectorService& service, const std::string& id)
{
    Picker picker(service, id, instance);
    // An empty Win32 dialog template; typed child controls are created in WM_INITDIALOG.
    struct EmptyTemplate { DLGTEMPLATE dialog{}; WORD menu{}, windowClass{}, title{}; };
    EmptyTemplate resource;
    resource.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    resource.dialog.cx = 640; resource.dialog.cy = 450;
    const auto result = DialogBoxIndirectParamW(instance, &resource.dialog, owner, DialogProc, reinterpret_cast<LPARAM>(&picker));
    if (result == -1) throw WindowsException("DialogBoxIndirectParamW", GetLastError());
}
}
