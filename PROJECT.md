# X52 Battlefield Mapper â€” canonical project record

## Purpose and scope

Build an external native Windows diagnostic/remapping utility for original Saitek
X52 PS28. The inspector provides evidence-driven dropout capture and recovery.
On 2026-09-21 the user expanded scope to Battlefield binding import and Logitech
PR0 profile authoring after installing the correct X52 software. This supersedes
the original Milestone-1-only gate for profile authoring, not for speculative
hardware writes or virtual-output implementation. The original brief remains
preserved in `docs/requirements.md` as historical requirements.

## Architecture and decisions

- Windows x64 C++20, Visual Studio v143, Unicode Win32 controls. `/W4 /WX /permissive-`.
- `device`: SetupAPI enumeration and HID strings/attributes/capabilities. VID/PID
  are authoritative. Only one matching joystick/gamepad collection is auto-opened;
  multiple targets produce a visible selection limitation rather than a guessed choice.
- `hid`: read-only overlapped ReadFile plus descriptor-driven HidP parsing.
  Report size comes from HIDP_CAPS, never the USB endpoint's packet size.
- `input`: typed normalized controls with report/page/usage/link identity. Physical
  names and Stick/Throttle groups require recorded user assignments.
- `diagnostics`: JSONL events, bounded explicit captures, conservative health state,
  internal safe values, validation and blend, user-mark analysis.
- `app`: dedicated input worker, mutex snapshots, bounded command queue. UI runs at
  10 Hz independently of reports. One event writer; one temporary export worker.
- `profiles`: schema/device validation and atomic learned-control JSON replacement.
- `ui`: native main window and per-HID physical-control picker, with embedded X52
  manufacturer photos rendered through Windows GDI+.
- `profiles/BattlefieldProfiles`: bounded BF3/BF4 binding import, PR0 syntax tree,
  original-X52 template validation and draft export for Logitech activation.
- `mapping` / `output`: no independent runtime or virtual output backend.

Read cancellation is drained before destroying OVERLAPPED, buffer or handle.
Manual recovery affects this application's read handle only. Unknown hardware
recovery is represented by UnsupportedHardwareRecovery, which sends no commands.

## Hardware and established findings â€” 2026-09-21

After the user installed Logitech software, both X52 H.O.T.A.S. HID and USB nodes
report OK, VID/PID unchanged (06A3:075C). Installed driver: Logitech 8.0.116.0,
oem58.inf; profiler executables: 8.0.213.0. The user reports brighter LEDs, a lit
LCD and working throttle mouse/scroll buttons. They initially reported no further
dropouts, then reported a very brief ("microscopic") recurrence. Duration was not
measured, and no corresponding raw capture was identified. Cable fault remains
a hypothesis; driver installation did not establish a complete dropout fix.

The installed `C:\Windows\System32\SaiD075C.pr0` is an original-X52 control catalog
and six-mode template. Its SHA-256 is
`5A488F16251CA09CC1EF58A387970CF6153F8871E91A808A170514CC249F64AC`.
The user-created `x52 export sample.pr0` confirms version 5, UTF-16LE BOM and a
Trigger assignment to keyboard page 7 / usage 0x2C / value 1 (Space). These are
vendor/profile-format facts, separate from physical dropout verification.

The following report-capability observations precede that driver change:

Windows currently enumerates Saitek X52 Flight Control System, manufacturer Saitek,
VID 06A3, PID 075C, generic desktop joystick collection (page 1, usage 4).
Live read-only probes succeeded. An early Debug probe received 207 reports in
approximately 5.5 seconds with no reported parser/read error.

Descriptor and live report observations:

| Capability | Observed value |
|---|---|
| Windows input report length | **15 bytes**, including byte 0 = report ID 0 |
| Output / feature report lengths on this collection | 0 / 0 |
| Desktop X, Y usage range | 0..2047, 11 bits |
| Desktop Rz usage range | 0..1023, 10 bits |
| Desktop Z, Rx, Ry, slider usage range | 0..255, 8 bits |
| Hat usage 0x39 | 1..8; HasNull; raw 0 observed as neutral |
| Page 5 usages 0x24 / 0x26 | 0..15, 4 bits; physical meaning not established |
| Button page | usages 1..34 |
| Decoded controls | 44, link collection 1, report ID 0 |

