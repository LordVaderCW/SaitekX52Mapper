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
