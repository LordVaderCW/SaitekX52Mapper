# PS28 HID research log

Status vocabulary: UNKNOWN = no measurement; OBSERVED = one supported observation;
REPEATABLE = reproduced independently; VERIFIED = repeatable and checked against
alternative controls/faults. A descriptor usage name is not a physical assignment.

| Control / finding | Report byte / mask | Range | Status | Evidence |
|---|---|---|---|---|
| Windows input buffer | Length 15; ID byte 0 = 0 | 15 bytes | OBSERVED | Live read-only Debug and Release probes |
| Desktop usages X/Y | Parsed by HidP, no hard-coded offsets | 0..2047 | OBSERVED | HID value caps + live data |
| Desktop Rz | Parsed by HidP | 0..1023 | OBSERVED | HID value caps + live data |
| Desktop Z/Rx/Ry/slider | Parsed by HidP | 0..255 | OBSERVED | HID value caps + live data |
| Hat usage | Parsed by HidP | 1..8; zero null observed | OBSERVED | HasNull + live packet |
| Button usages | Parsed by HidP | 1..34 | OBSERVED | Button caps |
| Page 5 usages 0x24/0x26 | Parsed by HidP | 0..15 | OBSERVED | Meaning UNKNOWN |
| Stick/throttle ownership | Unknown until learned | — | UNKNOWN | Operate one control at a time |
| Stick-disconnect signature | None established | — | UNKNOWN | Fault captures required |

Learn Mode records the baseline, changed bytes/bits and changed HID usages. It
retains evidence per candidate and persists chosen names/groups as OBSERVED, never
automatically VERIFIED. Multiple changed controls are shown rather than selecting
the first noisy axis. Byte indices and bit indices are zero-based.