The supplied hardware brief lists a 16-byte endpoint packet maximum, USB 2.0,
10 ms polling, etc. These are user-supplied facts, not independently measured USB
descriptor findings. The 15-byte HID buffer observation does not contradict an
endpoint maximum of 16 bytes. Do not pad the buffer to 16 or guess packet offsets.

## Health and recovery semantics

Default: neutralise, five consecutive valid reports, 100 ms analog blend, 2000 ms
report freshness limit. Settings can be changed for the current session in the UI.
Button neutral is released; hat neutral is -1; axis neutral is user configurable.
Hold-last is explicit and remains flagged invalid during failure.

Confirmed Windows read failure causes safe internal state, close, one-second
backoff, rediscovery, reopen, parser reset, validation and restoration. Manual
reinitialize uses the same flow. Silence withholds stale input but does not prove
USB removal, stick failure, or require an aggressive reopen. Fresh valid reports
recover automatically. Report IDs validate independently.

Stick-side failure detection remains **UNKNOWN**. User markers suspend learned
Stick and unassigned controls while learned Throttle controls continue to be
decoded. This is a conservative group assignment mechanism, not proof of throttle
health. Changes during marked intervals establish observed activity only. A return
marker validates new reports from learned Stick controls before restoring. If no
Stick group has been learned, it conservatively validates all known report IDs.
Stationary input never increments suspected-dropout counters automatically.

## Dependencies and build

The canonical solution is the user's existing `SaitekX52Mapper.sln`, with its
original `SaitekX52Mapper/SaitekX52Mapper.vcxproj` identity and startup target.
The real Win32 entry point is now `SaitekX52Mapper/SaitekX52Mapper.cpp`; all HID,
diagnostics, control-picker sources and embedded photos are included in that
project and its Solution Explorer filters. F5 uses `$(TargetPath)` and the
repository working directory. Debug symbols are generated for source debugging.
The redundant X52BattlefieldMapper solution/project were removed. `X52Tests` is
included in the existing solution's x64 builds. Build.ps1 uses that same solution.
The supported/tested configuration remains x64; original x86 configurations are
retained but are not part of the acceptance claim.
Both Debug and Release x64 builds through the original solution and their core
tests passed after integration. Executable/PDB generation and source/resource
inclusion were checked. The running IDE was not automated for this correction.

Windows SDK HID, SetupAPI, common controls, shell/COM support. No runtime framework,
drivers or third-party binaries are bundled. nlohmann/json 3.12.0 header is vendored
from the upstream release tag, MIT license retained. SHA-256:
`AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63`.
Existing repository license is GPL-3.0 and was preserved.
The five embedded manufacturer reference photos retain separate image rights;
see `assets/README.md` for provenance. They depict the non-Pro X52 and provide no
evidence of HID numbering.

## Implementation status and acceptance

Implemented: enumeration, live descriptor decoding, raw/changed-byte display,
Learn baseline/candidates/JSON evidence, device notifications, explicit full-report
capture, structured events, internal failsafe, watchdog freshness, automatic read
recovery, manual reopen, report validation/blending and diagnostic exports.
Manual dropout marking now auto-arms a capture from the 200-record pre-roll, unless
an earlier bounded capture is awaiting export. This addresses the first observed
workflow where a fault marker preceded explicit capture startup.

Physical identification iteration: every live HID row now opens a native picker
through double-click, Enter or Identify selected input. The same picker handles
Learn candidates. A 31-feature physical catalog covers the original X52's controls,
with explicit directions/positions, type checks and duplicate-link prevention.
Stable catalog IDs, names, groups, neutral values and evidence persist atomically.
Manual labels are USER_ASSIGNED; Learn-backed labels are OBSERVED. No mapping is
promoted to VERIFIED. Latest per-control transitions are retained between UI frames.
See `docs/control-identification.md` for the identification workflow and sources.

The picker now overlays a small transparent red outline circle on the selected
physical control. UI-only source-image anchors follow crop/scale transformations;
the stroke stays 3 pixels, without covering the photographed control. Grip axes
highlight the grip; individual controls use tighter circles. A fifth embedded
side photo exposes the trigger and pinkie switch. Far-side areas are explicitly
labelled, and hidden controls have no misleading marker in unrelated views.

