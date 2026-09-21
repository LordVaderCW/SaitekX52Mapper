#include "BattlefieldProfiles.hpp"
#include "../util/Text.hpp"
#include <shlobj.h>
#include <regex>
#include <set>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace x52 {
namespace {
std::string ReadBounded(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > 4 * 1024 * 1024) throw std::runtime_error("Profile exceeds 4 MiB");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open profile for reading");
    std::string text((std::istreambuf_iterator<char>(stream)), {});
    if (stream.bad()) throw std::runtime_error("Profile read failed");
    if (text.starts_with("\xff\xfe")) {
        if (text.size() % 2) throw std::runtime_error("Truncated UTF-16 profile");
        std::wstring wide;
        for (std::size_t i = 2; i < text.size(); i += 2)
            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(text[i]) | (static_cast<unsigned char>(text[i + 1]) << 8)));
        return Utf8(wide);
    }
    if (text.starts_with("\xef\xbb\xbf")) text.erase(0, 3);
    return text;
}
std::string Unquote(const std::string& value)
{
    return value.size() >= 2 && value.front() == '\'' && value.back() == '\'' ? value.substr(1, value.size() - 2) : value;
}
std::string Quote(const std::string& value)
{
    if (value.empty() || value.size() > 160 || value.find_first_of("'\r\n\\") != std::string::npos)
        throw std::runtime_error("Profile label contains unsupported characters");
    return "'" + value + "'";
}
const Pr0Node& Child(const Pr0Node& parent, const std::string& tag)
{
    const Pr0Node* found{};
    for (const auto& node : parent.children) if (node.tag == tag) {
        if (found) throw std::runtime_error("Ambiguous profile section: " + tag);
        found = &node;
    }
    if (!found) throw std::runtime_error("Missing profile section: " + tag);
    return *found;
}
Pr0Node& Child(Pr0Node& parent, const std::string& tag)
{
    (void)Child(static_cast<const Pr0Node&>(parent), tag);
    for (auto& node : parent.children) if (node.tag == tag) return node;
    throw std::runtime_error("Missing profile section");
}
const Pr0Node& Controller(const Pr0Node& profile) { return Child(Child(profile, "controllers"), "controller"); }
void ValidateX52(const Pr0Node& profile)
{
    if (profile.tag != "profile" || Unquote(Controller(profile).value) != "e81d998b-c604-4d71-be97-35ca01439c7e" ||
        Unquote(Child(Controller(profile), "member").value) != "c7719f41-f667-4514-bbb4-3f38c9e4d05a")
        throw std::runtime_error("Expected the original X52 profile template, not an X52 Pro profile");
    (void)Child(Controller(profile), "controls");
    const auto& shifts = Child(Controller(profile), "shifts").children;
    if (shifts.size() != 6)
        throw std::runtime_error("Expected the stock six-mode X52 template");
    const std::array<std::string, 6> modes{"Mode 1", "Mode 2", "Mode 3", "Mode 1 + Pinkie", "Mode 2 + Pinkie", "Mode 3 + Pinkie"};
    for (std::size_t i = 0; i < modes.size(); ++i)
        if (shifts[i].tag != "shift" || !shifts[i].attributes.contains("name") || Unquote(shifts[i].attributes.at("name")) != modes[i])
            throw std::runtime_error("Unexpected X52 mode order; template needs review");
}
std::string Guid()
{
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Cannot create profile command ID");
    wchar_t text[40]{};
    if (!StringFromGUID2(guid, text, 40)) throw std::runtime_error("Cannot format profile command ID");
    auto result = Utf8(text); result = result.substr(1, result.size() - 2);
    return result;
}
unsigned KeyboardUsage(int scan)
{
    // DirectInput scan codes (dinput.h) -> USB keyboard page 0x07 physical keys.
    static const std::map<int, unsigned> keys{
        {1,0x29},{2,0x1e},{3,0x1f},{4,0x20},{5,0x21},{6,0x22},{7,0x23},{8,0x24},{9,0x25},{10,0x26},{11,0x27},
        {12,0x2d},{13,0x2e},{14,0x2a},{15,0x2b},{16,0x14},{17,0x1a},{18,0x08},{19,0x15},{20,0x17},{21,0x1c},
        {22,0x18},{23,0x0c},{24,0x12},{25,0x13},{26,0x2f},{27,0x30},{28,0x28},{29,0xe0},{30,0x04},{31,0x16},
        {32,0x07},{33,0x09},{34,0x0a},{35,0x0b},{36,0x0d},{37,0x0e},{38,0x0f},{39,0x33},{40,0x34},{41,0x35},
        {42,0xe1},{43,0x31},{44,0x1d},{45,0x1b},{46,0x06},{47,0x19},{48,0x05},{49,0x11},{50,0x10},{51,0x36},
        {52,0x37},{53,0x38},{54,0xe5},{55,0x55},{56,0xe2},{57,0x2c},{58,0x39},
        {59,0x3a},{60,0x3b},{61,0x3c},{62,0x3d},{63,0x3e},{64,0x3f},{65,0x40},{66,0x41},{67,0x42},{68,0x43},
        {69,0x53},{70,0x47},{71,0x5f},{72,0x60},{73,0x61},{74,0x56},{75,0x5c},{76,0x5d},{77,0x5e},{78,0x57},
        {79,0x59},{80,0x5a},{81,0x5b},{82,0x62},{83,0x63},{86,0x64},{87,0x44},{88,0x45},
        {156,0x58},{157,0xe4},{181,0x54},{183,0x46},{184,0xe6},{197,0x48},
        {199,0x4a},{200,0x52},{201,0x4b},{203,0x50},{205,0x4f},{207,0x4d},{208,0x51},{209,0x4e},{210,0x49},{211,0x4c}};
    const auto found = keys.find(scan); return found == keys.end() ? 0 : found->second;
}
}
std::vector<BattlefieldBinding> ReadBattlefieldBindings(const std::filesystem::path& path)
{
    std::istringstream stream(ReadBounded(path));
    const std::regex pattern(R"(^GstKeyBinding\.([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)\.([0-9]+)\.([A-Za-z0-9_]+)\s+(-?[0-9]+)\s*$)");
    std::map<std::string, std::map<std::string, int>> fields;
    std::map<std::string, BattlefieldBinding> bindings;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.starts_with("GstKeyBinding.")) continue;
        std::smatch match;
        if (!std::regex_match(line, match, pattern)) throw std::runtime_error("Malformed Battlefield binding line");
        const auto id = match[1].str() + "." + match[2].str() + "." + match[3].str();
        if (!fields[id].emplace(match[4].str(), std::stoi(match[5].str())).second)
            throw std::runtime_error("Duplicate Battlefield binding field: " + id);
        bindings[id] = {id, match[1].str(), match[2].str(), std::stoi(match[3].str())};
    }
    std::vector<BattlefieldBinding> result;
    for (auto& [id, binding] : bindings) {
        const auto& values = fields.at(id);
        for (const auto key : {"type", "button", "axis", "negate"})
            if (!values.contains(key)) throw std::runtime_error("Incomplete Battlefield binding: " + id);
        binding.type = values.at("type"); binding.button = values.at("button"); binding.axis = values.at("axis"); binding.negate = values.at("negate");
        result.push_back(binding);
    }
    if (result.empty()) throw std::runtime_error("No Battlefield binding records found");
    return result;
}
std::optional<ProfileAction> ProfileOutput(const BattlefieldBinding& binding)
{
    if (binding.type == 0 && binding.axis == 0 && KeyboardUsage(binding.button))
        return ProfileAction{"keyboard", KeyboardUsage(binding.button), 7};
    if (binding.type == 1 && binding.axis == 24 && binding.button >= 0 && binding.button <= 1)
        return ProfileAction{"mouse", static_cast<unsigned>(binding.button + 1), 9};
    return std::nullopt; // Unbound, joystick axes and unproven mouse encodings stay untouched.
}
std::wstring BindingLabel(const BattlefieldBinding& binding)
{
    auto label = Wide(binding.context + " / " + binding.action + " [" + std::to_string(binding.slot) + "]");
    if (binding.type == 0) {
        wchar_t name[96]{};
        const auto code = (static_cast<unsigned>(binding.button) & 0x7f) << 16;
        const auto extended = binding.button >= 128 ? 1u << 24 : 0u;
        if (GetKeyNameTextW(static_cast<LONG>(code | extended), name, 96)) label += L" - " + std::wstring(name);
        else label += L" - scan " + std::to_wstring(binding.button);
    } else label += binding.button == 0 ? L" - Left mouse" : L" - Right mouse";
    return label;
}
Pr0Node ParsePr0(std::string_view text)
{
    if (text.size() > 4 * 1024 * 1024) throw std::runtime_error("PR0 exceeds 4 MiB");
    std::size_t offset{}, count{};
    const auto whitespace = [&] { while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) ++offset; };
    const auto token = [&]() -> std::string {
        whitespace(); const auto start = offset;
        if (offset < text.size() && text[offset] == '\'') {
            ++offset;
            while (offset < text.size() && text[offset] != '\'') {
                if (text[offset] == '\\') throw std::runtime_error("Escaped PR0 strings are not supported");
                ++offset;
            }
            if (offset == text.size()) throw std::runtime_error("Unterminated PR0 quote");
            ++offset;
        } else while (offset < text.size() && !std::isspace(static_cast<unsigned char>(text[offset])) && text[offset] != '[' && text[offset] != ']' && text[offset] != '=') ++offset;
        if (start == offset) throw std::runtime_error("Expected PR0 token");
        return std::string(text.substr(start, offset - start));
    };
    const auto parse = [&](auto&& self, unsigned depth) -> Pr0Node {
        whitespace();
        if (depth > 32 || ++count > 20000 || offset >= text.size() || text[offset++] != '[') throw std::runtime_error("Invalid PR0 structure");
        Pr0Node node; node.tag = token(); whitespace();
        if (offset < text.size() && text[offset] == '=') { ++offset; node.value = token(); }
        for (;;) {
            whitespace(); if (offset == text.size()) throw std::runtime_error("Unclosed PR0 block");
            if (text[offset] == ']') { ++offset; return node; }
            if (text[offset] == '[') node.children.push_back(self(self, depth + 1));
            else {
                auto key = token(); whitespace();
                if (offset == text.size() || text[offset++] != '=') throw std::runtime_error("Invalid PR0 attribute");
                if (!node.attributes.emplace(std::move(key), token()).second) throw std::runtime_error("Duplicate PR0 attribute");
            }
        }
    };
    auto root = parse(parse, 0); whitespace();
    if (offset != text.size()) throw std::runtime_error("Trailing content after PR0 profile");
    return root;
}
Pr0Node ReadX52Template(const std::filesystem::path& path)
{
    auto root = ParsePr0(ReadBounded(path)); ValidateX52(root); return root;
}
std::string SerializePr0(const Pr0Node& root)
{
    std::ostringstream output;
    const auto write = [&](auto&& self, const Pr0Node& node, int depth) -> void {
        output << std::string(static_cast<std::size_t>(depth * 2), ' ') << '[' << node.tag;
        if (!node.value.empty()) output << '=' << node.value;
        for (const auto& [key, value] : node.attributes) output << ' ' << key << '=' << value;
        if (!node.children.empty()) output << "\r\n";
        for (const auto& child : node.children) self(self, child, depth + 1);
        if (!node.children.empty()) output << std::string(static_cast<std::size_t>(depth * 2), ' ');
        output << "]\r\n";
    };
    write(write, root, 0); return output.str();
}
std::vector<ProfileButton> ProfileButtons(const Pr0Node& profile)
{
    ValidateX52(profile); std::vector<ProfileButton> buttons;
    for (const auto& control : Child(Controller(profile), "controls").children)
        if (control.tag == "button" && control.value != "0x00090006" && control.value != "0x0009001E")
            buttons.push_back({control.value, Unquote(control.attributes.at("name"))});
    return buttons; // Pinkie and clutch remain reserved for the vendor's mode/profile selection.
}
std::string EncodePr0(const Pr0Node& node)
{
    const auto wide = Wide(SerializePr0(node));
    std::string bytes{"\xff\xfe"}; bytes.reserve(2 + wide.size() * 2);
    for (const auto character : wide) {
        bytes.push_back(static_cast<char>(character & 0xff));
        bytes.push_back(static_cast<char>((character >> 8) & 0xff));
    }
    return bytes;
}
Pr0Node BuildBattlefieldPr0(const Pr0Node& base, const std::vector<ProfileMapping>& mappings, const std::string& name)
{
    ValidateX52(base);
    if (mappings.empty()) throw std::runtime_error("Assign at least one control before exporting");
    auto profile = base; profile.value = Quote(name); profile.attributes["version"] = "0x00000005";
    auto& shifts = Child(Child(Child(profile, "controllers"), "controller"), "shifts").children;
    auto& commands = Child(profile, "commands").children;
    const auto buttons = ProfileButtons(base); std::set<std::pair<int, std::string>> used;
    // Input must be the stock No Profile template: preserve its default mouse
    // commands, selections and fallback chains, not arbitrary sample assignments.
    if (Unquote(base.value) != "No Profile") throw std::runtime_error("Export requires the installed stock No Profile template");
    for (const auto& mapping : mappings) {
        if (mapping.mode < 0 || mapping.mode >= 6 || !used.emplace(mapping.mode, mapping.control).second)
            throw std::runtime_error("Invalid mode or duplicate physical control in profile draft");
        const auto button = std::find_if(buttons.begin(), buttons.end(), [&](const auto& entry) { return entry.id == mapping.control; });
        const auto action = ProfileOutput(mapping.binding);
        if (button == buttons.end() || !action) throw std::runtime_error("Unsupported profile input or output");
        auto& shift = shifts[static_cast<std::size_t>(mapping.mode)];
        auto assignments = std::find_if(shift.children.begin(), shift.children.end(), [](const auto& child) { return child.tag == "assignments"; });
        if (assignments == shift.children.end()) { shift.children.push_back({"assignments"}); assignments = std::prev(shift.children.end()); }
        std::erase_if(assignments->children, [&](const auto& child) { return child.tag == "button" && child.value == mapping.control; });
        const auto command = Guid();
        assignments->children.push_back({"button", mapping.control, {{"name", Quote(button->name)}, {"role", "bands"}},
            {{"bands", {}, {}, {{"band", "1", {{"command", command}}, {}}}}}});
        commands.push_back({"actioncommand", command, {{"name", Quote(mapping.binding.context + " " + mapping.binding.action)}},
            {{"actionblock", {}, {}, {{"action", {}, {{"device", action->device}, {"usage", std::to_string(action->usage)},
                {"page", std::to_string(action->page)}, {"value", "1"}}, {}}}}}});
    }
    return profile;
}
std::filesystem::path BattlefieldSettingsPath(int game)
{
    if (game != 3 && game != 4) throw std::runtime_error("Choose Battlefield 3 or 4");
    PWSTR text{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &text))) throw std::runtime_error("Cannot locate Documents");
    struct Release { PWSTR value; ~Release() { CoTaskMemFree(value); } } release{text};
    return std::filesystem::path(text) / (game == 3 ? L"Battlefield 3/settings/PROF_SAVE_profile" : L"Battlefield 4/settings/PROFSAVE_profile");
}
std::filesystem::path InstalledX52TemplatePath()
{
    wchar_t path[MAX_PATH]{};
    const auto length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH) throw std::runtime_error("Cannot locate the X52 template directory");
    return std::filesystem::path(path) / L"SaiD075C.pr0";
}
}
