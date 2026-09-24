#include "DeadzoneSettings.hpp"
#include "../util/Text.hpp"
#include <shlobj.h>
#include <set>

namespace x52 {
namespace {
unsigned Integer(const std::string& text)
{
    std::size_t used{}; const auto value = std::stoul(text, &used, 0);
    if (used != text.size() || value > 0xfffffffful) throw std::runtime_error("Invalid calibration number");
    return static_cast<unsigned>(value);
}
const Pr0Node& Only(const Pr0Node& parent, const char* name)
{
    if (parent.children.size() != 1 || parent.children[0].tag != name) throw std::runtime_error("Unsupported calibration structure");
    return parent.children[0];
}
const Pr0Node& Controls(const Pr0Node& root)
{
    if (root.tag != "profile" || !root.value.empty() || root.attributes.size() != 1 || Integer(root.attributes.at("version")) != 0x01000001)
        throw std::runtime_error("Expected a calibration-only profile; command profiles cannot be edited here");
    const auto& controllers = Only(root,"controllers");
    const auto& controller = Only(controllers,"controller");
    if (controller.value != "e81d998b-c604-4d71-be97-35ca01439c7e" || !controller.attributes.empty() || controller.children.size() != 2 ||
        controller.children[0].tag != "member" || controller.children[0].value != "c7719f41-f667-4514-bbb4-3f38c9e4d05a" ||
        !controller.children[0].children.empty() || !controller.children[0].attributes.empty() || controller.children[1].tag != "controls")
        throw std::runtime_error("Expected the original X52 calibration controller");
    if (!controllers.attributes.empty() || !controllers.value.empty() || !controller.children[1].value.empty() || !controller.children[1].attributes.empty())
        throw std::runtime_error("Unsupported calibration metadata");
    return controller.children[1];
}
std::string Bytes(const std::filesystem::path& file)
{
    if (std::filesystem::file_size(file) > 64*1024) throw std::runtime_error("Calibration file exceeds 64 KiB");
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read calibration file");
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (input.bad() || text.size() > 64*1024) throw std::runtime_error("Calibration read failed");
    return text;
}
Pr0Node Parse(const std::string& bytes)
{
    if (!bytes.starts_with("\xff\xfe")) return ParsePr0(bytes);
    if (bytes.size()%2) throw std::runtime_error("Truncated UTF-16 calibration");
    std::wstring text;
    for (std::size_t i=2;i<bytes.size();i+=2) text.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) | (static_cast<unsigned char>(bytes[i+1])<<8)));
    return ParsePr0(Utf8(text));
}
void Write(const std::filesystem::path& file, const std::string& bytes)
{
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); output.close();
    if (!output) throw std::runtime_error("Cannot save calibration file");
}
void Replace(const std::filesystem::path& file, const std::string& bytes)
{
    const auto temporary = file.wstring() + L".x52mapper-" + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    Write(temporary, bytes);
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error=GetLastError(); DeleteFileW(temporary.c_str()); throw WindowsException("Replace calibration file",error);
    }
}
void ValidateFile(const std::filesystem::path& file)
{
    if(file.empty())throw std::runtime_error("The X52 driver reports no active calibration file. Open the Logitech X52 Properties / Deadzones page, then Refresh here. No settings were changed.");
    struct Folder { PWSTR value{}; ~Folder(){ CoTaskMemFree(value); } } folder;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData,0,nullptr,&folder.value))) throw std::runtime_error("Cannot locate Logitech calibration folder");
    const auto expected = std::filesystem::path(folder.value)/L"SmartTechnology"/L"Cpls";
    if (!file.is_absolute() || std::filesystem::weakly_canonical(file.parent_path()) != std::filesystem::weakly_canonical(expected) ||
        !file.filename().wstring().starts_with(L"SaiC075C-") || file.extension() != L".pr0")
        throw std::runtime_error("Unsupported calibration location; use the installed X52 Properties panel first");
    const auto attributes=GetFileAttributesW(file.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES)throw WindowsException("Read saved X52 calibration attributes",GetLastError());
    if(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("Expected a regular saved X52 calibration file");
}
}
void ValidateDeadzone(const AxisDeadzone& axis)
{
    const auto& a=axis.limits;
    if (a[0]<0 || a[3]>65535 || a[0]>=a[1] || a[1]>a[2] || a[2]>=a[3])
        throw std::runtime_error("Keep active travel on both sides: minimum < centre low <= centre high < maximum");
}
std::array<AxisDeadzone,9> DecodeDeadzones(const Pr0Node& root)
{
    const auto& controls=Controls(root); std::array<AxisDeadzone,9> result{}; std::set<DWORD> seen;
    if (controls.children.size()!=9) throw std::runtime_error("Expected all nine X52 calibration axes");
    for (const auto& node:controls.children) {
        const auto id=Integer(node.value); const auto it=std::find(DeadzoneAxes.begin(),DeadzoneAxes.end(),id);
        if (node.tag!="axis" || it==DeadzoneAxes.end() || !seen.insert(id).second || node.attributes!=std::map<std::string,std::string>{{"envelope","envelope"}})
            throw std::runtime_error("Unsupported or duplicate calibration axis");
        const auto& envelope=Only(node,"envelope");
        if (!envelope.children.empty() || !envelope.value.empty()) throw std::runtime_error("Unsupported calibration curve");
        const std::set<std::string> allowed{"lran","cran","hran","lsat","ldead","hdead","hsat","lcurve","hcurve","lpower","hpower"};
        for (const auto& [key,value]:envelope.attributes) {
            if (!allowed.contains(key) || Integer(value)>65535) throw std::runtime_error("Unsupported calibration envelope attribute");
        }
        const auto get=[&](const char* key,int fallback){const auto v=envelope.attributes.find(key);return v==envelope.attributes.end()?fallback:static_cast<int>(Integer(v->second));};
        if (get("lran",0)!=0 || get("cran",32768)!=32768 || get("hran",65535)!=65535 || get("lcurve",32768)!=32768 || get("hcurve",32768)!=32768 || get("lpower",0)!=0 || get("hpower",0)!=0)
            throw std::runtime_error("Custom curves are not supported by the deadzone editor; no change made");
        auto& row=result[static_cast<std::size_t>(it-DeadzoneAxes.begin())];
        row.limits={get("lsat",0),get("ldead",32768),get("hdead",32768),get("hsat",65535)}; ValidateDeadzone(row);
    }
    return result;
}
Pr0Node UpdateDeadzones(Pr0Node root, const std::array<AxisDeadzone,9>& axes)
{
    (void)DecodeDeadzones(root);
    for (const auto& axis:axes) ValidateDeadzone(axis);
    auto& controls=root.children[0].children[0].children[1];
    for (auto& node:controls.children) {
        const auto index=static_cast<std::size_t>(std::find(DeadzoneAxes.begin(),DeadzoneAxes.end(),Integer(node.value))-DeadzoneAxes.begin());
        const std::array keys{"lsat","ldead","hdead","hsat"};
        for (std::size_t j=0;j<keys.size();++j) node.children[0].attributes[keys[j]]=std::to_string(axes[index].limits[j]);
    }
    return root;
}
DeadzoneSettings ReadDeadzones(const std::wstring& devicePath)
{
    DeadzoneSettings result; result.devicePath=devicePath; result.file=ReadX52CalibrationPath(devicePath); ValidateFile(result.file);
    result.original=Bytes(result.file); result.axes=DecodeDeadzones(Parse(result.original)); return result;
}
DeadzoneSettings ApplyDeadzones(const DeadzoneSettings& desired, const std::filesystem::path& directory)
{
    const auto current=ReadDeadzones(desired.devicePath);
    if (current.file!=desired.file || current.original!=desired.original) throw std::runtime_error("Calibration changed outside the mapper. Refresh before applying your edits.");
    const auto encoded=EncodePr0(UpdateDeadzones(Parse(current.original),desired.axes));
    if (current.axes==desired.axes) return current;
    const auto backups=directory/L"calibration-backups"; std::filesystem::create_directories(backups);
    const auto backup=backups/(L"before-"+std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count())+L".pr0");
    Write(backup,current.original);
    if (Bytes(backup)!=current.original) throw std::runtime_error("Calibration backup verification failed");
    Replace(current.file,encoded);
    try {
        ReloadX52Calibration(current.devicePath,current.file);
        auto actual=ReadDeadzones(current.devicePath);
        if (actual.axes!=desired.axes) throw std::runtime_error("Calibration readback mismatch");
        return actual;
    } catch (...) {
        try { Replace(current.file,current.original); ReloadX52Calibration(current.devicePath,current.file); }
        catch (...) { throw std::runtime_error("Calibration apply and rollback failed. Original saved in data/calibration-backups; refresh the device before continuing."); }
        throw;
    }
}
DeadzoneSettings ReloadSavedDeadzones(const DeadzoneSettings& expected, const std::filesystem::path& directory)
{
    const auto current=ReadDeadzones(expected.devicePath);
    if(current.file!=expected.file || current.original!=expected.original || current.axes!=expected.axes)
        throw std::runtime_error("Saved calibration changed or edits are pending. Refresh before reloading.");
    const auto backups=directory/L"calibration-backups";
    std::filesystem::create_directories(backups);
    const auto backup=backups/(L"before-reload-"+std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count())+L".pr0");
    Write(backup,current.original);
    if(Bytes(backup)!=current.original)throw std::runtime_error("Calibration backup verification failed");
    ReloadX52Calibration(current.devicePath,current.file);
    auto actual=ReadDeadzones(current.devicePath);
    if(actual.file!=current.file || actual.original!=current.original || actual.axes!=current.axes)
        throw std::runtime_error("Calibration changed during reload. Refresh to inspect the current settings.");
    return actual;
}
}