Build/test results are recorded in `docs/testing.md`. Physical controls must still
be operated individually, and actual Mini-DIN faults plus physical USB reconnects
must be observed repeatedly before all Milestone 1 hardware acceptance criteria
can honestly be marked complete. Software reopen tests do not substitute for those.
Debug and Release builds and core tests pass. The final Debug live test decoded
500 reports and passed ten software reopen cycles plus manual-marker auto-capture
and validation checks. The user reported a ~0.5 s physical dropout; its original
marker predates the retained raw trace. No protocol signature was promoted.

## Known limitations and future work

- No verified PS28 stick-disconnect status bit or repeatable fault signature yet.
- No electrical reset command is known or sent.
- No BF3/BF4 game testing or verified mappings. No virtual driver/backend chosen.
- One physical PS28 at a time; no collection selector yet.
- Value arrays are explicitly rejected, not silently misdecoded; this PS28 uses
  scalar values. Generic hats with logical null are supported.
- UI shows sampled transitions; explicit capture retains every delivered report.
  Windows/USB buffering can still lose reports before the application reads them.
- Capture is memory-bounded and exported on stop/normal exit. Process crash or
  power loss can lose unexported capture data; regular event log is flushed.
- Recovery settings are session-only. Learned names/groups persist outside the EXE.
- Physical mappings and cable comparisons belong in `docs/hid-research.md` and
  `docs/x52-dropout-analysis.md`, with observed/repeatable/verified evidence status.

Next: finish annotated control and fault captures, compare repeated old/new cable
traces and derive a tested signature only if supported by evidence. In parallel,
validate exported profile drafts in Logitech's editor and Battlefield. Current
authoring supports physical buttons, six vendor modes, standard keyboard keys and
left/right mouse outputs; hats, axis programming and other mouse actions are not
exported until equivalent format samples have been validated. Axes and all game
files are left unchanged. Default vendor mouse/scroll assignments and shift
fallbacks are retained unless a button receives an explicit override. Export does
not activate profiles or claim standalone joystick-memory programming.

Profile-authoring checks passed in Release and in a separate Debug validation
output directory. The normal Debug executable remained locked by the running
debuggee; Visual Studio needs a stop/rebuild before F5 uses this final revision.
Actual Logitech-editor loading and Battlefield gameplay remain unverified.

## MFD settings and analogue noise filtering — 2026-09-21

Added MFD & inputs to the existing Visual Studio application. Native checkboxes
control clutch mode, latched clutch, MFD/LED on/off and each clock's 12-hour format;
percentage fields control brightness. The independent settings worker uses only
commands traced from the installed original-X52 CPL, checks 06A3:075C and driver
8.0.116.0, validates returned lengths/ranges and reads back every change. Opening
the tab only queries settings. Errors disable editing until refreshed. This does
not replace the driver, program onboard memory or activate a PR0 profile.

All seven settings passed a live change/read-back/restore/read-back test on the
connected X52. Original settings restored: clutch enabled, latch off, both lights
100%, all clocks 24-hour. These are driver readbacks; physical display appearance
and restart persistence still require user observation. LED packing was corrected
during validation (index WORD first, percentage WORD second); the driver rejected
the initial incorrectly packed request without changing brightness.

Four independently switchable filters target explicitly identified throttle.main,
throttle.rotary_side, throttle.rotary_top and throttle.slider controls. Defaults
are 45 ms exponential smoothing and one raw-count jitter tolerance. Settings persist
in input-filters.json. Original raw/decoded evidence remains separate; the live
Normalized column and safe internal state consume filtered values. Safety remains
after the filter, so transport failure goes directly to the selected failsafe.
There is still no independent game-output runtime; this does not smooth axes read
directly by Battlefield. Buttons, hats and stick axes are untouched.

Corrected the photo anchors from the user's physical labels: E is the top blue
button, D the side blue button and I/clutch the lower rotary-centre button. No HID
usage IDs or existing learned assignments were silently permuted.

See docs/mfd-settings.md for driver-command provenance and supported limits.

Final verification: Debug and Release rebuilt in the canonical out/x64 paths;
core tests pass in both. Debug read-only MFD and BF3/BF4 import/export tests pass.
All seven live driver setting roundtrips passed in Release. The updated Debug
application was started via the already-open Visual Studio DTE for the existing
SaitekX52Mapper.sln, with its existing startup project. No alternate executable
was launched. The inspector connected and reached Running after initialization.

