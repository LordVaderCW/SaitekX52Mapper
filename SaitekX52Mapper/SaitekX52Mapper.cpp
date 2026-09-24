#include "app/InspectorService.hpp"
#include "util/Text.hpp"
#include "ui/ControlPicker.hpp"
#include "ui/BattlefieldTheme.hpp"
#include "profiles/BattlefieldProfiles.hpp"
#include <commctrl.h>
#include <dbt.h>
#include <shellapi.h>
#include <sstream>
#include <iomanip>
#include <array>
#include <set>
#include <future>
#include "device/MfdSettings.hpp"
#include "device/PowerManagement.hpp"
#include "ui/PropertiesView.hpp"

namespace {
using namespace x52;
constexpr wchar_t WindowClass[] = L"X52BattlefieldMapper.Inspector";
constexpr int NavigationBase = 700;
constexpr std::array PageNames{L"LIVE INPUTS", L"LEARN CONTROLS", L"CONNECTION HEALTH", L"HID INVENTORY", L"BATTLEFIELD PROFILES", L"MFD", L"REGISTRY TWEAKS", L"TEST", L"DEADZONES", L"LEDS"};
enum : int { Refresh = 101, Reinitialize, Export, Folder, Learn, SaveLearn, CaptureStart,
    CaptureStop, Dropout, Returned, Apply, ControlList, Tabs, Candidate, Name, Group, Neutral,
    Hold, Blend, Validation, Stale, CaptureLabel, IdentifySelected,
    ImportBF3, ImportBF4, ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice,
    AddProfileMapping, RemoveProfileMapping, ExportPr0, ProfileMappingList, ProfileInfo, MfdRefresh, MfdClutch, MfdLatched, MfdLight, LedLight,
    ClockSelect, ClockFormat, LedLevel, LedPercent, MfdInfo,
    FilterThrottle, FilterSide, FilterTop, FilterSlider, FilterTime, FilterJitter, FilterApply,
    PowerRefresh, PowerDisable, PowerRestore, PowerInfo, DeadzoneView, DeadzoneRefresh, DeadzoneApply, DeadzoneInfo, TestView, LedRefresh, Zone2, Zone3, DateFormat, Daylight, LedInfo, DeadzoneReload };
struct Child { HWND handle{}; int page{-1}; int x{}, y{}, w{}, h{}; bool stretchX{}, stretchY{}; };
std::wstring Text(HWND window)
{
    const auto count = GetWindowTextLengthW(window);
    std::wstring value(static_cast<std::size_t>(count) + 1, L'\0');
    GetWindowTextW(window, value.data(), count + 1);
    value.resize(static_cast<std::size_t>(count));
    return value;
}
void SetText(HWND window, const std::wstring& value)
{
    if (Text(window) != value && !SetWindowTextW(window, value.c_str()))
        throw WindowsException("SetWindowTextW", GetLastError());
}
std::wstring Number(double value)
{
    std::wostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}
void EnableIfChanged(HWND window, bool enabled)
{
    if ((IsWindowEnabled(window) != FALSE) != enabled) EnableWindow(window, enabled);
}
double Numeric(HWND control)
{
    const auto text = Text(control);
    std::size_t parsed{};
    const auto value = std::stod(text, &parsed);
    if (parsed != text.size() || !std::isfinite(value)) throw std::runtime_error("Enter a finite number without trailing text");
    return value;
}
struct Application {
    BattlefieldTheme theme;
    InspectorService service{DefaultDataDirectory()};
    HWND window{}, title{}, subtitle{}, status{}, tabs{}, list{}, raw{}, learnInfo{}, health{}, inventory{}, footer{};
    HINSTANCE instance{};
    HFONT normal{}, heading{}, mono{};
    HDEVNOTIFY notification{};
    std::vector<Child> children;
    std::vector<std::string> rows, candidates;
    std::vector<std::array<std::wstring, 7>> displayedCells;
    std::vector<std::uint8_t> previousDisplayed;
    std::vector<BitChange> recentChanges;
    ULONGLONG changesUntil{};
    Snapshot last;
    int profileGame{};
    std::vector<BattlefieldBinding> battlefieldBindings, actionChoices;
    std::vector<ProfileButton> profileButtons;
    std::vector<ProfileMapping> profileMappings;
    int page{}, dpi{96};
    std::future<MfdSettings> mfdTask;
    std::optional<MfdSettings> mfdSettings;
    std::optional<DWORD> pendingLed;
    ULONGLONG nextLedWrite{};
    bool liveLedWrite{};
    std::vector<std::pair<MfdOption, DWORD>> pendingMfdChanges;
    bool pendingMfdRefresh{};
    DWORD rememberedLed{100};
    bool filterUiLoaded{}, mfdTried{};
    PropertiesView deadzoneView, testView;
    std::future<DeadzoneSettings> deadzoneTask;
    std::optional<DeadzoneSettings> deadzones;
    bool deadzoneTried{}, deadzoneDirty{};
    bool deadzoneReloading{};
    void StartDeadzones(bool apply=false, bool reload=false)
    {
        if(deadzoneTask.valid())return;
        deadzoneTried=true;
        if((apply || reload) && !deadzones)return;
        if(reload && deadzoneDirty)return;
        deadzoneReloading=reload;
        if(apply)deadzones->axes=deadzoneView.Settings();
        const auto desired=deadzones;const auto directory=service.directory();
        EnableIfChanged(Item(DeadzoneView),false);EnableIfChanged(Item(DeadzoneApply),false);EnableIfChanged(Item(DeadzoneRefresh),false);
        EnableIfChanged(Item(DeadzoneReload),false);
        SetText(Item(DeadzoneInfo),reload?L"Reloading saved calibration without changing limits. This is not a USB or firmware reset...":apply?L"Saving calibration backup, applying and checking readback...":L"Reading Logitech driver calibration...");
        deadzoneTask=std::async(std::launch::async,[apply,reload,desired,directory]{
            if(reload)return ReloadSavedDeadzones(*desired,directory);
            if(apply)return ApplyDeadzones(*desired,directory);
            std::vector<std::wstring> paths;for(const auto& device:EnumerateHid())if(device.isPs28())paths.push_back(device.path);
            if(paths.size()!=1)throw std::runtime_error("Connect exactly one original X52 to edit deadzones");
            return ReadDeadzones(paths.front());
        });
    }
    void PollDeadzones()
    {
        if(page==8 && !deadzoneTried)StartDeadzones();
        if(!deadzoneTask.valid() || deadzoneTask.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try {deadzones=deadzoneTask.get();deadzoneView.Settings(deadzones->axes);deadzoneDirty=false;
            EnableIfChanged(Item(DeadzoneView),true);
            SetText(Item(DeadzoneInfo),deadzoneReloading?L"Saved calibration reloaded; file and limits unchanged. Check centred X/Y in Test and in-game; recovery is not verified.":L"Driver calibration loaded. Drag to edit; Apply saves to the X52 driver. Refresh discards pending edits.");
        }catch(const std::exception& error){deadzones.reset();SetText(Item(DeadzoneInfo),L"Deadzones unavailable: "+Wide(error.what()));}
        EnableIfChanged(Item(DeadzoneRefresh),true);
        EnableIfChanged(Item(DeadzoneReload),deadzones.has_value()&&!deadzoneDirty);
    }
    bool powerTried{};
    std::optional<X52PowerDevice> powerDevice;
    std::unique_ptr<UniqueHandle> powerProcess;
    std::wstring powerResult;
    void RefreshPower()
    {
        powerTried = true; powerDevice.reset();
        EnableIfChanged(Item(PowerDisable), false); EnableIfChanged(Item(PowerRestore), false);
        try {
            const auto devices = ReadX52PowerDevices();
            if (devices.size() != 1) throw std::runtime_error("Connect exactly one original X52 (VID 06A3 / PID 075C), then refresh");
            powerDevice = devices.front();
            const auto& device = *powerDevice;
            const auto backup = PowerBackupPath(service.directory(), device.instance);
            SetText(Item(PowerInfo), L"X52 USB device instance\r\n" + device.instance + L"\r\n\r\nHKLM\\" + device.registryPath +
                L"\r\n\r\nEnhancedPowerManagementEnabled = " + (device.value ? std::to_wstring(*device.value) + L" (DWORD)" : L"not present") +
                L"\r\n" + (device.value == DWORD{0} ? L"Enhanced power management is disabled in the registry." : L"Disable sets this one device value to DWORD 0.") +
                L"\r\n\r\nOriginal-value backup:\r\n" + backup.wstring() + L"\r\n\r\n" + powerResult);
            EnableIfChanged(Item(PowerDisable), !powerProcess && device.value != DWORD{0});
            EnableIfChanged(Item(PowerRestore), !powerProcess && std::filesystem::exists(backup));
        } catch (const std::exception& error) { SetText(Item(PowerInfo), L"Registry settings unavailable: " + Wide(error.what())); }
    }
    void ChangePower(bool restore)
    {
        if (powerProcess) return;
        RefreshPower();
        if (!powerDevice) return;
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size()) throw std::runtime_error("Cannot locate registry helper executable");
        executable.resize(length);
        const auto parameters = std::wstring(restore ? L"--x52-power-restore \"" : L"--x52-power-disable \"") + powerDevice->instance + L"\"";
        SHELLEXECUTEINFOW launch{sizeof(launch)}; launch.fMask = SEE_MASK_NOCLOSEPROCESS; launch.hwnd = window;
        launch.lpVerb = L"runas"; launch.lpFile = executable.c_str(); launch.lpParameters = parameters.c_str(); launch.nShow = SW_HIDE;
        if (!ShellExecuteExW(&launch)) {
            if (GetLastError() == ERROR_CANCELLED) { powerResult = L"Administrator approval was cancelled. No registry change was applied."; RefreshPower(); return; }
            throw WindowsException("Start administrator helper", GetLastError());
        }
        powerProcess = std::make_unique<UniqueHandle>(launch.hProcess);
        powerResult = L"Applying the requested value and verifying registry readback...";
        EnableIfChanged(Item(PowerRefresh), false); RefreshPower();
    }
    void PollPower()
    {
        if (page == 6 && !powerTried) RefreshPower();
        if (!powerProcess || WaitForSingleObject(powerProcess->get(), 0) != WAIT_OBJECT_0) return;
        DWORD code = 1; GetExitCodeProcess(powerProcess->get(), &code); powerProcess.reset();
        powerResult = code == 0 ? L"Registry value verified. Reconnect the X52 to the same USB port, or restart Windows, before testing.\r\nThis is a power-management experiment; dropout improvement is not yet verified." :
            L"The change did not complete. Review the helper error and current registry value before retrying.";
        EnableIfChanged(Item(PowerRefresh), true); RefreshPower();
    }
    bool Checked(int id) const { return SendMessageW(Item(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
    void Check(int id, bool value) { if (Checked(id) != value) SendMessageW(Item(id), BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0); }
    void MfdEnabled(bool enabled)
    {
        for (const auto id : {MfdClutch, MfdLight, LedLight, ClockSelect, ClockFormat, Zone2, Zone3, DateFormat, Daylight}) EnableIfChanged(Item(id), enabled);
        EnableIfChanged(Item(LedLevel), enabled);
        EnableIfChanged(Item(MfdLatched), enabled && Checked(MfdClutch));
    }
    void StartMfd(std::optional<std::pair<MfdOption, DWORD>> change = {}, bool fromSlider = false)
    {
        if (mfdTask.valid()) {
            if (change) {
                const auto existing = std::find_if(pendingMfdChanges.begin(), pendingMfdChanges.end(),
                    [&](const auto& queued) { return queued.first == change->first; });
                if (existing != pendingMfdChanges.end()) existing->second = change->second;
                else pendingMfdChanges.push_back(*change);
            } else pendingMfdRefresh = true;
            return;
        }
        mfdTried = true;
        liveLedWrite = fromSlider;
        // Live writes leave unrelated controls untouched. Other edits are queued
        // behind the current driver operation instead of being dropped.
        if (!mfdSettings) { MfdEnabled(false); EnableIfChanged(Item(MfdRefresh), false); }
        if (!fromSlider) SetText(Item(MfdInfo), change ? L"Applying setting and reading it back from the driver..." : L"Reading settings from the X52 driver...");
        mfdTask = std::async(std::launch::async, [change] {
            std::vector<std::wstring> paths;
            for (const auto& device : EnumerateHid()) if (device.isPs28()) paths.push_back(device.path);
            if (paths.size() != 1) throw std::runtime_error("Connect exactly one original X52 to edit its settings");
            return change ? SetMfdOption(paths.front(), change->first, change->second) : ReadMfdSettings(paths.front());
        });
    }
    bool MfdQueued(MfdOption option) const
    {
        return std::any_of(pendingMfdChanges.begin(), pendingMfdChanges.end(),
            [option](const auto& queued) { return queued.first == option; });
    }
    void SendPendingMfd()
    {
        if (mfdTask.valid()) return;
        if (!pendingMfdChanges.empty()) {
            const auto change = pendingMfdChanges.front();
            pendingMfdChanges.erase(pendingMfdChanges.begin());
            StartMfd(change);
        } else if (pendingMfdRefresh) { pendingMfdRefresh = false; StartMfd(); }
    }
    void ShowClockFormat()
    {
        const auto clock = SendMessageW(Item(ClockSelect), CB_GETCURSEL, 0, 0);
        if (mfdSettings && clock >= 0 && clock < 3 && !MfdQueued(static_cast<MfdOption>(static_cast<int>(MfdOption::Clock1) + clock))) {
            const auto format = mfdSettings->twelveHour[static_cast<std::size_t>(clock)] ? 1 : 0;
            if (SendMessageW(Item(ClockFormat), CB_GETCURSEL, 0, 0) != format)
                SendMessageW(Item(ClockFormat), CB_SETCURSEL, format, 0);
        }
    }
    void SendPendingLed()
    {
        if (!pendingLed || !mfdSettings || mfdTask.valid()) return;
        if (*pendingLed == mfdSettings->ledBrightness) {
            pendingLed.reset(); return;
        }
        if (GetTickCount64() < nextLedWrite) return;
        const auto value = *pendingLed;
        pendingLed.reset();
        nextLedWrite = GetTickCount64() + 33;
        StartMfd({{MfdOption::LedBrightness, value}}, true);
    }
    void LedSlider()
    {
        const auto value = static_cast<DWORD>(SendMessageW(Item(LedLevel), TBM_GETPOS, 0, 0));
        SetText(Item(LedPercent), std::to_wstring(value) + L"%");
        if (mfdSettings) {
            // Replace a pending value, including when reversing to the previous
            // readback while a different write is still in flight.
            pendingLed = value;
            SendPendingLed();
        }
    }
    void PollMfd()
    {
        if ((page == 5 || page == 9) && !mfdTried) StartMfd();
        SendPendingMfd();
        SendPendingLed();
        if (!mfdTask.valid() || mfdTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            mfdSettings = mfdTask.get(); const auto& values = *mfdSettings;
            if (!MfdQueued(MfdOption::Clutch)) Check(MfdClutch, values.clutch);
            if (!MfdQueued(MfdOption::Latched)) Check(MfdLatched, values.latched);
            if (!MfdQueued(MfdOption::MfdBrightness)) Check(MfdLight, values.mfdBrightness != 0);
            if (!MfdQueued(MfdOption::LedBrightness) && !pendingLed) Check(LedLight, values.ledBrightness != 0);
            if (values.ledBrightness) rememberedLed = values.ledBrightness;
            if (!liveLedWrite && !pendingLed && !MfdQueued(MfdOption::LedBrightness)) {
                if (SendMessageW(Item(LedLevel), TBM_GETPOS, 0, 0) != values.ledBrightness)
                    SendMessageW(Item(LedLevel), TBM_SETPOS, TRUE, values.ledBrightness);
                SetText(Item(LedPercent), std::to_wstring(values.ledBrightness) + L"%");
            }
            ShowClockFormat();
            const auto select=[&](int id,int value){if(SendMessageW(Item(id),CB_GETCURSEL,0,0)!=value)SendMessageW(Item(id),CB_SETCURSEL,value,0);};
            if(!MfdQueued(MfdOption::DateFormat))select(DateFormat,static_cast<int>(values.dateFormat));
            if(!MfdQueued(MfdOption::Daylight))Check(Daylight,values.daylight);
            for(int i=0;i<2;++i)if(!MfdQueued(i==0?MfdOption::Zone2:MfdOption::Zone3))select(i==0?Zone2:Zone3,static_cast<int>(std::find(X52TimeZones.begin(),X52TimeZones.end(),values.zoneMinutes[static_cast<std::size_t>(i)])-X52TimeZones.begin()));
            MfdEnabled(true);
            SetText(Item(MfdInfo), values.clutch ?
                L"I is currently reserved for Logitech profile selection. Uncheck the option above to use I as a regular button.\r\nPress once to latch means press I to enter profile selection, then press it again to exit; otherwise hold I." :
                L"I is available as a regular button, including in our profile editor.\r\nChanges are checked against the driver after applying.");
        } catch (const std::exception& error) {
            pendingLed.reset();
            pendingMfdChanges.clear(); pendingMfdRefresh = false;
            mfdSettings.reset(); MfdEnabled(false);
            SetText(Item(MfdInfo), L"Settings unavailable: " + Wide(error.what()) + L"\r\nRefresh to read the actual state before making another change.");
        }
        EnableIfChanged(Item(MfdRefresh), true);
        liveLedWrite = false;
        SendPendingMfd();
        SendPendingLed();
    }

    ~Application()
    {
        service.Stop();
        if (notification) UnregisterDeviceNotification(notification);
        for (auto font : {normal, heading, mono}) if (font) DeleteObject(font);
    }
    HWND Item(int id) const { return GetDlgItem(window, id); }
    HWND Add(const wchar_t* cls, const wchar_t* text, int id, int targetPage,
        int x, int y, int w, int h, DWORD style = 0, bool sx = false, bool sy = false)
    {
        const bool isEdit = std::wstring_view(cls) == L"EDIT";
        if (std::wstring_view(cls) == L"STATIC" && !(style & SS_OWNERDRAW)) style |= SS_NOPREFIX;
        if (std::wstring_view(cls) == L"COMBOBOX") style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
        if (std::wstring_view(cls) == L"LISTBOX") style |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
        const auto handle = CreateWindowExW(0, cls, text,
            WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        if (!handle) throw WindowsException("CreateWindowExW (control)", GetLastError());
        children.push_back({handle, targetPage, x, y, w, h, sx, sy});
        if (isEdit) SendMessageW(handle, EM_SETLIMITTEXT, 4 * 1024 * 1024, 0);
        theme.Style(handle);
        return handle;
    }
    void Fonts()
    {
        for (auto font : {normal, heading, mono}) if (font) DeleteObject(font);
        normal = CreateFontW(-MulDiv(16, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Bahnschrift");
        heading = CreateFontW(-MulDiv(38, dpi, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Bahnschrift SemiCondensed");
        mono = CreateFontW(-MulDiv(13, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        if (!normal || !heading || !mono) throw WindowsException("CreateFontW", GetLastError());
        for (const auto& child : children) SendMessageW(child.handle, WM_SETFONT,
            reinterpret_cast<WPARAM>(child.handle == title ? heading :
                (child.handle == raw || child.handle == inventory || child.handle == learnInfo ? mono : normal)), TRUE);
    }
    void Create()
    {
        dpi = static_cast<int>(GetDpiForWindow(window));
        theme.Attach(window);
        title = Add(L"STATIC", PageNames[0], 0, -1, 36, 54, 980, 50);
        subtitle = Add(L"STATIC", L"X52 BATTLEFIELD MAPPER   /   DEVICE CONTROL   /", 0, -1, 38, 27, 1150, 23, 0, true);
        status = Add(L"STATIC", L"Discovering Saitek X52...", 0, -1, 38, 116, 1190, 23, 0, true);
        Add(L"BUTTON", L"Reinitialize X52 input", Reinitialize, -1, 24, 129, 190, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Refresh inventory", Refresh, -1, 226, 129, 156, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Export diagnostic", Export, -1, 394, 129, 160, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Open data folder", Folder, -1, 566, 129, 152, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Identify input...", IdentifySelected, -1, 730, 129, 246, 32, WS_TABSTOP);
        int i = 0;
        for (const auto label : PageNames) {
            const auto nav = Add(L"BUTTON", label, NavigationBase + i, -1, 36, 184 + i * 32, 224, 31, WS_TABSTOP);
            SetPropW(nav, L"X52.Navigation", reinterpret_cast<HANDLE>(1));
            if (i++ == 0) SetPropW(nav, L"X52.Selected", reinterpret_cast<HANDLE>(1));
        }
        Add(L"STATIC", L"", 710, -1, 274, 184, 1, 490, SS_OWNERDRAW, false, true);
        Add(L"STATIC", L"", 711, -1, 36, 694, 1200, 1, SS_OWNERDRAW, true);
        Add(L"STATIC", L"Double-click a HID row (or press Enter) to link it to a physical X52 button, hat, switch or axis.", 0, 0, 24, 220, 952, 24, 0, true);
        list = Add(WC_LISTVIEWW, L"", ControlList, 0, 24, 254, 952, 204,
            WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, true, true);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        i = 0;
        for (const auto& column : std::array<std::pair<const wchar_t*, int>, 7>{{
            {L"HID usage / report / link", 185}, {L"Physical control link", 260}, {L"Group", 80},
            {L"Raw", 65}, {L"Range", 92}, {L"Normalized", 105}, {L"Safe internal", 145}}}) {
            LVCOLUMNW item{}; item.mask = LVCF_TEXT | LVCF_WIDTH; item.pszText = const_cast<wchar_t*>(column.first);
            item.cx = MulDiv(column.second, dpi, 96);
            if (ListView_InsertColumn(list, i++, &item) == -1) throw std::runtime_error("Unable to create list column");
        }
        raw = Add(L"EDIT", L"Waiting for reports", 0, 0, 24, 474, 952, 74,
            ES_READONLY | ES_MULTILINE | WS_VSCROLL | WS_TABSTOP, true);
        Add(L"STATIC", L"1. Start a baseline.   2. Move ONE control.   3. Select its HID usage and identify it in the photo picker.", 0, 1, 24, 220, 952, 24, 0, true);
        Add(L"BUTTON", L"Start Learn Mode", Learn, 1, 24, 256, 164, 32, WS_TABSTOP);
        Add(L"STATIC", L"Changed HID usage", 0, 1, 204, 260, 150, 25);
        Add(L"COMBOBOX", L"", Candidate, 1, 356, 256, 370, 280, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        Add(L"STATIC", L"Physical control name", 0, 1, 24, 312, 170, 25);
        Add(L"BUTTON", L"Identify candidate...", SaveLearn, 1, 204, 306, 250, 32, WS_TABSTOP);
        Add(L"STATIC", L"The picker includes actual reference photos and named X52 controls. Saved links can be corrected from any live-input row.", 0, 1, 24, 356, 952, 44, 0, true);
        learnInfo = Add(L"EDIT", L"No baseline recorded.", 0, 1, 24, 414, 952, 260, ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, true, true);
        Add(L"STATIC", L"Capture label / cable", 0, 2, 24, 225, 160, 24);
        Add(L"EDIT", L"Old 5 m cable - describe conditions", CaptureLabel, 2, 190, 219, 500, 30, WS_TABSTOP | ES_AUTOHSCROLL);
        Add(L"BUTTON", L"Start capture", CaptureStart, 2, 24, 263, 198, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Stop / save capture", CaptureStop, 2, 234, 263, 174, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Mark stick dropout", Dropout, 2, 420, 263, 174, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Mark stick returned", Returned, 2, 606, 263, 180, 32, WS_TABSTOP);
        Add(L"STATIC", L"Dropout output", 0, 2, 24, 318, 115, 24);
        const auto hold = Add(L"COMBOBOX", L"", Hold, 2, 145, 310, 240, 180, CBS_DROPDOWNLIST | WS_TABSTOP);
        for (const auto label : {L"Neutralise (recommended)", L"Hold last state"}) SendMessageW(hold, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        SendMessageW(hold, CB_SETCURSEL, 0, 0);
        Add(L"STATIC", L"Blend ms", 0, 2, 405, 318, 70, 24);
        Add(L"EDIT", L"100", Blend, 2, 480, 310, 60, 30, ES_NUMBER | WS_TABSTOP);
        Add(L"STATIC", L"Valid reports", 0, 2, 558, 318, 95, 24);
        Add(L"EDIT", L"5", Validation, 2, 660, 310, 60, 30, ES_NUMBER | WS_TABSTOP);
        Add(L"STATIC", L"Freshness ms", 0, 2, 24, 358, 115, 24);
        Add(L"EDIT", L"2000", Stale, 2, 145, 351, 80, 30, ES_NUMBER | WS_TABSTOP);
        Add(L"BUTTON", L"Apply settings", Apply, 2, 245, 350, 140, 32, WS_TABSTOP);
        Add(L"STATIC", L"Hold-last can sustain a turn. No reports means stale input; it does not prove a stick fault.", 0, 2, 405, 357, 550, 40);
        health = Add(L"EDIT", L"", 0, 2, 24, 406, 952, 268, ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, true, true);
        inventory = Add(L"EDIT", L"Enumerating...", 0, 3, 24, 220, 952, 454,
            ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP, true, true);
        Add(L"BUTTON", L"Import Battlefield 3", ImportBF3, 4, 24, 220, 180, 30, WS_TABSTOP);
        Add(L"BUTTON", L"Import Battlefield 4", ImportBF4, 4, 216, 220, 180, 30, WS_TABSTOP);
        Add(L"STATIC", L"Imports joystick / joypad bindings from your saved game settings.", 0, 4, 415, 224, 560, 25, 0, true);
        Add(L"STATIC", L"X52 mode", 0, 4, 24, 260, 210, 23);
        Add(L"COMBOBOX", L"", ProfileMode, 4, 24, 283, 270, 220, CBS_DROPDOWNLIST | WS_TABSTOP);
        FillCombo(ProfileMode, {L"Mode 1", L"Mode 2", L"Mode 3",
            L"Mode 1 + Pinkie", L"Mode 2 + Pinkie", L"Mode 3 + Pinkie"});
        Add(L"STATIC", L"Identified physical X52 control / HID link", 0, 4, 310, 260, 650, 23);
        Add(L"COMBOBOX", L"", ProfileControl, 4, 310, 283, 666, 300, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, true);
        Add(L"STATIC", L"Game context", 0, 4, 24, 320, 155, 23);
        Add(L"COMBOBOX", L"", ProfileContext, 4, 24, 344, 155, 220, CBS_DROPDOWNLIST | WS_TABSTOP);
        Add(L"STATIC", L"Existing Battlefield joystick / joypad binding", 0, 4, 191, 320, 570, 23);
        Add(L"COMBOBOX", L"", ProfileActionChoice, 4, 191, 344, 570, 360, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        Add(L"BUTTON", L"Assign / replace", AddProfileMapping, 4, 777, 341, 199, 30, WS_TABSTOP);
        Add(L"LISTBOX", L"", ProfileMappingList, 4, 24, 391, 952, 173,
            LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, true);
        SendMessageW(Item(ProfileMappingList), LB_SETHORIZONTALEXTENT, MulDiv(1350, dpi, 96), 0);
        Add(L"BUTTON", L"Remove selected override", RemoveProfileMapping, 4, 24, 576, 230, 30, WS_TABSTOP);
        Add(L"BUTTON", L"Export joystick plan", ExportPr0, 4, 752, 576, 224, 30, WS_TABSTOP);
        Add(L"STATIC", L"Import BF3 or BF4 to see saved joystick buttons, axes and inversion. Choose an identified X52 control.\r\nAssignments are saved as a mapping plan; joystick output and PR0 joystick export are not implemented.", ProfileInfo, 4, 24, 619, 952, 63, 0, true);
        for (const auto item : {ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), FALSE);
        Add(L"STATIC", L"X52 device settings", 0, 5, 24, 220, 500, 24);
        Add(L"BUTTON", L"Refresh from X52", MfdRefresh, 5, 752, 215, 224, 30, WS_TABSTOP);
        Add(L"BUTTON", L"Use I for Logitech profile selection", MfdClutch, 5, 24, 255, 360, 26, BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(L"BUTTON", L"Press once to latch (instead of holding I)", MfdLatched, 5, 420, 255, 440, 26, BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(L"BUTTON", L"MFD backlight on", MfdLight, 5, 24, 293, 235, 26, BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(L"BUTTON", L"Button LEDs on", LedLight, 9, 24, 332, 210, 26, BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(L"STATIC", L"LED brightness", 0, 9, 250, 332, 130, 26);
        Add(TRACKBAR_CLASSW, L"", LedLevel, 9, 385, 326, 490, 38, TBS_HORZ | TBS_NOTICKS | WS_TABSTOP);
        SendMessageW(Item(LedLevel), TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendMessageW(Item(LedLevel), TBM_SETPAGESIZE, 0, 10);
        Add(L"STATIC", L"--%", LedPercent, 9, 891, 332, 70, 26);
        Add(L"STATIC", L"Clock on the MFD", 0, 5, 24, 345, 150, 26);
        Add(L"COMBOBOX", L"", ClockSelect, 5, 180, 340, 260, 160, CBS_DROPDOWNLIST | WS_TABSTOP);
        FillCombo(ClockSelect, {L"Clock 1", L"Clock 2", L"Clock 3"});
        Add(L"STATIC", L"Time format", 0, 5, 460, 345, 110, 26);
        Add(L"COMBOBOX", L"", ClockFormat, 5, 580, 340, 395, 160, CBS_DROPDOWNLIST | WS_TABSTOP);
        FillCombo(ClockFormat, {L"24-hour (e.g. 18:30)", L"12-hour (e.g. 6:30 PM)"});
        Add(L"BUTTON",L"Clock 1: daylight time adjustment",Daylight,5,24,390,420,26,BS_AUTOCHECKBOX|WS_TABSTOP);
        Add(L"STATIC",L"Clock 2 time zone",0,5,24,438,210,26);
        Add(L"COMBOBOX",L"",Zone2,5,250,432,340,240,CBS_DROPDOWNLIST|WS_TABSTOP);
        Add(L"STATIC",L"Clock 3 time zone",0,5,24,483,210,26);
        Add(L"COMBOBOX",L"",Zone3,5,250,477,340,240,CBS_DROPDOWNLIST|WS_TABSTOP);
        for(const int minutes:X52TimeZones){wchar_t label[40]{};swprintf_s(label,L"GMT %c%02d:%02d",minutes<0?L'-':L'+',abs(minutes)/60,abs(minutes)%60);
            for(const auto id:{Zone2,Zone3})SendMessageW(Item(id),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));}
        Add(L"STATIC",L"Date format",0,5,24,528,210,26);
        Add(L"COMBOBOX",L"",DateFormat,5,250,522,340,160,CBS_DROPDOWNLIST|WS_TABSTOP);
        FillCombo(DateFormat,{L"MM-DD-YY",L"DD-MM-YY",L"YY-MM-DD"});
        Add(L"STATIC",L"Open this tab to read the device settings.",MfdInfo,5,24,580,952,75,0,true);
        Add(L"STATIC",L"X52 button illumination",0,9,24,220,600,26);
        Add(L"BUTTON",L"Refresh from X52",LedRefresh,9,752,215,224,30,WS_TABSTOP);
        Add(L"STATIC",L"Brightness updates on the device while you drag. Arrow keys make fine adjustments.",0,9,24,270,952,28,0,true);
        Add(L"STATIC",L"",LedInfo,9,24,400,952,100,0,true);
        Add(L"STATIC",L"Test the driver-reported axes, buttons and hats. No mapper smoothing is applied to this view.",0,7,24,220,952,28,0,true);
        testView.Attach(Add(L"STATIC",L"",TestView,7,24,265,952,410,0,true,true),theme,false);
        Add(L"STATIC",L"Drag the four handles: minimum, centre low, centre high, maximum. Red marks the reported input.",0,8,24,215,952,24,0,true);
        Add(L"STATIC",L"Keyboard: Up/Down choose axis; Space chooses handle; Left/Right adjust; Shift makes larger steps.",0,8,24,245,952,24,0,true);
        deadzoneView.Attach(Add(L"STATIC",L"",DeadzoneView,8,24,280,952,340,WS_TABSTOP|SS_NOTIFY,true,true),theme,true);
        EnableIfChanged(Item(DeadzoneView),false);
        Add(L"BUTTON",L"Refresh from driver",DeadzoneRefresh,8,24,635,245,30,WS_TABSTOP);
        Add(L"BUTTON",L"Reload saved calibration",DeadzoneReload,8,295,635,310,30,WS_TABSTOP);
        EnableIfChanged(Item(DeadzoneReload),false);
        Add(L"BUTTON",L"Apply driver deadzones",DeadzoneApply,8,730,635,245,30,WS_TABSTOP);
        EnableIfChanged(Item(DeadzoneApply),false);
        Add(L"STATIC",L"",DeadzoneInfo,8,24,674,952,35,0,true);
        Add(L"STATIC", L"Noise filtering for identified throttle axes", 0, 0, 24, 558, 952, 22, 0, true);
        for (int axis = 0; axis < 4; ++axis) {
            constexpr std::array labels{L"Throttle lever", L"Side rotary", L"Top rotary", L"Thumb slider"};
            Add(L"BUTTON", labels[static_cast<std::size_t>(axis)], FilterThrottle + axis, 0, 24 + axis * 238, 582, 230, 26, BS_AUTOCHECKBOX | WS_TABSTOP);
        }
        Add(L"STATIC", L"Smoothing (ms)", 0, 0, 24, 622, 145, 24);
        Add(L"EDIT", L"45", FilterTime, 0, 173, 616, 70, 28, WS_TABSTOP);
        Add(L"STATIC", L"Jitter tolerance (raw counts)", 0, 0, 270, 622, 225, 24);
        Add(L"EDIT", L"1", FilterJitter, 0, 505, 616, 70, 28, WS_TABSTOP);
        Add(L"BUTTON", L"Save input filtering", FilterApply, 0, 752, 616, 224, 30, WS_TABSTOP);
        Add(L"STATIC", L"Filters smooth Normalized and Safe internal values here. Raw readings stay unchanged; direct game input is unaffected.", 0, 0, 24, 662, 952, 27, 0, true);
        MfdEnabled(false);
        Add(L"STATIC", L"Original X52 - USB power management", 0, 6, 24, 220, 952, 26);
        Add(L"STATIC", L"Applies only to the connected X52 USB instance (06A3:075C). Other devices and global power settings stay unchanged.\r\nWindows administrator approval is required to apply or restore. Opening this page only reads the registry.", 0, 6, 24, 260, 952, 55, 0, true);
        Add(L"EDIT", L"", PowerInfo, 6, 24, 330, 952, 255, ES_MULTILINE | ES_READONLY | WS_VSCROLL, true, true);
        Add(L"BUTTON", L"Refresh registry", PowerRefresh, 6, 24, 610, 230, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Disable for this X52", PowerDisable, 6, 270, 610, 285, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Restore original value", PowerRestore, 6, 573, 610, 300, 32, WS_TABSTOP);
        EnableIfChanged(Item(PowerDisable), false); EnableIfChanged(Item(PowerRestore), false);
        footer = Add(L"STATIC", L"", 0, -1, 36, 707, 1200, 36, 0, true);
        Fonts();
        DEV_BROADCAST_DEVICEINTERFACE_W filter{};
        filter.dbcc_size = sizeof(filter); filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        HidD_GetHidGuid(&filter.dbcc_classguid);
        notification = RegisterDeviceNotificationW(window, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
        if (!notification) throw WindowsException("RegisterDeviceNotificationW", GetLastError());
        if (!SetTimer(window, 1, 100, nullptr)) throw WindowsException("SetTimer", GetLastError());
        if (!SetTimer(window, 2, 33, nullptr)) throw WindowsException("SetTimer (live brightness)", GetLastError());
        service.Start();
        Layout();
    }
    void Layout()
    {
        theme.Resize(); // update backdrop before children paint at their new positions
        RECT rect{}; GetClientRect(window, &rect);
        const auto extraX = MulDiv(rect.right, 96, dpi) - 1280;
        const auto extraY = MulDiv(rect.bottom, 96, dpi) - 750;
        for (const auto& child : children) {
            auto x = child.x, y = child.y, w = child.w;
            if (child.page >= 0) { x += 264; y -= 35; }
            const auto id = GetDlgCtrlID(child.handle);
            const std::array actions{Reinitialize, Refresh, Export, Folder, IdentifySelected};
            const auto action = std::find(actions.begin(), actions.end(), id);
            if (action != actions.end()) { x = 36; y = 516 + static_cast<int>(action - actions.begin()) * 36; w = 224; }
            if (child.handle == footer || id == 711 || (child.page == 0 && child.y >= 474)) y += extraY;
            if ((child.page == 6 && child.y >= 610) || (child.page==8 && child.y>=635)) y += extraY;
            if (!MoveWindow(child.handle, MulDiv(x, dpi, 96), MulDiv(y, dpi, 96),
                MulDiv(std::max(1, w + (child.stretchX ? extraX : 0)), dpi, 96),
                MulDiv(std::max(1, child.h + (child.stretchY ? extraY : 0)), dpi, 96), TRUE))
                throw WindowsException("MoveWindow", GetLastError());
            ShowWindow(child.handle, child.page == -1 || child.page == page ? SW_SHOW : SW_HIDE);
        }
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    void Poll()
    {
        last = service.Read();
        if (!filterUiLoaded && last.filtersReady) {
            for (int i = 0; i < 4; ++i) Check(FilterThrottle + i, last.filters.enabled[static_cast<std::size_t>(i)]);
            SetText(Item(FilterTime), Number(last.filters.smoothingMs)); SetText(Item(FilterJitter), Number(last.filters.jitterCounts));
            filterUiLoaded = true;
        }
        EnableIfChanged(Item(FilterApply), filterUiLoaded);
        PollMfd();
        PollPower();
        PollDeadzones();
        if (page == 7) testView.Inputs(last.physical,last.connected);
        if (page == 8) deadzoneView.Inputs(last.physical,last.connected);
        SetText(Item(LedInfo),mfdSettings ? L"LED settings are read back from the driver after each change." : Text(Item(MfdInfo)));
        SetText(status, (last.connected ? L"CONNECTED   |   " : L"NOT CONNECTED   |   ") + last.device);
        SetText(footer, Wide(last.error.empty() ? last.status : "ERROR: " + last.error));
        EnableIfChanged(Item(Learn), last.connected && last.sequence > 0);
        EnableIfChanged(Item(SaveLearn), last.learning && !last.learnCandidates.empty());
        EnableIfChanged(Item(CaptureStart), !last.captureActive);
        EnableIfChanged(Item(Returned), last.recovery.stickSuspended);
        if (page == 0) {
            std::vector<std::string> keys;
            for (const auto& [id, control] : last.physical.controls) { (void)control; keys.push_back(id); }
            if (keys != rows) {
                rows = keys; ListView_DeleteAllItems(list);
                displayedCells.assign(rows.size(), {});
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    auto text = Wide(rows[i]);
                    LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i); item.pszText = text.data();
                    ListView_InsertItem(list, &item);
                }
            }
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto& control = last.physical.controls.at(rows[i]);
                const auto assignment = last.assignments.find(rows[i]);
                const auto safe = last.safe.controls.find(rows[i]);
                const auto set = [&](int column, std::wstring value) {
                    auto& previous = displayedCells[i][static_cast<std::size_t>(column)];
                    if (previous != value) {
                        ListView_SetItemText(list, static_cast<int>(i), column, value.data());
                        previous = std::move(value);
                    }
                };
                set(1, assignment == last.assignments.end() ? L"Unassigned" : Wide(assignment->second.name));
                set(2, assignment == last.assignments.end() || assignment->second.group == InputGroup::Unknown ? L"Unknown" :
                    assignment->second.group == InputGroup::Stick ? L"Stick" : L"Throttle");
                set(3, std::to_wstring(control.raw));
                set(4, std::to_wstring(control.minimum) + L".." + std::to_wstring(control.maximum));
                const auto filtered = last.filtered.controls.find(rows[i]);
                set(5, control.kind == ControlKind::Button ? (control.raw ? L"PRESSED" : L"released") :
                    Number(filtered == last.filtered.controls.end() ? control.normalized : filtered->second.normalized));
                set(6, safe == last.safe.controls.end() ? L"unavailable" : Number(safe->second.normalized) + (safe->second.valid ? L"" : L" [withheld]"));
            }
            const auto differences = Differences(previousDisplayed, last.raw);
            if (!differences.empty()) { recentChanges = differences; changesUntil = GetTickCount64() + 750; }
            previousDisplayed = last.raw;
            std::wostringstream rawText;
            rawText << L"RAW HID   " << Wide(last.utc) << L"   " << last.raw.size() << L" bytes (Windows report buffer)\r\n";
            for (std::size_t i = 0; i < last.raw.size(); ++i) {
                const bool changed = GetTickCount64() < changesUntil && std::any_of(recentChanges.begin(), recentChanges.end(),
                    [i](const auto& change) { return change.byte == i; });
                rawText << (changed ? L"[" : L" ") << std::hex << std::uppercase << std::setfill(L'0')
                    << std::setw(2) << static_cast<unsigned>(last.raw[i]) << (changed ? L"]" : L" ") << L' ';
            }
            rawText << L"\r\nRecent display changes (byte and bit indices are zero-based): ";
            for (const auto& change : recentChanges) {
                rawText << std::dec << L"byte " << change.byte << L" bits ";
                for (unsigned bit = 0; bit < 8; ++bit) if (change.mask & (1u << bit)) rawText << bit << L' ';
                rawText << L"; ";
            }
            SetText(raw, rawText.str());
        } else if (page == 1) {
            if (candidates != last.learnCandidates) {
                const auto selected = Text(Item(Candidate));
                candidates = last.learnCandidates;
                SendMessageW(Item(Candidate), CB_RESETCONTENT, 0, 0);
                for (const auto& id : candidates) SendMessageW(Item(Candidate), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(id).c_str()));
                auto index = SendMessageW(Item(Candidate), CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(selected.c_str()));
                SendMessageW(Item(Candidate), CB_SETCURSEL, index == CB_ERR ? 0 : index, 0);
            }
            std::ostringstream learningText;
            learningText << "Observed raw ranges since baseline (small spans may be noise):\n";
            for (const auto& id : last.learnCandidates) {
                const auto range = last.learnRanges.find(id);
                if (range != last.learnRanges.end()) learningText << id << "   " << range->second.first << " .. " << range->second.second << '\n';
            }
            learningText << "\n" << (last.learnEvidence.is_null() || last.learnEvidence.empty() ?
                "Start Learn Mode, then press or move one physical control. Axis jitter may create extra candidates." : last.learnEvidence.dump(2));
            SetText(learnInfo, WindowsLines(learningText.str()));
        } else if (page == 2) {
            const auto& metrics = last.recovery.health.metrics;
            const auto msNow = 1000.0 * static_cast<double>(Qpc()) / static_cast<double>(QpcFrequency());
            std::wostringstream text;
            text << L"Stick/throttle comparison: LOG ONLY (no automatic input or recovery changes)\r\n"
                << L"Activity observations: " << last.activityObservations
                << (last.activityObservationActive ? L"  |  Stick unchanged while throttle activity observed" : L"  |  No active observation") << L"\r\n";
            for (const auto group : {InputGroup::Stick, InputGroup::Throttle, InputGroup::Unknown}) {
                const auto& activity = last.activity.at(static_cast<std::size_t>(group));
                text << (group == InputGroup::Stick ? L"Stick" : group == InputGroup::Throttle ? L"Throttle" : L"Unassigned")
                    << L": " << activity.controls << L" inputs | raw axis changes: " << activity.axisChanges
                    << L" | button/hat changes: " << activity.digitalChanges;
                if (activity.lastChangeMs) text << L" | last: " << Wide(activity.lastControl) << L" ("
                    << static_cast<long long>(msNow - *activity.lastChangeMs) << L" ms ago)";
                text << L"\r\n";
            }
            text << L"Raw axis changes include noise. Holding the stick still can produce the same observation.\r\n\r\n"
                << L"USB handle: " << (last.connected ? L"OPEN" : L"CLOSED")
                << L"     Report freshness: " << (last.recovery.transportAvailable ? L"CURRENT" : L"UNAVAILABLE / STALE")
                << L"\r\nStick health: " << Wide(HealthName(last.recovery.health.stick))
                << L"\r\nRecovery: " << Wide(RecoveryName(last.recovery.state))
                << L"     Last report: " << (last.sequence ? std::to_wstring(static_cast<long long>(msNow - last.lastReportMs)) + L" ms ago" : L"none")
                << L"\r\nReports: " << metrics.reportsReceived << L"     Read / validation failures: " << metrics.readFailures
                << L"\r\nUser-marked dropouts: " << metrics.suspectedDropouts << L"     Verified protocol dropouts: " << metrics.confirmedDropouts
                << L"\r\nSuccessful input recoveries: " << metrics.successfulRecoveries;
            if (last.recovery.lastInterruptionMs) text << L"     Last interruption: " << static_cast<long long>(*last.recovery.lastInterruptionMs) << L" ms";
            text << L"\r\nLearned throttle changes during marked dropout: " << last.throttleChangesDuringDropout
                << L"     Learned stick changes: " << last.stickChangesDuringDropout
                << L"\r\nCapture: " << (last.captureActive ? L"RECORDING" : L"stopped") << L"     Reports captured: " << last.captureReports
                << L"\r\nDecode + internal safety processing: " << Number(last.processMs) << L" ms (latest sample; excludes USB/UI/disk)"
                << L"\r\n\r\nNo verified stick-side signature. Markers record your observation, not an automatic diagnosis."
                << L"\r\nRecent raw history: up to 10 seconds (bounded by size). Mark a physical dropout promptly to retain it."
                << L"\r\nReinitialize restarts Windows reads; it does not electrically reset the joystick."
                << L"\r\nVirtual controller: not implemented in this milestone.\r\nData: " << service.directory().wstring();
            SetText(health, text.str());
        } else SetText(inventory, last.inventory);
    }
    void Command(int id)
    {
        if (id >= NavigationBase && id < NavigationBase + static_cast<int>(PageNames.size())) {
            page = id - NavigationBase;
            for (int i = 0; i < static_cast<int>(PageNames.size()); ++i) {
                const auto nav = Item(NavigationBase + i);
                if (i == page) SetPropW(nav, L"X52.Selected", reinterpret_cast<HANDLE>(1));
                else RemovePropW(nav, L"X52.Selected");
                InvalidateRect(nav, nullptr, FALSE);
            }
            SetText(title, PageNames[static_cast<std::size_t>(page)]); Layout(); Poll(); return;
        }
        switch (id) {
        case DeadzoneRefresh: StartDeadzones(); break;
        case DeadzoneApply: StartDeadzones(true); break;
        case DeadzoneView: deadzoneDirty=true; EnableIfChanged(Item(DeadzoneApply),deadzones.has_value()&&!deadzoneTask.valid()); EnableIfChanged(Item(DeadzoneReload),false); SetText(Item(DeadzoneInfo),L"Pending changes. Apply writes driver calibration; Refresh discards these edits.");break;
        case DeadzoneReload: StartDeadzones(false,true);break;
        case LedRefresh: StartMfd();break;
        case Daylight: StartMfd(std::pair{MfdOption::Daylight,static_cast<DWORD>(Checked(Daylight))});break;
        case Zone2: case Zone3: case DateFormat: {
            const auto value=SendMessageW(Item(id),CB_GETCURSEL,0,0);
            if(value!=CB_ERR)StartMfd(std::pair{id==Zone2?MfdOption::Zone2:id==Zone3?MfdOption::Zone3:MfdOption::DateFormat,static_cast<DWORD>(value)});break;
        }
        case PowerRefresh: powerResult.clear(); RefreshPower(); break;
        case PowerDisable: ChangePower(false); break;
        case PowerRestore: ChangePower(true); break;
        case MfdRefresh: StartMfd(); break;
        case MfdClutch: StartMfd({{MfdOption::Clutch, Checked(id)}}); break;
        case MfdLatched: StartMfd({{MfdOption::Latched, Checked(id)}}); break;
        case MfdLight: StartMfd({{MfdOption::MfdBrightness, Checked(id) ? 100u : 0u}}); break;
        case LedLight: pendingLed.reset(); StartMfd({{MfdOption::LedBrightness, Checked(id) ? rememberedLed : 0}}); break;
        case ClockSelect: ShowClockFormat(); break;
        case ClockFormat: {
            const auto clock = SendMessageW(Item(ClockSelect), CB_GETCURSEL, 0, 0);
            const auto format = SendMessageW(Item(ClockFormat), CB_GETCURSEL, 0, 0);
            if (clock >= 0 && clock < 3 && format >= 0 && format < 2)
                StartMfd({{static_cast<MfdOption>(static_cast<int>(MfdOption::Clock1) + clock), static_cast<DWORD>(format)}});
            break;
        }
        case FilterApply: {
            x52::Command command{CommandType::ConfigureFilters};
            for (int i = 0; i < 4; ++i) command.filters.enabled[static_cast<std::size_t>(i)] = Checked(FilterThrottle + i);
            command.filters.smoothingMs = Numeric(Item(FilterTime)); command.filters.jitterCounts = Numeric(Item(FilterJitter));
            command.filters.Validate(); service.Send(std::move(command)); break;
        }
        case Refresh: service.Send({CommandType::Rescan}); break;
        case Reinitialize: service.Send({CommandType::Reinitialize}); break;
        case Export: service.Send({CommandType::Export}); break;
        case Folder:
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", service.directory().c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                throw std::runtime_error("Unable to open data folder");
            break;
        case Learn: service.Send({CommandType::StartLearn}); break;
        case SaveLearn:
            ShowControlPicker(window, instance, service, Utf8(Text(Item(Candidate)))); Poll(); break;
        case IdentifySelected: IdentifyRow(); break;
        case ImportBF3: LoadBattlefield(3); break;
        case ImportBF4: LoadBattlefield(4); break;
        case ProfileContext: FilterProfileActions(); break;
        case AddProfileMapping: AssignProfile(); break;
        case RemoveProfileMapping: {
            const auto index = SendMessageW(Item(ProfileMappingList), LB_GETCURSEL, 0, 0);
            if (index < 0 || static_cast<std::size_t>(index) >= profileMappings.size()) throw std::runtime_error("Select a profile override to remove");
            profileMappings.erase(profileMappings.begin() + index); SaveProfileDraft(); RefreshProfileMappings(); break;
        }
        case ExportPr0: ExportProfile(); break;
        case CaptureStart: service.Send({CommandType::StartCapture, Utf8(Text(Item(CaptureLabel)))}); break;
        case CaptureStop: service.Send({CommandType::StopCapture}); break;
        case Dropout: service.Send({CommandType::MarkDropout, "Physical stick dropout observed by user"}); break;
        case Returned: service.Send({CommandType::MarkReturn, "Physical stick return observed by user"}); break;
        case Apply: {
            x52::Command command{CommandType::Configure};
            const auto validation = Numeric(Item(Validation));
            if (validation < 2 || validation > 100 || std::floor(validation) != validation)
                throw std::runtime_error("Validation reports must be a whole number from 2 to 100");
            command.settings.validationReports = static_cast<unsigned>(validation);
            command.settings.blendMs = Numeric(Item(Blend));
            command.settings.staleMs = Numeric(Item(Stale));
            command.settings.behaviour = SendMessageW(Item(Hold), CB_GETCURSEL, 0, 0) == 1 ? DropoutBehaviour::HoldLast : DropoutBehaviour::Neutralise;
            service.Send(std::move(command)); break;
        }
        }
    }
    void FillCombo(int id, const std::vector<std::wstring>& values)
    {
        SendMessageW(Item(id), CB_RESETCONTENT, 0, 0);
        for (const auto& value : values) if (SendMessageW(Item(id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str())) < 0)
            throw std::runtime_error("Cannot populate profile selector");
        SendMessageW(Item(id), CB_SETCURSEL, 0, 0);
    }
    std::filesystem::path ProfileDraftPath() const { return service.directory() / (L"bf" + std::to_wstring(profileGame) + L"-joystick-authoring.json"); }
    void LoadBattlefield(int game)
    {
        for (const auto item : {ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), FALSE);
        SetText(Item(ProfileInfo), L"Importing saved joystick / joypad bindings...");
        auto bindings = JoystickBindings(ReadBattlefieldBindings(BattlefieldSettingsPath(game)));
        if (bindings.empty()) throw std::runtime_error("No joystick / joypad bindings were found in this game's saved settings");
        std::vector<ProfileButton> controls;
        for (const auto& [id, learned] : service.Read().assignments)
            controls.push_back({id, learned.name + " [" + id + "]"});
        // Game settings are read-only. Physical inputs come from the user's HID links.
        profileGame = game; battlefieldBindings = std::move(bindings); profileButtons = std::move(controls);
        profileMappings.clear();
        unsigned unresolved{};
        if (std::filesystem::exists(ProfileDraftPath())) {
            if (std::filesystem::file_size(ProfileDraftPath()) > 1024 * 1024) throw std::runtime_error("Saved profile draft exceeds 1 MiB");
            std::ifstream stream(ProfileDraftPath());
            const auto draft = Json::parse(stream, [](int depth, Json::parse_event_t, Json&) {
                if (depth > 16) throw std::runtime_error("Profile draft is too deeply nested");
                return true;
            });
            if (draft.at("schema") != 1 || draft.at("game") != game || draft.at("input_source") != "battlefield_joystick")
                throw std::runtime_error("Invalid joystick plan schema/game/source");
            std::set<std::pair<int, std::string>> used;
            for (const auto& item : draft.at("mappings")) {
                const auto mode = item.at("mode").get<int>(); const auto control = item.at("control").get<std::string>();
                const auto bindingId = item.at("binding").get<std::string>();
                const auto binding = std::find_if(battlefieldBindings.begin(), battlefieldBindings.end(), [&](const auto& value) { return value.id == bindingId; });
                const auto button = std::find_if(profileButtons.begin(), profileButtons.end(), [&](const auto& value) { return value.id == control; });
                if (mode < 0 || mode >= 6 || button == profileButtons.end() || binding == battlefieldBindings.end() ||
                    (JoystickKind(*binding) != JoystickBindingKind::Button && JoystickKind(*binding) != JoystickBindingKind::Axis) ||
                    !used.emplace(mode, control).second) { ++unresolved; continue; }
                profileMappings.push_back({mode, control, *binding});
            }
        }
        std::vector<std::wstring> names;
        for (const auto& button : profileButtons) names.push_back(Wide(button.name));
        FillCombo(ProfileControl, names);
        std::set<std::string> contexts;
        for (const auto& binding : battlefieldBindings) contexts.insert(binding.context);
        names = {L"All contexts"}; for (const auto& context : contexts) names.push_back(Wide(context));
        FillCombo(ProfileContext, names); FilterProfileActions(); RefreshProfileMappings();
        for (const auto item : {ProfileMode, ProfileContext, ProfileActionChoice}) EnableWindow(Item(item), TRUE);
        for (const auto item : {ProfileControl, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), !profileButtons.empty());
        const auto axes = std::count_if(battlefieldBindings.begin(), battlefieldBindings.end(), [](const auto& binding) { return JoystickKind(binding) == JoystickBindingKind::Axis; });
        const auto unassigned = std::count_if(battlefieldBindings.begin(), battlefieldBindings.end(), [](const auto& binding) { return JoystickKind(binding) == JoystickBindingKind::Unassigned; });
        SetText(Item(ProfileInfo), L"BF" + std::to_wstring(game) + L": imported " + std::to_wstring(battlefieldBindings.size()) +
            L" joystick records (" + std::to_wstring(axes) + L" axes, " + std::to_wstring(unassigned) + L" unassigned). " +
            std::to_wstring(profileButtons.size()) + L" identified X52 inputs.\r\n" +
            (unresolved ? std::to_wstring(unresolved) + L" saved overrides need reassignment. " : L"") +
            (profileButtons.empty() ? L"Identify physical controls on Live inputs, then reimport. " : L"") +
            L"Plan only: joystick output / PR0 joystick export are not implemented. Codes are Battlefield IDs, not HID button numbers.");
    }
    void FilterProfileActions()
    {
        const auto selected = Utf8(Text(Item(ProfileContext)));
        actionChoices.clear(); std::vector<std::wstring> labels;
        for (const auto& binding : battlefieldBindings) if (binding.type == 2 && (selected == "All contexts" || binding.context == selected)) {
            actionChoices.push_back(binding); labels.push_back(BindingLabel(binding));
        }
        FillCombo(ProfileActionChoice, labels);
    }
    void SaveProfileDraft()
    {
        WriteJson(ProfileDraftPath(), BuildJoystickPlan(profileGame, profileMappings));
    }
    void RefreshProfileMappings()
    {
        SendMessageW(Item(ProfileMappingList), LB_RESETCONTENT, 0, 0);
        for (const auto& mapping : profileMappings) {
            const auto button = std::find_if(profileButtons.begin(), profileButtons.end(), [&](const auto& value) { return value.id == mapping.control; });
            const auto label = L"Mode " + std::to_wstring(mapping.mode % 3 + 1) + (mapping.mode >= 3 ? L" + Pinkie" : L"") +
                L" / " + Wide(button->name) + L" -> " + BindingLabel(mapping.binding);
            SendMessageW(Item(ProfileMappingList), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
    }
    void AssignProfile()
    {
        const auto mode = static_cast<int>(SendMessageW(Item(ProfileMode), CB_GETCURSEL, 0, 0));
        const auto button = SendMessageW(Item(ProfileControl), CB_GETCURSEL, 0, 0);
        const auto action = SendMessageW(Item(ProfileActionChoice), CB_GETCURSEL, 0, 0);
        if (mode < 0 || button < 0 || action < 0 || static_cast<std::size_t>(button) >= profileButtons.size() || static_cast<std::size_t>(action) >= actionChoices.size())
            throw std::runtime_error("Select a mode, identified X52 control and Battlefield joystick binding");
        const auto kind = JoystickKind(actionChoices[static_cast<std::size_t>(action)]);
        if (kind != JoystickBindingKind::Axis && kind != JoystickBindingKind::Button)
            throw std::runtime_error("This joystick entry is unassigned or has an unknown encoding. Choose an assigned button or axis");
        const auto& control = profileButtons[static_cast<std::size_t>(button)].id;
        std::erase_if(profileMappings, [&](const auto& row) { return row.mode == mode && row.control == control; });
        profileMappings.push_back({mode, control, actionChoices[static_cast<std::size_t>(action)]});
        SaveProfileDraft(); RefreshProfileMappings();
    }
    void ExportProfile()
    {
        if (profileMappings.empty()) throw std::runtime_error("Assign at least one joystick mapping before exporting");
        const auto directory = service.directory() / L"profiles"; std::filesystem::create_directories(directory);
        const auto file = directory / (L"BF" + std::to_wstring(profileGame) + L"-X52-joystick-" + std::to_wstring(Qpc()) + L".json");
        WriteJson(file, BuildJoystickPlan(profileGame, profileMappings));
        SetText(Item(ProfileInfo), L"Exported joystick mapping plan: " + file.wstring() +
            L"\r\nThis JSON preserves Battlefield's joystick bindings. It is not an active controller profile or a Logitech PR0 file.");
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
            throw std::runtime_error("Profile saved, but its folder could not be opened");
    }
    void IdentifyRow()
    {
        const auto index = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        if (index < 0 || static_cast<std::size_t>(index) >= rows.size()) {
            SetText(footer, L"Select a row on Live inputs, then choose Identify selected input."); return;
        }
        ShowControlPicker(window, instance, service, rows[static_cast<std::size_t>(index)]);
        Poll();
    }
};
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<Application*>(create->lpCreateParams);
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)) && GetLastError()) return FALSE;
        app->window = window; app->instance = create->hInstance;
    }
    try {
        if (app) switch (message) {
        case WM_CREATE: app->Create(); return 0;
        case WM_SIZE: app->Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize = {MulDiv(1298, app->dpi, 96), MulDiv(795, app->dpi, 96)}; return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wParam); app->Fonts();
            const auto* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            app->Layout(); return 0;
        }
        case WM_TIMER:
            if (wParam == 1) app->Poll();
            else if (wParam == 2) app->PollMfd();
            return 0;
        case WM_PAINT: app->theme.Paint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PRINTCLIENT: app->theme.Background(reinterpret_cast<HDC>(wParam), window); return 0;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
            return app->theme.Color(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam), message);
        case WM_MEASUREITEM: reinterpret_cast<MEASUREITEMSTRUCT*>(lParam)->itemHeight = MulDiv(28, app->dpi, 96); return TRUE;
        case WM_DRAWITEM: if (app->theme.Draw(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam))) return TRUE; break;
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) == app->Item(LedLevel)) app->LedSlider();
            return 0;
        case WM_COMMAND:
            app->theme.Command(wParam, lParam);
            if ((LOWORD(wParam)==DeadzoneView && HIWORD(wParam)==1) || HIWORD(wParam) == BN_CLICKED || ((LOWORD(wParam) == ProfileContext || LOWORD(wParam) == ClockSelect || LOWORD(wParam) == ClockFormat || LOWORD(wParam)==Zone2 || LOWORD(wParam)==Zone3 || LOWORD(wParam)==DateFormat) && HIWORD(wParam) == CBN_SELCHANGE)) app->Command(LOWORD(wParam)); return 0;
        case WM_NOTIFY:
            if (const auto result = app->theme.Notify(reinterpret_cast<NMHDR*>(lParam))) return *result;
            if (reinterpret_cast<NMHDR*>(lParam)->idFrom == ControlList) {
                const auto code = reinterpret_cast<NMHDR*>(lParam)->code;
                if (code == NM_DBLCLK || (code == LVN_KEYDOWN && reinterpret_cast<NMLVKEYDOWN*>(lParam)->wVKey == VK_RETURN)) app->IdentifyRow();
            }
            if (reinterpret_cast<NMHDR*>(lParam)->idFrom == Tabs && reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE) {
                app->page = TabCtrl_GetCurSel(app->tabs); app->Layout(); app->Poll();
            }
            return 0;
        case WM_DEVICECHANGE:
            if ((wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) && lParam) {
                const auto* header = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
                if (header->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                    const auto* data = reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE_W*>(lParam);
                    x52::Command command{CommandType::DeviceEvent};
                    command.text = Utf8(data->dbcc_name); command.removed = wParam == DBT_DEVICEREMOVECOMPLETE;
                    app->service.Send(std::move(command));
                }
            }
            return TRUE;
        case WM_DESTROY:
            KillTimer(window, 1); KillTimer(window, 2); app->window = nullptr; PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& error) {
        MessageBoxW(window, Wide(error.what()).c_str(), L"X52 Inspector", MB_OK | MB_ICONERROR);
        if (message == WM_CREATE) return -1;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    try {
        int count{};
        struct Arguments { LPWSTR* value{}; ~Arguments() { if (value) LocalFree(value); } } args{CommandLineToArgvW(GetCommandLineW(), &count)};
        if (!args.value) throw WindowsException("Read command line", GetLastError());
        if (count > 1) {
            const std::wstring_view operation(args.value[1]);
            if (count != 3 || (operation != L"--x52-power-disable" && operation != L"--x52-power-restore"))
                throw std::runtime_error("Unsupported command line");
            ChangeX52Power(DefaultDataDirectory(), args.value[2], operation == L"--x52-power-restore");
            return 0;
        }
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES};
        if (!InitCommonControlsEx(&controls)) throw WindowsException("InitCommonControlsEx", GetLastError());
        Application app;
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass); windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = instance; windowClass.lpszClassName = WindowClass;
        windowClass.hbrBackground = nullptr;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&windowClass)) throw WindowsException("RegisterClassExW", GetLastError());
        const auto window = CreateWindowExW(WS_EX_CONTROLPARENT, WindowClass,
            L"X52 Battlefield Mapper", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 1440, 960, nullptr, nullptr, instance, &app);
        if (!window) throw WindowsException("CreateWindowExW", GetLastError());
        ShowWindow(window, show);
        MSG message{}; BOOL status{};
        while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        const auto messageError = status == -1 ? GetLastError() : ERROR_SUCCESS;
        if (app.window) DestroyWindow(app.window);
        if (messageError != ERROR_SUCCESS) throw WindowsException("GetMessageW", messageError);
        return static_cast<int>(message.wParam);
    } catch (const std::exception& error) {
        MessageBoxW(nullptr, Wide(error.what()).c_str(), L"X52 Battlefield Mapper", MB_OK | MB_ICONERROR);
        return 1;
    }
}
