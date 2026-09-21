#include "app/InspectorService.hpp"
#include "util/Text.hpp"
#include "ui/ControlPicker.hpp"
#include "profiles/BattlefieldProfiles.hpp"
#include <commctrl.h>
#include <dbt.h>
#include <shellapi.h>
#include <sstream>
#include <iomanip>
#include <array>
#include <set>

namespace {
using namespace x52;
constexpr wchar_t WindowClass[] = L"X52BattlefieldMapper.Inspector";
enum : int { Refresh = 101, Reinitialize, Export, Folder, Learn, SaveLearn, CaptureStart,
    CaptureStop, Dropout, Returned, Apply, ControlList, Tabs, Candidate, Name, Group, Neutral,
    Hold, Blend, Validation, Stale, CaptureLabel, IdentifySelected,
    ImportBF3, ImportBF4, ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice,
    AddProfileMapping, RemoveProfileMapping, ExportPr0, ProfileMappingList, ProfileInfo };
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
double Numeric(HWND control)
{
    const auto text = Text(control);
    std::size_t parsed{};
    const auto value = std::stod(text, &parsed);
    if (parsed != text.size() || !std::isfinite(value)) throw std::runtime_error("Enter a finite number without trailing text");
    return value;
}
struct Application {
    InspectorService service{DefaultDataDirectory()};
    HWND window{}, title{}, subtitle{}, status{}, tabs{}, list{}, raw{}, learnInfo{}, health{}, inventory{}, footer{};
    HINSTANCE instance{};
    HFONT normal{}, heading{}, mono{};
    HDEVNOTIFY notification{};
    std::vector<Child> children;
    std::vector<std::string> rows, candidates;
    std::vector<std::uint8_t> previousDisplayed;
    std::vector<BitChange> recentChanges;
    ULONGLONG changesUntil{};
    Snapshot last;
    int profileGame{};
    Pr0Node profileTemplate;
    std::vector<BattlefieldBinding> battlefieldBindings, actionChoices;
    std::vector<ProfileButton> profileButtons;
    std::vector<ProfileMapping> profileMappings;
    int page{}, dpi{96};
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
        const auto handle = CreateWindowExW(isEdit ? WS_EX_CLIENTEDGE : 0, cls, text,
            WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        if (!handle) throw WindowsException("CreateWindowExW (control)", GetLastError());
        children.push_back({handle, targetPage, x, y, w, h, sx, sy});
        if (isEdit) SendMessageW(handle, EM_SETLIMITTEXT, 4 * 1024 * 1024, 0);
        return handle;
    }
    void Fonts()
    {
        for (auto font : {normal, heading, mono}) if (font) DeleteObject(font);
        normal = CreateFontW(-MulDiv(14, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        heading = CreateFontW(-MulDiv(26, dpi, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
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
        title = Add(L"STATIC", L"X52 Battlefield Mapper", 0, -1, 24, 18, 650, 38);
        subtitle = Add(L"STATIC", L"X52 INSPECTOR + BATTLEFIELD PROFILE AUTHORING   /   Logitech activates exported profiles", 0, -1, 26, 62, 960, 23, 0, true);
        status = Add(L"STATIC", L"Discovering Saitek X52...", 0, -1, 26, 94, 960, 23, 0, true);
        Add(L"BUTTON", L"Reinitialize X52 input", Reinitialize, -1, 24, 129, 190, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Refresh inventory", Refresh, -1, 226, 129, 156, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Export diagnostic", Export, -1, 394, 129, 160, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Open data folder", Folder, -1, 566, 129, 152, 32, WS_TABSTOP);
        Add(L"BUTTON", L"Identify selected input...", IdentifySelected, -1, 730, 129, 246, 32, WS_TABSTOP);
        tabs = Add(WC_TABCONTROLW, L"", Tabs, -1, 24, 177, 952, 30, WS_TABSTOP, true);
        int i = 0;
        for (const auto label : {L"Live inputs", L"Learn controls", L"Connection health", L"HID inventory", L"Battlefield profiles"}) {
            TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(label);
            if (TabCtrl_InsertItem(tabs, i++, &item) == -1) throw std::runtime_error("Unable to create tab");
        }
        Add(L"STATIC", L"Double-click a HID row (or press Enter) to link it to a physical X52 button, hat, switch or axis.", 0, 0, 24, 220, 952, 24, 0, true);
        list = Add(WC_LISTVIEWW, L"", ControlList, 0, 24, 254, 952, 284,
            WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, true, true);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        i = 0;
        for (const auto& column : std::array<std::pair<const wchar_t*, int>, 7>{{
            {L"HID usage / report / link", 185}, {L"Physical control link", 260}, {L"Group", 80},
            {L"Raw", 65}, {L"Range", 92}, {L"Normalized", 105}, {L"Safe internal", 145}}}) {
            LVCOLUMNW item{}; item.mask = LVCF_TEXT | LVCF_WIDTH; item.pszText = const_cast<wchar_t*>(column.first);
            item.cx = MulDiv(column.second, dpi, 96);
            if (ListView_InsertColumn(list, i++, &item) == -1) throw std::runtime_error("Unable to create list column");
        }
        raw = Add(L"EDIT", L"Waiting for reports", 0, 0, 24, 554, 952, 120,
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
        Add(L"BUTTON", L"Start disconnect capture", CaptureStart, 2, 24, 263, 198, 32, WS_TABSTOP);
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
        Add(L"STATIC", L"Reads your Documents settings; keeps analogue axes in the game.", 0, 4, 415, 224, 560, 25, 0, true);
        Add(L"STATIC", L"X52 mode", 0, 4, 24, 260, 210, 23);
        Add(L"COMBOBOX", L"", ProfileMode, 4, 24, 283, 270, 220, CBS_DROPDOWNLIST | WS_TABSTOP);
        FillCombo(ProfileMode, {L"Mode 1 (base)", L"Mode 2 (inherits Mode 1)", L"Mode 3 (inherits Mode 1)",
            L"Mode 1 + Pinkie", L"Mode 2 + Pinkie", L"Mode 3 + Pinkie"});
        Add(L"STATIC", L"Physical button (manufacturer names)", 0, 4, 310, 260, 650, 23);
        Add(L"COMBOBOX", L"", ProfileControl, 4, 310, 283, 666, 300, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, true);
        Add(L"STATIC", L"Game context", 0, 4, 24, 320, 155, 23);
        Add(L"COMBOBOX", L"", ProfileContext, 4, 24, 344, 155, 220, CBS_DROPDOWNLIST | WS_TABSTOP);
        Add(L"STATIC", L"Existing Battlefield command / key", 0, 4, 191, 320, 570, 23);
        Add(L"COMBOBOX", L"", ProfileActionChoice, 4, 191, 344, 570, 360, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        Add(L"BUTTON", L"Assign / replace", AddProfileMapping, 4, 777, 341, 199, 30, WS_TABSTOP);
        Add(L"LISTBOX", L"", ProfileMappingList, 4, 24, 391, 952, 173,
            LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, true);
        SendMessageW(Item(ProfileMappingList), LB_SETHORIZONTALEXTENT, MulDiv(1350, dpi, 96), 0);
        Add(L"BUTTON", L"Remove selected override", RemoveProfileMapping, 4, 24, 576, 230, 30, WS_TABSTOP);
        Add(L"BUTTON", L"Export .pr0 draft", ExportPr0, 4, 752, 576, 224, 30, WS_TABSTOP);
        Add(L"STATIC", L"Import a game, select a mode/button and an existing command. Hats/axis programming is not exported yet.\r\nUnassigned controls retain Logitech defaults; removing an override restores mode inheritance.", ProfileInfo, 4, 24, 619, 952, 63, 0, true);
        for (const auto item : {ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), FALSE);
        footer = Add(L"STATIC", L"", 0, -1, 24, 694, 952, 40, 0, true);
        Fonts();
        DEV_BROADCAST_DEVICEINTERFACE_W filter{};
        filter.dbcc_size = sizeof(filter); filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        HidD_GetHidGuid(&filter.dbcc_classguid);
        notification = RegisterDeviceNotificationW(window, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
        if (!notification) throw WindowsException("RegisterDeviceNotificationW", GetLastError());
        if (!SetTimer(window, 1, 100, nullptr)) throw WindowsException("SetTimer", GetLastError());
        service.Start();
        Layout();
    }
    void Layout()
    {
        RECT rect{}; GetClientRect(window, &rect);
        const auto extraX = MulDiv(rect.right, 96, dpi) - 1000;
        const auto extraY = MulDiv(rect.bottom, 96, dpi) - 750;
        for (const auto& child : children) {
            auto y = child.y;
            if (child.handle == raw || child.handle == footer) y += extraY;
            if (!MoveWindow(child.handle, MulDiv(child.x, dpi, 96), MulDiv(y, dpi, 96),
                MulDiv(std::max(1, child.w + (child.stretchX ? extraX : 0)), dpi, 96),
                MulDiv(std::max(1, child.h + (child.stretchY ? extraY : 0)), dpi, 96), TRUE))
                throw WindowsException("MoveWindow", GetLastError());
            ShowWindow(child.handle, child.page == -1 || child.page == page ? SW_SHOW : SW_HIDE);
        }
    }
    void Poll()
    {
        last = service.Read();
        SetText(status, (last.connected ? L"CONNECTED   |   " : L"NOT CONNECTED   |   ") + last.device +
            L"   |   Reports: " + std::to_wstring(last.sequence));
        SetText(footer, Wide(last.error.empty() ? last.status : "ERROR: " + last.error));
        EnableWindow(Item(Learn), last.connected && last.sequence > 0);
        EnableWindow(Item(SaveLearn), last.learning && !last.learnCandidates.empty());
        EnableWindow(Item(CaptureStart), !last.captureActive);
        EnableWindow(Item(Returned), last.recovery.stickSuspended);
        if (page == 0) {
            std::vector<std::string> keys;
            for (const auto& [id, control] : last.physical.controls) { (void)control; keys.push_back(id); }
            if (keys != rows) {
                rows = keys; ListView_DeleteAllItems(list);
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
                const auto set = [&](int column, std::wstring value) { ListView_SetItemText(list, static_cast<int>(i), column, value.data()); };
                set(1, assignment == last.assignments.end() ? L"Unassigned" : Wide(assignment->second.name));
                set(2, assignment == last.assignments.end() || assignment->second.group == InputGroup::Unknown ? L"Unknown" :
                    assignment->second.group == InputGroup::Stick ? L"Stick" : L"Throttle");
                set(3, std::to_wstring(control.raw));
                set(4, std::to_wstring(control.minimum) + L".." + std::to_wstring(control.maximum));
                set(5, control.kind == ControlKind::Button ? (control.raw ? L"PRESSED" : L"released") : Number(control.normalized));
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
            text << L"USB handle: " << (last.connected ? L"OPEN" : L"CLOSED")
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
                << L"\r\nLearn stick/throttle groups before assessing activity. Unassigned controls are withheld on marked dropout."
                << L"\r\nReinitialize restarts Windows reads; it does not electrically reset the joystick."
                << L"\r\nVirtual controller: not implemented in this milestone.\r\nData: " << service.directory().wstring();
            SetText(health, text.str());
        } else SetText(inventory, last.inventory);
    }
    void Command(int id)
    {
        switch (id) {
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
    std::filesystem::path ProfileDraftPath() const { return service.directory() / (L"bf" + std::to_wstring(profileGame) + L"-authoring.json"); }
    void LoadBattlefield(int game)
    {
        for (const auto item : {ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), FALSE);
        SetText(Item(ProfileInfo), L"Importing saved bindings and the installed X52 template...");
        auto bindings = ReadBattlefieldBindings(BattlefieldSettingsPath(game));
        auto base = ReadX52Template(InstalledX52TemplatePath());
        auto buttons = ProfileButtons(base);
        // Read-only imports: game settings and vendor template are never modified.
        profileGame = game; battlefieldBindings = std::move(bindings); profileTemplate = std::move(base); profileButtons = std::move(buttons);
        profileMappings.clear();
        unsigned unresolved{};
        if (std::filesystem::exists(ProfileDraftPath())) {
            if (std::filesystem::file_size(ProfileDraftPath()) > 1024 * 1024) throw std::runtime_error("Saved profile draft exceeds 1 MiB");
            std::ifstream stream(ProfileDraftPath());
            const auto draft = Json::parse(stream, [](int depth, Json::parse_event_t, Json&) {
                if (depth > 16) throw std::runtime_error("Profile draft is too deeply nested");
                return true;
            });
            if (draft.at("schema") != 1 || draft.at("game") != game) throw std::runtime_error("Invalid profile draft schema/game");
            std::set<std::pair<int, std::string>> used;
            for (const auto& item : draft.at("mappings")) {
                const auto mode = item.at("mode").get<int>(); const auto control = item.at("control").get<std::string>();
                const auto bindingId = item.at("binding").get<std::string>();
                const auto binding = std::find_if(battlefieldBindings.begin(), battlefieldBindings.end(), [&](const auto& value) { return value.id == bindingId; });
                const auto button = std::find_if(profileButtons.begin(), profileButtons.end(), [&](const auto& value) { return value.id == control; });
                if (mode < 0 || mode >= 6 || button == profileButtons.end() || binding == battlefieldBindings.end() || !ProfileOutput(*binding) || !used.emplace(mode, control).second) { ++unresolved; continue; }
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
        for (const auto item : {ProfileMode, ProfileControl, ProfileContext, ProfileActionChoice, AddProfileMapping, RemoveProfileMapping, ExportPr0}) EnableWindow(Item(item), TRUE);
        SetText(Item(ProfileInfo), L"BF" + std::to_wstring(game) + L": imported " + std::to_wstring(battlefieldBindings.size()) +
            L" binding records. Export supports keyboard / left-right mouse commands on buttons. Axes stay in Battlefield.\r\n" +
            (unresolved ? std::to_wstring(unresolved) + L" saved overrides need reassignment. " : L"") +
            L"Pinkie/clutch remain reserved; hats await a matching profiler sample. Mode inheritance follows Logitech defaults.");
    }
    void FilterProfileActions()
    {
        const auto selected = Utf8(Text(Item(ProfileContext)));
        actionChoices.clear(); std::vector<std::wstring> labels;
        for (const auto& binding : battlefieldBindings) if (ProfileOutput(binding) && (selected == "All contexts" || binding.context == selected)) {
            actionChoices.push_back(binding); labels.push_back(BindingLabel(binding));
        }
        FillCombo(ProfileActionChoice, labels);
    }
    void SaveProfileDraft()
    {
        Json overrides = Json::array();
        for (const auto& mapping : profileMappings) overrides.push_back({{"mode", mapping.mode}, {"control", mapping.control}, {"binding", mapping.binding.id}});
        WriteJson(ProfileDraftPath(), {{"schema", 1}, {"game", profileGame}, {"mappings", overrides}});
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
            throw std::runtime_error("Select a mode, physical button and Battlefield command");
        const auto& control = profileButtons[static_cast<std::size_t>(button)].id;
        std::erase_if(profileMappings, [&](const auto& row) { return row.mode == mode && row.control == control; });
        profileMappings.push_back({mode, control, actionChoices[static_cast<std::size_t>(action)]});
        SaveProfileDraft(); RefreshProfileMappings();
    }
    void ExportProfile()
    {
        const auto name = "Battlefield " + std::to_string(profileGame) + " X52 draft";
        const auto built = BuildBattlefieldPr0(profileTemplate, profileMappings, name);
        (void)ParsePr0(SerializePr0(built));
        const auto text = EncodePr0(built);
        const auto directory = service.directory() / L"profiles"; std::filesystem::create_directories(directory);
        const auto file = directory / (L"BF" + std::to_wstring(profileGame) + L"-X52-" + std::to_wstring(Qpc()) + L".pr0");
        const auto temporary = file.wstring() + L".tmp";
        { std::ofstream stream(temporary, std::ios::binary); stream.write(text.data(), static_cast<std::streamsize>(text.size())); stream.close();
          if (!stream) throw std::runtime_error("Could not write profile draft"); }
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_WRITE_THROUGH)) throw WindowsException("Publish profile draft", GetLastError());
        SetText(Item(ProfileInfo), L"Exported: " + file.wstring() + L"\r\nOpen this draft in Logitech's profiler and test it before activating. It has not been loaded or written to joystick memory.");
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
            info->ptMinTrackSize = {MulDiv(1018, app->dpi, 96), MulDiv(795, app->dpi, 96)}; return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wParam); app->Fonts();
            const auto* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            app->Layout(); return 0;
        }
        case WM_TIMER: if (wParam == 1) app->Poll(); return 0;
        case WM_CTLCOLORSTATIC:
            SetBkColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_WINDOW));
            SetTextColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_WINDOWTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED || (LOWORD(wParam) == ProfileContext && HIWORD(wParam) == CBN_SELCHANGE)) app->Command(LOWORD(wParam)); return 0;
        case WM_NOTIFY:
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
            KillTimer(window, 1); app->window = nullptr; PostQuitMessage(0); return 0;
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
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
        if (!InitCommonControlsEx(&controls)) throw WindowsException("InitCommonControlsEx", GetLastError());
        Application app;
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass); windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = instance; windowClass.lpszClassName = WindowClass;
        windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&windowClass)) throw WindowsException("RegisterClassExW", GetLastError());
        const auto window = CreateWindowExW(WS_EX_CONTROLPARENT, WindowClass,
            L"X52 Battlefield Mapper - PS28 Input Inspector", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1180, 900, nullptr, nullptr, instance, &app);
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