## Settings UI correction — 2026-09-21

Following physical feedback, MFD backlight is an on/off checkbox only. Intermediate
values were accepted by the driver but the user observed no brightness change;
previous roundtrip tests established driver readback, not visible LCD dimming.
The working button LED brightness control is now a native 0..100% slider with a
live value label; release commits and verifies the setting. The tab is MFD & LEDs.
Three repetitive clock checkboxes were replaced by a clock selector and a single
explicit 12-hour/24-hour format choice. Selecting a clock does not write settings.
The UI explains that time zones remain configured through Logitech's panel.

Noise filtering moved to Live inputs below the raw report display, with bottom
anchoring on resize and extra height allocated to the live table. Clutch language
now describes using I for Logitech profile selection and press-to-latch behaviour.
I is available in PR0 authoring; exports using it explain that Logitech profile
selection must be off. Pinkie stays reserved. No device settings are automatically
changed by this correction.

Verification for this correction: canonical Debug and Release builds passed,
with core and BF3/BF4 profile tests in both, including I-button authoring/export.
Read-only hardware check retained the user's settings (MFD driver value 50,
LED 10%, clutch on, latch off, clocks 24-hour). No settings roundtrip writes were
performed for the UI correction. Restarted Debug through the existing Visual
Studio DTE and startup project after the user stopped the previous debuggee.

## Live LED brightness slider — 2026-09-21

LED brightness now responds to thumb tracking, clicks and keyboard adjustments,
without waiting for release. A single pending value is overwritten by the newest
position while one asynchronous driver write/readback is in flight. A 33 ms timer
services completion and the final pending value. The active trackbar keeps mouse
capture and its chosen position; older readbacks cannot move it backward. Other
settings wait for the brightness update to settle, and failures clear the queue.
The existing original-X52 identity/version checks and write verification remain.

Verification: Debug and Release builds and core tests passed; restarted the updated
Debug app through the existing Visual Studio solution. Live dragging appearance
has not been visually verified in this turn; no device settings were changed by
the build/test commands.

## Rear throttle wheel choices - 2026-09-21

The physical-control picker now lists Scroll wheel up, Scroll wheel down, and
Scroll wheel click / right mouse button (RMB) explicitly under Button, Mouse /
scroll, and All control types. Each fixes the corresponding part automatically.
The existing throttle.scroll ID and Wheel up/down/press parts are preserved for
saved links and duplicate detection; the whole-wheel scalar choice remains in
Mouse / scroll. The existing photo annotation identifies the rear wheel area
and explicitly says that the control is on the far side of the photograph.
No HID numbers are inferred, and no driver or mouse capture behaviour changes.

Verification: canonical x64 Debug and Release builds passed, as did core tests
in both configurations. Regressions cover all three Button choices, Mouse-filter
identity, unit filtering, HID type compatibility, saved-link roundtrips and
 duplicate rejection. Physical wheel input capture has not been tested this turn.

## Mapped stick/throttle observations (log only) - 2026-09-21

Read the user's saved links: all 44 controls assigned, 24 Stick and 20 Throttle.
The user describes drift lock at the last angle and unresponsive stick buttons,
with throttle still responsive, and explicitly requests logging only for now.

Added a separate activity observer using those assignments and unfiltered raw
values. It logs a candidate after at least 250 ms of unchanged freshly sampled
stick values and repeated throttle activity. Throttle axis evidence requires
three counts or 2% of range, whichever is greater; button/hat changes count
without analogue thresholds. Each episode logs a start and end, raw state,
mapping snapshot and unchanged-since timing in the existing session JSONL log.
Stick movement ends it; invalid/stale input, closure or remapping interrupts it.
The observer never invokes recovery, changes safe values or increments confirmed
or user-marked dropout counts. It can also log normal intentional stick holds;
it is not a verified protocol signature or a measurement of physical fault duration.
Faults shorter than 250 ms or without throttle activity may be missed.

Connection health displays group coverage, raw axis changes, button/hat changes,
last-change ages and log-only observation status. Capture pre-roll now targets ten
seconds, bounded by 16 MiB encoded bytes and 4096 records. Capture analysis includes
per-control ranges/transitions/unchanged runs and report gaps/errors; invalid
report discontinuities are not counted as observed physical movement.

