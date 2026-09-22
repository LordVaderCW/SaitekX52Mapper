# X52 Battlefield Mapper

Native Windows 10/11 x64 C++20 diagnostics for the **original Saitek X52 PS28,
VID 06A3 / PID 075C**. Includes the **X52 Inspector** and **Battlefield profile authoring**.

Open **SaitekX52Mapper.sln** in Visual Studio 2022 with Desktop development
with C++ and a Windows 10/11 SDK. Select x64 Debug or Release and build.
The existing **SaitekX52Mapper** project is the application and startup target.
Press **F5** to build and debug, or **Ctrl+F5** to run without debugging.
If Visual Studio detects external project changes, reload them. The executable is
`out/x64/Release/SaitekX52Mapper.exe` (or the corresponding Debug folder).

Dependency and packaged application installers belong in [dist](dist/README.md).
It includes the preserved `X52_HOTAS_x64_8_0_213_0.exe` vendor package and its checksum.

From a Visual Studio developer PowerShell:

```powershell
msbuild SaitekX52Mapper.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild SaitekX52Mapper.sln /m /p:Configuration=Release /p:Platform=x64
./out/x64/Debug/X52Tests.exe
./out/x64/Release/X52Tests.exe --hardware
```

`--hardware` requires a connected PS28 and performs ten **software handle reopen**
cycles. It never sends output/feature reports or changes device configuration.

The inspector provides live raw/decoded reports, byte/bit differences, persistent
control learning, native device notifications, full-report fault capture, internal
failsafe state and automatic recovery of failed Windows reads. USB disappearance
and a stick-side Mini-DIN fault are different observations. There is no verified
automatic stick-dropout signature yet.

Double-click a **Live inputs** row (or press Enter) to open its physical-control
picker. Actual X52 reference photos accompany selectable buttons, hats, directions,
axes and switches. Links persist and can be corrected or cleared. Use **Learn
controls** to collect an isolated movement first, then **Identify candidate**.
See [control identification](docs/control-identification.md).
Use **Connection health** to capture a fault and mark its start/return.
**Stop / save capture** exports JSON in the `data` folder beside `SaitekX52Mapper.exe`.
Learned controls, input filtering settings and event logs also live there. **Open
data folder** opens this folder. Storage follows the executable, regardless of
the working directory; Debug and Release builds each have their own `data` folder.
Keep the executable in a writable folder. If storage is unavailable, the app
reports an error instead of falling back to AppData. Move the `data` folder with
the executable to preserve settings and captures.
The same folder stores event JSONL and `learned-controls.json`. Capture includes
up to 200 pre-roll records and stops at about 64 MiB of serialized report data.
Normal operation does not write unchanged reports to disk. See
[the capture procedure](docs/x52-dropout-analysis.md) and [troubleshooting](docs/troubleshooting.md).

The **Battlefield profiles** page imports your BF3/BF4 settings from Documents.
It shows joystick/joypad bindings only: button codes, axes, inversion and unassigned
entries. Choose a mode, an identified X52 input and an assigned joystick binding,
then Assign / replace. **Export joystick plan** saves a JSON plan with the exact
Battlefield fields and learned HID links. Plans are not active controller profiles;
joystick output and PR0 joystick export are not implemented. Existing keyboard
drafts remain separate. See [profile workflow](docs/profile-authoring.md).

The **MFD & LEDs** page controls the installed original X52 driver's clutch,
latched clutch, MFD backlight, button LEDs, brightness and three clock formats.
Every change is read back; merely opening the page does not change settings.
The **Live inputs** page saves per-axis noise filtering for the identified throttle lever, both
rotaries and thumb slider. Raw captures stay intact; normalized and safe internal
values receive jitter suppression plus smoothing. Defaults: 45 ms, one raw count.
See [MFD settings and filtering](docs/mfd-settings.md).

The **Registry tweaks** page reads the original X52's USB enhanced-power setting.
It can set `EnhancedPowerManagementEnabled` to `0` with Windows administrator
approval, preserving and restoring the original value in the local data folder.
This is a device-specific dropout experiment, not a confirmed fix. See
[power-management behaviour and sources](docs/power-management.md).

**No independent game-output runtime or virtual controller is implemented.**
Joystick plans cannot be activated in Logitech's profiler.
No hooks, injection, game modifications, anti-cheat interference, raw HID output/feature writes or
driver installation. Software reopen is not an electrical joystick reset.

See [PROJECT.md](PROJECT.md) for measured findings and remaining acceptance tests.
Source is GPL-3.0 under the existing LICENSE. Vendored nlohmann/json 3.12.0 retains
its MIT license in `third_party/nlohmann/LICENSE.MIT`.
Manufacturer reference-photo provenance and separate rights are in [assets/README.md](assets/README.md).
