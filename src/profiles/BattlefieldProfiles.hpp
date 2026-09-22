#pragma once
#include "../diagnostics/Journal.hpp"
#include <map>
#include <optional>

namespace x52 {
struct BattlefieldBinding {
    std::string id, context, action;
    int slot{}, type{}, button{}, axis{}, negate{};
};
std::vector<BattlefieldBinding> ReadBattlefieldBindings(const std::filesystem::path& path);
enum class JoystickBindingKind { NotJoystick, Button, Axis, Unassigned, Unknown };
JoystickBindingKind JoystickKind(const BattlefieldBinding& binding);
std::vector<BattlefieldBinding> JoystickBindings(const std::vector<BattlefieldBinding>& bindings);
struct ProfileAction { std::string device; unsigned usage{}, page{}; };
std::optional<ProfileAction> ProfileOutput(const BattlefieldBinding& binding);
std::wstring BindingLabel(const BattlefieldBinding& binding);
struct Pr0Node {
    std::string tag, value; // values retain the original quoting
    std::map<std::string, std::string> attributes;
    std::vector<Pr0Node> children;
};
Pr0Node ParsePr0(std::string_view text);
Pr0Node ReadX52Template(const std::filesystem::path& path);
std::string SerializePr0(const Pr0Node& node);
std::string EncodePr0(const Pr0Node& node); // UTF-16LE with BOM, as saved by Logitech
struct ProfileButton { std::string id, name; };
std::vector<ProfileButton> ProfileButtons(const Pr0Node& profile);
struct ProfileMapping { int mode{}; std::string control; BattlefieldBinding binding; };
Json BuildJoystickPlan(int game, const std::vector<ProfileMapping>& mappings);
Pr0Node BuildBattlefieldPr0(const Pr0Node& base, const std::vector<ProfileMapping>& mappings, const std::string& name);
std::filesystem::path BattlefieldSettingsPath(int game);
std::filesystem::path InstalledX52TemplatePath();
}