Verification: x64 Debug and Release builds and core tests passed. Tests cover
frozen-stick/throttle activity, jitter rejection, single-episode start/end,
parser interruption, report-ID isolation, unchanged safety/dropout counters,
capture group summaries, timing and pre-roll time/count/byte bounds. No real
physical dropout was reproduced or claimed, and no hardware settings were written.

## Battlefield-style native interface - 2026-09-21

Restyled the existing Visual Studio application to follow the user's Battlefield
menu reference: left navigation with white active rows, uppercase Bahnschrift
headings, thin dividers, dark teal panels, muted text and a soft teal/red backdrop.
The wider main window retains all six pages, with common actions in the sidebar.
Tables, headers, native scrollbars, checkboxes, dropdowns, buttons, profile lists,
LED trackbar and physical-control popup use the shared BattlefieldTheme renderer.
Native control behaviour, focus and keyboard handling remain in place. No saved
mapping, device-setting, input-filter or log-only monitor behaviour was changed.

The procedural backdrop is presented with a Direct2D HWND render target (default
hardware-preferred rendering), with a GDI fallback if Direct2D is unavailable or
loses its target. Native child controls remain Win32/GDI; no continuous animation
or rendering thread competes with the HID worker. Background resources are cached
and rebuilt on resize. The GPU adapter/backend was not separately profiled.

Verification: existing solution built in x64 Debug and Release; core tests passed
in both. Inspected rendered previews of all six pages, the identification popup
with its photo marker, and an open clock dropdown. Fixed clipped action labels,
ampersand rendering and native scrollbar contrast during review. Restarted through
Visual Studio and returned the running Debug app to Live inputs. Hardware reads
continued; no device settings or control links were changed during the UI checks.
Preview artifacts are under out/theme-*.png (not packaged application assets).

## Custom scrollbar controls - 2026-09-21

Replaced the theme's non-client scrollbar overpainting after the user reported
white patches and redraw glitches. Tables, multiline text panels and the profile
list now have independent X52.CustomScrollbar windows. Each native content window
is clipped to its client rectangle; native non-client scrollbar pixels are excluded
rather than painted over. Native ranges/positions remain the content model.

The custom controls implement proportional thumbs with a minimum hit size,
hover/drag feedback, mouse capture, track paging/repeat, mouse-wheel forwarding,
arrow/Page/Home/End keyboard operations, and horizontal/vertical positioning.
They synchronize with content changes, resize/move and page visibility, and are
destroyed with their content. The old GetWindowDC scrollbar repaint path is removed.
No device settings, input mappings or dropout-monitor behaviour were changed.

Verification: canonical x64 Debug and Release builds and core tests passed.
New native-control tests exercise edit, report-list and list-box scrolling in both
orientations, full-range thumb geometry, track clicks, captured drags beyond the
bar, wheel forwarding, page hide/show, window clipping, resizing and destruction.
Visual checks covered live inputs, HID inventory and Learn controls. In the running
Debug app, the custom scrollbar moved the live table from row 0 to row 29 and back.
Visual Studio remains running the rebuilt app, on Live inputs. Preview artifacts:
out/custom-scroll-live.png, custom-scroll-inventory.png, custom-scroll-learn.png,
and custom-scroll-bottom.png.

## Scrollbar visual refinement - 2026-09-21

Removed the full-track XOR focus rectangle shown in the user's feedback. The
scrollbars now have a quiet two-pixel rail and a rounded six-pixel thumb, widening
to eight pixels on hover, focus or dragging (DPI-scaled). Keyboard focus is shown
by the thumb's pale cyan fill; dragging uses the brighter selected colour.
Small end insets keep the thumb away from the panel edges. Full-width hit targets,
range calculations, scrolling, mouse capture and keyboard handling are unchanged.

Verification: Debug/Release builds and core/native scrollbar tests passed. Inspected
running-app previews of the idle bar and a clicked/focused bar; the long rectangular
focus outline is gone. The updated Debug app is running through the existing Visual
Studio solution. Previews: out/refined-scrollbars.png and refined-scrollbar-focused.png.

## Buffered painting and eased scrolling - 2026-09-21

