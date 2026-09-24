# Validation record

## Battlefield / Logitech profile authoring â€” 2026-09-21

Final Release x64 build and core/profile integration checks passed. Final Debug
build and the same checks passed with `OutDir=out/debug-validation/`, because
the running Visual Studio debuggee locked the normal Debug executable (LNK1168).
The existing solution/project contains all changes; stop debugging and rebuild
to update the normal F5 target. No running process was forcibly stopped.

Core checks cover Battlefield field grouping, duplicate rejection, unbound and
joystick-output exclusion, Space scan-code to HID conversion, PR0 quoting/nesting
roundtrips and malformed input rejection. `X52Tests.exe --profiles` additionally
reads the installed original-X52 template and real BF3/BF4 files, imports 259/372
records, creates trigger-only drafts and reloads the exported UTF-16LE files.
It checks duplicate mode/control rejection and retention of stock mouse controls.
Both game-file SHA-256 values and the installed template were unchanged afterward.

Generated examples are `out/profile-research/BF3-Trigger-DRAFT.pr0` and
`BF4-Trigger-DRAFT.pr0`. They contain a single Mode-1 Trigger override to the
existing jet Fire keyboard binding, inherited through the stock mode fallbacks.
They are not complete control layouts. Logitech editor acceptance, actual emitted
key press/release behaviour, in-game operation and live UI inspection are not yet
verified. No profile was activated automatically, and no driver/device writes
were performed by the exporter.

## Physical-control photo highlights â€” 2026-09-21

Debug and Release x64 builds through the existing solution and core tests passed
with the new photo-circle rendering. The first Debug link was blocked by the
running inspector (LNK1168); rebuilding succeeded after the user closed it.
Source photos were inspected to place the image-relative anchors. Live UI
automation was not used for this change; on-screen circle placement remains a
user visual check. Markers are presentation-only and do not alter saved mappings.

## Existing Visual Studio project integration â€” 2026-09-21

`tools/Build.ps1 -Configuration Debug` and `-Configuration Release` now build
the original `SaitekX52Mapper.sln` and `SaitekX52Mapper.vcxproj` with Visual Studio's
MSBuild. Both x64 builds and core tests passed after the migration. The application
outputs `SaitekX52Mapper.exe` with PDB symbols; all listed source/resource paths
resolve and every inspector implementation file is included in the original
project. Its original project GUID and startup identity remain unchanged.
Debugger settings launch `$(TargetPath)` with the repository as working directory.
This verification used build tools, not an automated F5 action in the open IDE.

The physical picker tests cover catalog identity, axis/button/hat compatibility,
legacy and catalog persistence, duplicate rejection, correcting and removing links.
The preceding live Debug probe passed isolated assignment/clear commands and ten
software reopen cycles: 444 reports, 44 controls, 15-byte reports, no reported
error. Those test labels were confined to `out/hardware-probe`; they are not
physical mapping evidence or user-profile assignments.

## Automated checks

`X52Tests.exe` uses explicit assertions that stay enabled in Release. Coverage:

- Normalization endpoints, signed HID values, byte/bit transition masks.
- Five consecutive valid reports, 100 ms analog blending, explicit hold-last.
- Separate stick/throttle neutralization, hats neutral and full-transport failsafe.
- Invalid-report validation reset, 100 repeated synthetic transport recoveries.
- Stationary input never classifies a dropout; freshness expiry withholds state.
- One report ID cannot validate another report ID's controls.
- Learned-control persistence, wrong device identity, malformed/deep JSON and
  invalid input groups.
- Capture analysis reports observed group changes and user-marker timing without
  inventing a verified signature.
- Unsupported hardware recovery does not claim support or execute a reset.

`X52Tests.exe --hardware` opens the real PS28 read-only, captures reports and runs
ten independent close/reopen/validate/restore cycles. A Release run on 2026-09-21
passed, with 340 reports, 15-byte buffers, 44 decoded controls and no reported error.
Those are software reopen cycles, not ten physical cable disconnections.

