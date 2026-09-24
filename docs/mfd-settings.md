# Original X52 MFD settings and input filtering

Open the existing SaitekX52Mapper.sln in Visual Studio, select Debug/x64 and F5.
The separate MFD and LEDs tabs read the attached controller when first visited. Refresh
from X52 reloads changes made in Logitech's panel. Switches apply immediately.
Button LED brightness uses a native 0..100% drag slider with a percentage label;
dragging and keyboard adjustments update the device live, with readback after
each write. Only one request runs at once; newer positions replace the pending
position, including the final position on release. Updates are limited to roughly
30 per second and never move the thumb back to an older completed request. The
slider remains enabled during writes; other device settings wait for them to finish.
Errors discard pending changes and disable settings until a fresh read succeeds.
The MFD backlight is on/off only in this UI: off sends 0, on sends 100. The user
reported no visible intermediate MFD brightness changes even though the driver
accepts and returns intermediate numbers. Driver readback alone does not prove
physical brightness control. No settings are changed merely by opening a tab.

Uncheck Use I for Logitech profile selection to release I for ordinary input.
Press once to latch means press I to enter profile selection and again to exit;
otherwise hold I. Latching is disabled in our UI when profile selection is off.
The PR0 editor now includes I. Exports using it remind the user to disable Logitech
profile selection before use. Pinkie remains reserved for shifting.

The clock selector chooses Clock 1, 2 or 3; one explicit format dropdown edits
that slot's 24-hour or 12-hour setting. Selecting another clock only displays its
current format. MFD also edits clock 2/3 GMT offsets, date format and clock 1
daylight adjustment; see docs/device-properties.md for the extended adapter.
The three format settings remain independent.

Noise filtering is on Live inputs, below the raw report display. The input table
expands with the window while the raw report and filter controls remain anchored
below it. All four switches, smoothing time, jitter tolerance and Save input
filtering are available there.

The physical settings are driver-wide, not embedded in an exported PR0 profile.
The mapper does not reapply saved device settings on startup or reconnect.

## Provenance and limits

The installed original-X52 package is Logitech 8.0.116.0, USB/HID 06A3:075C,
SaiC075C.dll and SaiK075C.sys. The INF registers this CPL for this device. Static
inspection traced resource dialog 8000 (MFD), its initialization/application
handlers at RVAs 0x6270/0x6810 and virtual calls to the following driver requests.
These are interoperability observations of the locally installed vendor software,
not a published SDK and not X52 Pro DirectOutput. No vendor binary is redistributed.

| Setting | Get / set IOCTL | Data | CPL evidence (RVA) |
| --- | --- | --- | --- |
| Clutch button | 0x223604 / 0x223608 | DWORD 0x0009001E enabled, 0 disabled | 0x156E0 / 0x15790; UI compares/writes 0x9001E |
| Latched clutch | 0x22360C / 0x223610 | DWORD 0 or 1 | 0x15570 / 0x15630 |
| MFD brightness | 0x223614 / 0x223618 | DWORD 0..100 | 0x15030 / 0x150E0; UI range 0..100 |
| LED brightness | 0x222004 / 0x222000 | Get: DWORD index 0, DWORD percent result. Set: WORD index 0, WORD percent | 0x14ED0 / 0x14F90 -> 0x1A450; dialog 7000 handler 0x61E0 |
| Clock format | 0x22362C / 0x223630 | DWORD clock index 0..2, DWORD 12-hour flag | 0x15190 / 0x15230 -> 0x111F0 / 0x11320 |

The adapter verifies VID/PID and the active PnP driver version before issuing these
requests through a shared HID device handle. Queries validate sizes and ranges;
writes are bounded to the listed options and followed by fresh driver readback.
Unknown versions are rejected. Overlapped requests have a one-second timeout and
cancel/drain outstanding I/O before buffers are freed. Settings work runs off the
UI and HID-reading threads. No registry edits, raw USB commands or HID feature
reports are sent.

The manufacturer describes the clutch and display controls in its
[X52 user guide](https://www.logitech.com/assets/65328/2/x52-hotas.pdf).
Microsoft documents [DeviceIoControl](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol)
and [SetupDiGetDevicePropertyW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdevicepropertyw).

## Filtering

Use the four checkboxes to enable filtering per named throttle axis. Save input
filtering persists the choices, smoothing time (0..250 ms) and jitter tolerance
(0..5 raw counts) in `data/input-filters.json` beside the application executable.
Filtering requires the corresponding physical axis link from the identification
picker; unknown axes are not guessed from usage numbers.

A small hysteresis band holds the last accepted normalized target; exponential
smoothing approaches that target using elapsed report time. Endpoints remain
reachable. Turning filtering off bypasses both stages. Zero smoothing disables
only the exponential stage; zero jitter disables the hysteresis band. Reconnect,
long gaps, changed assignments and settings changes reset history. Raw readings,
learn evidence and report captures retain their original values. Export includes
separate filtered state and filter configuration. Failsafe handling runs afterward.

This smooths the mapper's normalized/internal inputs. It cannot alter the original
Windows controller's axes as read directly by games.

## Verification

Release live roundtrip: all seven settings changed, read back, restored and read
back successfully. Final state matched the initial state. No visual LCD/LED claim
is made. --mfd is read-only; --mfd-roundtrip deliberately changes and restores each
setting. Ordinary test runs never write device settings.

Synthetic tests cover one-count jitter, movement response, full travel, bypass,
reconnect initialization, unfiltered stick/buttons, report-rate independence,
parameter rejection and immediate failsafe after smoothing. Live inspector tests
also passed ten software reopen cycles. Physical cable dropouts and game behaviour
remain separate acceptance tests.
