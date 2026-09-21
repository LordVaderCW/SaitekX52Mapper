# Mini-DIN dropout evidence

## Current status

All 44 current HID inputs have now been linked by the user: 24 Stick and 20
Throttle. The reported failure holds the last stick angle (drift lock) and makes
stick buttons unresponsive while the throttle can remain responsive.

The user's current instruction is **log only** for stick/throttle comparisons.
The monitor records `stick_unchanged_throttle_active` when fresh stick raw values
have not changed for at least 250 ms and at least two reports show throttle
activity, with another throttle change at the time of logging. Throttle button/
hat transitions count directly; an axis must move at least three raw counts or
2% of its logical range, whichever is larger, from its observation anchor.
Any stick raw change ends the observation. Invalid reports, stale transport,
session closure, or changed links interrupt it. Each episode logs one start and
one end with raw state, mapping snapshot and timing. These events are written to
the session event log even when a full capture is not armed.

This is an activity observation, **not a verified dropout signature**. Holding the
stick still intentionally can produce the same log. Brief faults below 250 ms,
faults without throttle activity, or partial freezes with other stick controls
still changing can be missed. No automatic suspension, neutralisation, reopen,
or recovery is triggered by these observations. Existing manual recovery and
transport-failure handling are separate.

Connection health shows grouped raw-axis and button/hat transition counts and
last-change ages. Full captures retain up to ten seconds of pre-roll, capped at
16 MiB of encoded records and 4096 entries. Exports summarize every control's
raw range, transition count and longest unchanged run, plus report gaps/errors.
Neither analogue noise nor an unchanged value alone proves responsiveness.

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
3. Start disconnect capture while controls work. Up to ten seconds of preceding records are kept.
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