Added RAII memory-DC buffering for custom scrollbars, buttons, headers, selectors,
sliders, static labels/photo panels and native edit/list-box clients. List views
explicitly retain LVS_EX_DOUBLEBUFFER; the Direct2D main surface retains its own
presentation buffer and its GDI fallback now uses the same complete-frame helper.
Buffers are scoped to each paint, released after presentation, with direct-paint
fallback if allocation fails.

Wheel input over content or its bar, Shift-wheel, horizontal wheel, bar arrow/page
keys and track paging now ease for 140 ms on a 16 ms UI timer. Repeated input adds
to the pending destination. Partial wheel deltas accumulate; Windows wheel amount
and client-area-animation preferences are respected. Thumb position interpolates
continuously; native text/table/list vertical content remains line/row granular.
Dragging and Home/End stay immediate. Hiding, range changes, external scrolling
and destruction cancel animations; there is no idle animation loop.

Debug and Release x64 solution builds and core tests passed. Added native-control
regressions for deferred paging, wheel bursts/partial deltas, cancellation on Home
and hide, and buffered presentation/GDI resource release across repeated frames.
Inspected running-app Live inputs, HID inventory and physical-control picker
previews. Existing Visual Studio Debug app is running. No hardware settings writes
or game-output changes were made.

## Executable-local application data - 2026-09-21

DefaultDataDirectory now resolves GetModuleFileNameW and returns the executable's
sibling data directory. No AppData access/fallback remains in the runtime storage
path; working-directory changes do not relocate saved data. Existing startup/log
errors handle an unwritable deployment directory. Debug/Release data are separate.
Updated README, troubleshooting and filter-settings documentation.

Copied and SHA-256 verified all 24 legacy files (111,712,075 bytes), including all
44 learned controls, into out/x64/Debug/data. Seeded out/x64/Release/data with the
same learned-controls.json; no saved input-filters.json existed in the source.
Preserved diagnostic captures and old journals in Debug data. The verification
manifest is out/data-migration-manifest.json. Debug/Release builds and core tests
passed, including a new executable-relative path test with a changed working
directory. Launched the original Visual Studio Debug project, verified the new
journal is beside the executable and inspected the loaded mapping names.

Cleanup remains incomplete: automatic approval review rejected both recursive
legacy-folder deletion and a narrower cleanup of only hash-verified duplicate
files followed by empty-folder removal (blocked by policy; no specific reason).
The old C:/Users/KingJamesIX/AppData/Local/X52BattlefieldMapper folder remains as
a duplicate and is no longer used by the rebuilt application. No deletion occurred.

## Brightness redraw and resized backgrounds - 2026-09-21

Fixed live brightness readbacks repeatedly disabling/re-enabling MFD controls,
resetting unchanged checks/selections and alternating status text. State updates
are now conditional. Known settings remain editable during an asynchronous write;
other edits are serialized/coalesced by option, refresh requests are deferred,
and pending user edits are not overwritten by older readbacks. Driver operations
retain the existing identity/version checks, single-operation serialization and
brightness coalescing. Initial unavailable settings and errors still disable edits.

Themed buttons now use BS_OWNERDRAW so synchronous native state drawing also
reaches the themed renderer. Checkbox state and BN_CLICKED toggling are preserved
explicitly. Trackbar NM_CUSTOMDRAW suppresses default drawing as well. No-op check,
combo selection and slider-position updates avoid invalidation. Parent and child
backdrops share the same full-resolution raster. Resize regenerates that raster
before positioning/repainting children, and invalidates children only as part of
layout/background changes. Labels, checkbox surfaces and slider backgrounds blend
with the parent; fields and action buttons retain intentional panel fills.

Debug/Release solution builds and core tests passed. New ThemeTests cover 200
repeated unchanged updates without invalidation, checkbox activation/disabled
behavior, owner drawing, trackbar custom draw and pixel-aligned backgrounds across
three window sizes. Actual Debug app drag verification sent 30 native mouse moves
while maximized, then restored the window. Zero enabled-state changes in unrelated
MFD controls. Eight sampled UI regions (checks, refresh, clocks and explanatory
text) had zero changed pixels before versus during drag. Client-DC screenshots
avoid PrintWindow repainting away the issue under test. Inspected maximized and
restored captures. LED brightness started at 10 percent, varied during testing,
and was restored to 10 with a fresh driver readback. Other device settings were
not changed. Original Visual Studio Debug project remains running.