Final Debug hardware run: **500 reports, 44 controls, zero reported error, ten
software reopen cycles passed**, plus automatic pre-roll capture on a synthetic
manual marker and validated return with unassigned controls. Synthetic markers
are explicitly labelled AUTOMATED TEST and do not count as observed hardware faults.
Both x64 configurations build warning-clean with warnings treated as errors; core
tests pass in both. The first marker test polled too early during concurrent builds;
it now waits for observable state/export completion with a bounded timeout.

## Live native UI and user exercise

The native Live inputs, Learn controls and Connection health pages were exercised
against the attached device. The UI continued updating during read activity and
capture export. Visual inspection found and prompted fixes for JSON newlines and
missing learned-axis raw ranges. The rebuilt Release app was launched, but final
computer-use inspection was stopped by the user's physical Escape key. No further
UI automation was performed after that signal.

The user performed a physical movement exercise and reported a manually observed
dropout of approximately half a second. The event log has a manual marker at
14:53:17.363 UTC. Capture began at 14:53:57.913 UTC with pre-roll starting
14:53:52.287 UTC, so that **initial marker predates the retained raw trace**.
Whether another dropout occurred within the trace is not established yet.

Saved artifact (outside the source tree):
`%LOCALAPPDATA%/X52BattlefieldMapper/diagnostic-5851572952138.json`.
It contains 11,756 reports through 14:58:23.595 UTC, zero invalid decoded reports,
one user-triggered software reopen and no retained Windows removal event.
It does not prove that no stick-side fault occurred.

X and Y descriptor usages both covered 0..2047; Rz covered 0..1023. Therefore the
whole trace is not sufficient to uniquely associate just one changed field with
left/right movement. Additional isolated control trials are needed. Page-1 Z/Rx/Ry
also varied over small ranges; these changes alone do not establish physical names.

## Hardware acceptance still required

Repeated physical USB unplug/replug tests, individually named physical controls,
annotated stick-side failures with known throttle activity, repeated signature
comparison and replacement-cable comparison remain pending. No game, virtual
driver or anti-cheat tests were performed.

## MFD controls, filtering and photo corrections — 2026-09-21

Canonical Debug/x64 and Release/x64 solution builds passed with /W4 /WX. Both core
suites passed, including filter jitter, movement, endpoint, bypass, reconnect,
report-rate independence and failsafe precedence tests. Debug --mfd read the
connected device successfully; Debug/Release --profiles passed for both real BF
settings files. Release --mfd-roundtrip changed and restored all seven supported
settings with exact driver readbacks. Final state: clutch=1, latch=0, MFD=100,
LED=100, three 12-hour flags=0. An initial incorrectly packed LED request was
rejected; after tracing the caller and fixing WORD order, the full test passed.
No visual hardware change or persistence through power cycles is claimed.

Release --hardware passed ten software reopens, assignment/clear and capture
checks: 360 reports, 15 bytes, 44 controls, no reported error. This used an isolated
probe directory and did not change the user's learned mappings. The original
Debug output was rebuilt after the user stopped debugging, then started through
the existing Visual Studio solution using its DTE Debugger.Go. No Computer Use
was used. Physical MFD appearance and manual UI interaction remain user checks.

## Settings layout follow-up — 2026-09-21

Debug and Release /W4 /WX builds and core/profile tests passed after moving
filtering to Live inputs, replacing LED percentage entry with a native trackbar,
removing MFD percentage entry, and changing clock format editing to a selected
clock plus 12/24-hour dropdown. Added I-button availability and PR0 export checks;
pinkie remains reserved. Read-only MFD query showed the user's MFD=50/LED=10
settings unchanged. No new device writes were used to test these layout changes.
The updated Debug app was started through the existing Visual Studio solution.
Manual slider/clock UI interaction remains to be observed by the user.

