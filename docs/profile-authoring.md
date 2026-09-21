# Battlefield to Logitech profile workflow

Use the existing SaitekX52Mapper Visual Studio project. In **Battlefield profiles**:

1. Import Battlefield 3 or 4. The app reads `Documents/Battlefield 3/settings/PROF_SAVE_profile`
   or `Documents/Battlefield 4/settings/PROFSAVE_profile`, using Windows' Documents location.
2. Select an X52 mode, a named physical button and an existing game command/key.
   Context filtering separates jet, heli, infantry, vehicle and general bindings.
3. Assign / replace. Each button has one override per mode; assignments persist.
4. Export .pr0 draft. Open it in Logitech's editor, inspect the modes, then test
   the programmed outputs before using it in Battlefield.

Exports use the installed original-X52 `SaiD075C.pr0` as the base, retaining its
control catalog, mouse defaults, six mode selections and fallback relationships.
Mode 2/3 inherit Mode 1. Pinkie modes inherit their respective base mode. Removing
an override restores inheritance/default behaviour; it is not an explicit Disable.
Pinkie and clutch are reserved for shift/profile selection in this first editor.
Mouse/scroll buttons retain vendor assignments unless explicitly overridden.

Supported outputs are known DirectInput keyboard scan codes translated to USB
keyboard usages, plus left/right mouse actions found in the installed template.
Unbound value 255, unsupported keys, joystick bindings and mouse-axis/wheel
encodings are excluded from the selector. Hat/axis input programming is not
exported yet. Analogue flight controls remain in Battlefield; the app does not
rewrite game settings or convert axes to digital keys. Check custom bindings for
double actions if the game also listens to the same physical button directly.

Imports retain context/action/slot identities. Saved plans resolve these against
a fresh game import; missing/unsupported rows are reported for reassignment.
Editing a game's key changes the selected output after reimport. Key labels use
the Windows keyboard layout; the exported usage identifies the physical key.

PR0 files are bracket-structured text, not JSON. Parsing bounds size, nesting and
node counts, validates the original X52 controller/member and stock mode order,
rejects ambiguous/unsupported syntax, and retains unknown fields in the supported
syntax. Exports use UTF-16LE with BOM and version 5, matching the user's sample.
No profile is activated automatically or substituted for a vendor profile.

Observed on 2026-09-21: BF3 has 259 binding records; BF4 has 372, including unbound
and non-keyboard records. The saved sample confirms Trigger (`0x00090001`) to
Space (`page=7`, `usage=0x2C`, `value=1`). Generated trigger-only examples in
`out/profile-research` are integration probes, not complete or game-verified layouts.

The [Logitech X52 guide](https://www.logitech.com/assets/65328/2/x52-hotas.pdf),
pages 13–16, describes profiles created in the software and selected from files
on the computer through the MFD. Logitech's driver/profiler is the activation
runtime here. Standalone onboard storage/firmware upload has not been established.
