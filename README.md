# X52 Battlefield Mapper

Native Windows 10/11 x64 C++20 diagnostics for the **original Saitek X52 PS28,
VID 06A3 / PID 075C**. Includes the **X52 Inspector** and **Battlefield profile authoring**.

Open **SaitekX52Mapper.sln** in Visual Studio 2022 with Desktop development
with C++ and a Windows 10/11 SDK. Select x64 Debug or Release and build.
The existing **SaitekX52Mapper** project is the application and startup target.
Press **F5** to build and debug, or **Ctrl+F5** to run without debugging.
If Visual Studio detects external project changes, reload them. The executable is
`out/x64/Release/SaitekX52Mapper.exe` (or the corresponding Debug folder).

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
**Stop / save capture** exports JSON in `%LOCALAPPDATA%/X52BattlefieldMapper`.
The same folder stores event JSONL and `learned-controls.json`. Capture includes
up to 200 pre-roll records and stops at about 64 MiB of serialized report data.
Normal operation does not write unchanged reports to disk. See
[the capture procedure](docs/x52-dropout-analysis.md) and [troubleshooting](docs/troubleshooting.md).

The **Battlefield profiles** tab imports your BF3/BF4 settings from Documents.
Choose a mode, physical button and existing command, then Assign / replace.
Export .pr0 draft writes a new profile for review in Logitech's profiler. Authoring
selections persist separately from the games. The first version supports keyboard
and left/right mouse commands on buttons; analogue axes stay in Battlefield and
hats are not exported yet. See [profile workflow](docs/profile-authoring.md).

The **MFD & LEDs** tab controls the installed original X52 driver's clutch,
latched clutch, MFD backlight, button LEDs, brightness and three clock formats.
Every change is read back; merely opening the tab does not change settings.
The **Live inputs** tab saves per-axis noise filtering for the identified throttle lever, both
rotaries and thumb slider. Raw captures stay intact; normalized and safe internal
values receive jitter suppression plus smoothing. Defaults: 45 ms, one raw count.
See [MFD settings and filtering](docs/mfd-settings.md).

**No independent game-output runtime or virtual controller is implemented.**
Logitech's installed software is responsible for activating exported profiles.
No hooks, injection, game modifications, anti-cheat interference, raw HID output/feature writes or
driver installation. Software reopen is not an electrical joystick reset.

See [PROJECT.md](PROJECT.md) for measured findings and remaining acceptance tests.
Source is GPL-3.0 under the existing LICENSE. Vendored nlohmann/json 3.12.0 retains
its MIT license in `third_party/nlohmann/LICENSE.MIT`.
Manufacturer reference-photo provenance and separate rights are in [assets/README.md](assets/README.md).
