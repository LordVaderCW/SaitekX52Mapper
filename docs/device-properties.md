# Themed X52 properties

The existing Visual Studio application now has Test, Deadzones, LEDs and MFD
pages. These are native controls in the mapper, not an embedded vendor window.

Test draws stick and mouse XY positions, five other axes, HID buttons 1-34 and
three hats. It uses the known original-X52 report-0/page/usage/link identities
already observed in this project. Missing inputs are dimmed; disconnection clears
positions. Inputs are driver-reported HID values, before mapper smoothing, not
unprocessed sensor/ADC readings. Live inputs remains available for exact counts
and learned physical names. No virtual output or game remapping was added.

Deadzones reads the existing settings for X, Y, twist, throttle, both rotaries,
slider and both mouse axes. Four handles edit minimum saturation, centre low,
centre high and maximum saturation in the vendor's 0..65535 envelope domain.
Handles cannot cross or remove all active travel on either side. Up/Down chooses
an axis, Space chooses a handle, Left/Right adjusts it, and Shift increases the
step. The red marker is the already driver-reported position; it is not a preview
of pending edits or a bypass of the current driver deadzones.

Use the mouse to drag the white handles horizontally. Centre handles are drawn
slightly apart when their boundaries coincide, so either can be grabbed; their
connector lines mark the exact boundary. Dragging makes a pending edit.
Apply driver deadzones creates a timestamped
original-file backup in `data/calibration-backups` beside the executable, writes
the calibration, requests the vendor reload, and checks the returned file path
and stored bounds. Refresh discards pending changes and reads the current file.
Externally changed files or paths require a fresh read; they are not overwritten
using stale editor state. A reload failure attempts to restore the original file
and reload it, reporting rollback failure explicitly if necessary.

The vendor owns its calibration in ProgramData/SmartTechnology/Cpls. Updating
that file is needed for Logitech compatibility. The mapper's own settings and
backups remain beside its executable. Only an absolute SaiC075C-*.pr0 calibration
path returned by the validated driver in that vendor directory is accepted.
The parser requires the original X52 controller/member GUIDs, calibration version
0x01000001, nine unique known axes and plain envelopes. Command/shift profiles,
unknown attributes, nonlinear curves and unsupported layouts are rejected.
This intentionally does not recreate every advanced vendor curve mode.

LEDs retains the existing asynchronous live brightness slider. MFD retains
backlight on/off (per the user's hardware observation), clutch and latching, and
adds two time-zone selectors, date format and daylight adjustment. The clock
selector chooses which of the three independent 12/24-hour formats to edit.
Time zones are fixed GMT offsets from the vendor's 37-entry table, not automatic
regional daylight-saving rules. Opening pages never applies defaults.

## Local interoperability evidence

All commands are gated to original X52 06A3:075C and Logitech driver 8.0.116.0.
These observations come from installed SaiC075C.dll, not X52 Pro APIs:

| Operation | CPL evidence / driver interface |
| --- | --- |
| Deadzone page | Dialog 5000, initialization 0x5BE0, apply 0x6060; four envelope limits copied at 0x91C8 |
| Calibration path | 0x10DC0; IOCTL 0x222804, two zero DWORDs input; DWORD/WORD header then UTF-16 path, absolute kind 0 |
| Calibration reload | 0x13EF5 serializes calibration, 0x13F03 calls 0x10BE0; IOCTL 0x222800 takes 10 zero header bytes plus terminated UTF-16 path, returns a DWORD token; 0x22280C takes {0, token} and returns a DWORD |
| Clock 2/3 offsets | Get 0x22361C / set 0x223620, {clock index 1/2, signed minutes}; page calls at 0x6899/0x68DE; table at 0x45E80 |
| Date format | Get 0x223624 / set 0x223628, DWORD 0..2; page applies selection at 0x6908 |
| Daylight adjustment | Get 0x223658 / set 0x22365C, DWORD 0/1; page applies checkbox at 0x684B |

Calibration reload uses the current calibration-only path, never a game command
profile. Readback proves the stored configuration and successful driver request,
not physical performance, reconnect persistence or an in-game result.

## Verification (2026-09-23)

Debug and Release solution builds and core tests passed. Tests cover calibration
roundtrips, original preservation, command-profile/duplicate-axis/custom-curve
rejection, envelope ordering and handle clamping. `--properties-read` read all
nine tuned axes from the live driver calibration file.

`--deadzone-roundtrip` changed stick X centre-low by one envelope unit, reloaded,
read back and restored all original limits. Original pre-edit bytes are backed up.
`--mfd-extra-roundtrip` changed and restored both time zones, date format and
daylight with driver readback. These switches deliberately write device settings;
ordinary core tests do not.

The native UI probe verified keyboard editing enables Apply, pending edits leave
the calibration file hash unchanged, and Refresh clears the pending edit without
writing the file. Inspected normal and maximized pages. Both custom views use
double buffering, preserve the drawing context state, and repaint their own
surface without redrawing unrelated controls. No in-game or dropout-fix claim.

## Manual calibration reload

The Deadzones page offers **Reload saved calibration** separately from Refresh
(read only) and Apply (save edits). Reload is available only after a valid current
calibration is read and while there are no pending edits. It backs up the current
file, refuses stale state, reapplies that exact calibration and verifies that the
file bytes and all envelopes remain unchanged. This is a manual recovery experiment,
not firmware recalibration or a USB reset. Check the physically centred stick and
game afterward; success of the request alone does not establish recovery.

On 2026-09-24 a live test returned an empty current calibration path. The new action
correctly refused to issue the reload. The previous calibration file exists on disk,
but the mapper does not guess which file to load. The UI asks the user to open the
Logitech X52 Properties / Deadzones page and refresh, then retry only if the driver
identifies its current file. Physical/in-game recovery is still unverified.

Logitech's [published original-X52 recalibration procedure](https://support.logi.com/hc/en-nz/articles/360023346933-Recalibrate-the-X52-H-O-T-A-S-axes-RegEdit)
requires unplugging and reconnecting USB. No documented no-disconnect hardware
recentring command has been established here. A large centre offset is not something
to conceal by automatically expanding deadzones or treating stationary input as a fault.
