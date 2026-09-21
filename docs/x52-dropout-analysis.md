# Mini-DIN dropout evidence

## Current status

2026-09-21 update: Logitech driver 8.0.116.0 and profiler 8.0.213.0 are installed.
The user reports working LCD lighting, brighter LEDs and throttle mouse/scroll
controls. Apparent improvement was followed by a very brief ("microscopic")
dropout reported by the user. No measured duration or synchronized raw-report
trace is available for that recurrence. A cable fault remains a hypothesis;
the driver change cannot be called a complete fix. Continue annotated captures
and controlled old/new cable comparison alongside profile-authoring work.

No repeatable PS28 stick-side disconnect signature is established. No electrical
reset command is known. Software handle reopen has a separate test path and must
not be counted as a physical cable dropout or electrical reset.

The 5 m Mini-DIN configuration and shorter replacement cable are supplied test
conditions, not automatically detectable by this application.

The user reported a manually observed dropout of approximately **0.5 seconds** on
2026-09-21. The first manual marker (14:53:17 UTC / 15:53:17 UK) predates the saved
raw trace, which begins at 14:53:52 UTC. That incident's HID behavior and exact
duration therefore remain unmeasured. See `docs/testing.md` for the saved artifact
and movement ranges. No stick/throttle ownership was assigned during that capture.

## Capture procedure

1. Learn the main stick axes, a stick button and at least one throttle control;
   assign each control to its observed Stick or Throttle group.
2. In Connection health, enter cable length and conditions in the capture label.
3. Start disconnect capture while controls work. The preceding 200 records are kept.
   In the revised build, Mark stick dropout also starts a capture from pre-roll if
   none is active, so an unexpected fault can be marked without arming beforehand.
4. Operate a learned throttle control while reproducing the already known fault.
   Mark stick dropout only when you physically observe it. Keep moving the throttle
   so continued activity can be distinguished from unchanged values.
5. When the stick physically responds again, mark stick returned. Several valid
   reports are required before its internal safe state is restored.
6. Stop/save capture. Preserve several separate old-cable and new-cable exports.
7. Compare marker intervals, raw/previous packets, bit changes, invalid reports,
   device events, throttle/stick changes and recovery timing. Manual marker timing
   includes reaction delay. No USB-removal event does not prove continuous enumeration.

The export's analysis contains marked intervals and observed changes; it does not
automatically promote any packet pattern to a dropout signature. Stationary stick
controls, neutral axes and released buttons alone are insufficient evidence.

## Cable comparison

| Evidence | Old 5 m cable | Short replacement |
|---|---|---|
| Capture files | Movement capture saved; fault interval not established | Not yet tested |
| Observed physical dropouts | One user-reported (~0.5 s); raw fault interval not retained | Not established |
| Exposure duration | Not measured | Not measured |
| Continued learned throttle activity | Not established | Not established |
| Repeatable raw signature | UNKNOWN | UNKNOWN |
| Recovery timing / errors | Pending | Pending |

Only calculate dropout frequency over known observation time and clearly distinguish
user-marked events from future automatically detected events. Keep all raw evidence
for any proposed signature, including healthy controls that resemble that signature.
