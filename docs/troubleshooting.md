# Troubleshooting and operation

**No PS28 detected:** open HID inventory, check VID 06A3 / PID 075C, metadata errors
and the selected collection. Automatic retry runs once per second. X52 Pro is not
a supported replacement. Multiple matching controller collections require removing
extra targets in this first version; no arbitrary target is opened.

**Read failure:** the numeric Win32 error and system text are logged. State is
withheld while the worker cancels/drains, closes and retries. Reinitialize X52 Input
performs the same software recovery immediately. It does not power-cycle hardware.

**Stick marked failed but USB still present:** raw decoding continues. Learn
Stick/Throttle groups first. Unknown controls are conservatively withheld too.
Only click Mark stick returned after physically observing return; five valid
reports then restore the assigned stick controls with the configured analog blend.
No report-content signature is currently trusted to make that physical diagnosis.

**Neutral throttle is wrong:** choose the learned axis's neutral value (-1..1).
All axes normalize to -1..1 here; the later mapping engine will decide trigger vs
centred semantics. Hats always use -1 for neutral, buttons 0 for release.

**Changes appear without touching controls:** small analog jitter is expected as
an observation, not a fault. Learn Mode shows all changed usages; select the one
with deliberate response and repeat before promoting the assignment's status.

**Log/export:** Open data folder opens `data` beside `SaitekX52Mapper.exe`.
The executable folder must be writable. The application does not fall back to
AppData if this folder cannot be written. Each executable location has its own
data; copy the folder along with the executable when moving the application.
Exports are UTF-8 JSON. Raw reports are captured only when explicitly armed; regular
JSONL logs contain transitions/errors, not every identical report. Capture has a
64 MiB approximate serialized-data cap (JSON in-memory overhead is higher).
Mark stick dropout also begins capture automatically if none is active; the pre-roll
can retain a short fault that occurred just before the operator pressed the button.
At the cap, recording stops and the UI asks for export. Export also ends an active
capture. Export runs in the background; do not begin another export until it finishes.

**Configuration errors:** invalid JSON/device/schema/neutral/group/nesting produces
a visible error and a structured log. The original file is retained. Fix or move it
manually, then restart. No malformed assignment is silently promoted to a mapping.

**Freshness:** 2000 ms without reports withholds the internal state. This does not
establish a USB unplug or stick failure; some devices report only on changes.
Adjust the session freshness threshold using measured device behavior.
