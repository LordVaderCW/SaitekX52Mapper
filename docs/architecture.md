# Architecture

Physical PS28 -> read-only HID worker -> descriptor controls -> normalized X52State
-> health/freshness gate -> internal safety state. Native UI consumes immutable
snapshot copies. Future mapping/output layers will consume the safe normalized
controls; physical device lifetime must never own a virtual controller lifetime.

The input worker owns enumeration, HID handle, parser and one overlapped operation.
It processes commands between reads. Each pending read waits at most 20 ms before
checking commands/stop/freshness; it is not cancelled just because nothing changes.
Cancel/close drains the operation before freeing its storage. An unresponsive
kernel driver may still delay final cancellation completion; storage is not freed
while the OS can access it.

The UI owns all HWNDs and GDI fonts. Device-interface notification registration is
RAII-managed. Notifications are forwarded to the input worker; confirmed matching
path removals close the reader, while unrelated HID events do not interrupt input.

Events are serialized to a bounded asynchronous writer queue. Raw reports are held
in a 200-record pre-roll and copied into an explicit bounded capture only while
recording. Export transfers capture ownership to a temporary worker, keeping disk
serialization off the reader. Input, decode/safety timing and events use QPC; UTC
timestamps are for human correlation. Displayed processing time excludes USB,
Windows queuing, capture serialization, rendering and any future output backend.

Use Microsoft documentation for parser and I/O contracts:

- [HID API and descriptor parsing](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-api)
- [Synchronous and asynchronous I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/synchronous-and-asynchronous-i-o)
- [Cancellation must finish before releasing buffers](https://devblogs.microsoft.com/oldnewthing/20110202-00/?p=11613)

No Raw Input, DirectInput or XInput comparison is needed for the first descriptor
inspection milestone. Those remain later comparison tools, not hidden hooks.
