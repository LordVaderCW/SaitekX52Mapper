# X52 Battlefield Mapper — canonical project record

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

## Hardware and established findings — 2026-09-21

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