## Dependency installer archive - 2026-09-21

Created repository dist directory for vendor/dependency EXEs and future mapper
application installers. Copied the exact Downloads X52_HOTAS_x64_8_0_213_0.exe
(15,991,736 bytes). Source and destination SHA-256 match:
1661874a7aedaf610d230271aa37d037bab501a208ae875b47f012e6d81e9c1d.
Windows Authenticode reports Valid with Logitech Inc as signer. The executable's
9.20 version metadata belongs to its 7-Zip wrapper; no driver-component version
was inferred. Installer was neither executed nor modified. Added dist README and
SHA256SUMS, a root README link, and binary Git attributes for dist EXEs. No other
runtime installers or mapper installation package have been added yet.
Debug/Release solution builds and core tests passed; dist is not Git-ignored.

## Joystick-only Battlefield editor - 2026-09-21

User explicitly selected joystick view only. The importer previously parsed all
bindings, but the UI gated its action selector on keyboard/mouse PR0 output. The
page now filters device type 2 records and shows numeric joystick button codes,
axes, inversion, unassigned records and unknown encodings. Input choices come from
all saved physical HID identification links (including hats and axes), rather than
only the vendor PR0 button catalog. Battlefield numeric button/direction codes
are not assumed to be physical X52 HID usages. Axis=24/button=60 is unassigned;
button=60 with axis below 24 is an axis, avoiding loss of pitch/roll bindings.

Joystick assignments are persisted separately in bfN-joystick-authoring.json and
exported as JSON joystick plans with source device, context/action/slot, raw axis,
button, negate, mode and physical HID identity. The plan explicitly marks runtime
output unimplemented. Unassigned/unknown targets cannot be assigned. This page
no longer creates keyboard PR0 commands. Earlier keyboard draft files/PR0 exports
are preserved, and the low-level PR0 library remains for regression coverage.
Joystick PR0 encoding/runtime output remain future work; no invented vendor
joystick commands, game-file edits or profile activation were introduced.

Debug/Release solution builds and core tests passed. Tests cover mixed-device
imports, sentinel distinction, labels, inverted axes, exact plan fields, rejected
unsupported targets and duplicate mappings. Added --joystick-profiles read-only
integration checks. Actual files and the running UI both showed BF3=91 and
BF4=127 joystick records, with 44 learned X52 inputs. Jet Fire=button code 0,
Pitch=axis 7/negate 1 and Roll=axis 6 were observed in the saved files. Game file
SHA-256 hashes were unchanged across UI imports. Inspected the profile page with
BF4 jet pitch selected; no demonstration assignments were saved into user plans.
Original Visual Studio Debug app remains running. No in-game mapping test occurred.

## 2026-09-22 - Device-specific enhanced power management

Added Registry tweaks to the existing native Visual Studio application. It reads
present original-X52 USB instances (VID_06A3/PID_075C), displays the exact registry
path/value and supports disable/restore through a short-lived runas helper.
Only EnhancedPowerManagementEnabled is written; no HID child keys, global power
plans, other USB devices, ACL changes or automatic device resets are involved.
Original 0/1/absent state is preserved before writing in exe-local data/registry
JSON using CREATE_NEW; restore validates device identity and value. Both writes
verify readback. The main application remains asInvoker.

Debug and Release solution builds and core tests passed. New tests cover device
identity/path rejection, backup roundtrips and mismatches, DWORD type validation,
and write/readback/restore/deletion in an isolated HKCU test key. These tests do
not mutate HKLM. Visually inspected the new themed page in the running original
Visual Studio Debug application. Live readback on USB instance
6&1E5B6DD0&0&3 showed DWORD 0; its backup records original DWORD 1, created at
2026-09-22T00:28:17.414Z. The app reported helper success. At final inspection
the physical device was disconnected; reboot/reconnect and actual dropout
improvement remain unverified. No automatic reset or in-game test was performed.

Both user-supplied AVSIM and Reddit threads are linked in docs/power-management.md.
Similar X52 symptoms recur in those reports, but results for the registry workaround
are mixed and connector/solder issues are also reported. Do not infer a confirmed
cause or failure prevalence. Stick inactivity remains log-only suspicion.
