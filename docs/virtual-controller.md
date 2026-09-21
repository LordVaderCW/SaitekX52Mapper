# Virtual controller boundary

No virtual device is created in Milestone 1. Internal safety values are diagnostics,
not output to a game. Backend selection/research is deferred until the input layer
and actual PS28 behavior have been verified, as required by the brief.

Future IVirtualController ownership must be independent from physical HID sessions.
Physical failure changes submitted values; it must not unplug the virtual device.
Start with NullVirtualController for mapping tests. Before adopting a real backend,
document its support status, signed runtime, license, device model, install/uninstall
and limitations. Never silently install a driver or fabricate an unsigned backend.