## Live LED dragging — 2026-09-21

Canonical Debug and Release builds and core tests passed. Updated Debug started
through the existing Visual Studio solution. The settings transport/commands are
unchanged from the previously verified driver roundtrip; this revision changes
UI scheduling to a 33 ms, one-in-flight/latest-pending mechanism. No additional
hardware setting writes were performed during this build verification. Physical
brightness tracking during a real drag remains a user observation.

## Log-only mapped activity monitor - 2026-09-21

Canonical x64 Debug and Release builds and core tests passed. Synthetic cases
cover frozen stick plus moving throttle, button-only throttle evidence, noise
rejection, episode start/end, invalid reports, report-ID isolation and separation
from recovery actions. Capture tests cover per-control activity, report gaps and
pre-roll duration/size/count bounds. Real in-game drift lock remains unverified;
normal intentional stick holds can generate the same activity observation.

## Battlefield-style UI - 2026-09-21

Debug/Release builds and core tests passed. Native window-rendered previews were
inspected for Live inputs, Learn controls, Connection health, HID inventory,
Battlefield profiles, MFD & LEDs, the photo identification dialog and an expanded
clock selector. Tested page switching and opening/cancelling the picker without
saving. Settings pages performed readback only; no brightness/clock/clutch writes.
Direct2D is hardware-preferred with GDI fallback; GPU backend was not profiled.

## Custom scrollbar regression - 2026-09-21

Both x64 configurations built and passed the core suite including ScrollbarTests.
The tests create real, isolated Win32 edit/list-view/list-box controls without
hardware interaction. They check thumb geometry, page/drag/keyboard/wheel input,
visibility, clipping, resize and target-first destruction. A page-hiding defect
found by these tests was corrected before final verification. Running-app previews
show custom scrollbars at the top and bottom of the live table and on text panels.

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

## 2026-09-23 - Themed device properties

Added Test and Deadzones and split LEDs from MFD in the existing solution. Test
shows driver-reported axes/buttons/hats. Deadzones reads all nine existing vendor
calibration envelopes with four independently movable, bounded handles. Explicit
Apply backs up the original in exe-local data/calibration-backups, detects stale
edits, updates only validated calibration-only content and uses the traced vendor
reload sequence. The Logitech-owned calibration file remains in its vendor
ProgramData folder; our own data remains beside the executable. Opening pages
never changes the user's tuned defaults. Unsupported custom curves/command
profiles are rejected. Added clock 2/3 GMT offsets, date format and clock 1 daylight
adjustment through the validated Logitech 8.0.116.0 adapter.

Debug/Release builds and core tests passed. Live nine-axis read succeeded; one
stick X centre-low unit changed, reloaded and was restored. Live extended MFD
roundtrips restored all four original options with readback. Native UI keyboard
edit/Apply-enable/Refresh-discard and unchanged calibration hash checks passed.
Normal/maximized screenshots were inspected; corrected DC mapping-state leakage
in the new buffered Test renderer. Detailed protocol evidence, tests and limits
are in docs/device-properties.md. No in-game test or new dropout diagnosis.

Mouse editing regression checks (2026-09-23): native hit testing, independent
coincident centre-handle drags, capture release, disabled input and unchanged
other axes pass in Debug and Release. The running Debug app passed mouse drag
and keyboard pending-edit/refresh-discard checks with unchanged calibration hash.
The persistent header no longer displays a changing report counter; live table
cells skip identical text and hidden properties views skip input-display updates.
No comparative CPU benchmark was performed.

2026-09-24 manual calibration reload: Debug/Release builds and core tests passed.
Optional --reload-saved-calibration refused the connected driver's empty calibration
path before any request/write. Valid-path reload/stale/pending integration branches
remain unexercised in this device state. No physical centring or game recovery is
claimed. No automatic fallback to the old on-disk file was introduced.
