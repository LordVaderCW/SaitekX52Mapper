# Development rules

Build native Windows x64 C++20 using Unicode APIs, /W4 and /permissive-. Use RAII,
explicit error handling, bounded buffers, and clearly owned handles. No raw owning
pointers, unexplained mutable globals or volatile synchronization. Keep hardware,
normalization, health/recovery, eventual mapping/output, and UI independent.

The user expanded scope on 2026-09-21 to Battlefield binding import and Logitech
PR0 profile authoring, using the installed original X52 driver/template. Keep the
inspector and evidence collection. Profile export is distinct from driver replacement
or autonomous profile activation. Never infer PS28 controls from X52 Pro. Record unknown/observed/repeatable/
verified status honestly. A valid HID packet is not proof that the stick is healthy.
Stationary input is not evidence of failure. Do not add protocol signatures without
repeatable captures. No output/feature writes until a legitimate PS28 command is proven.
The user also authorized MFD/LED/clutch settings on 2026-09-21. The bounded
Logitech 8.0.116.0 driver settings adapter is documented in docs/mfd-settings.md;
keep version/identity checks and readback. Do not extrapolate USB or Pro commands.

No injection, hooks, game patches, anti-cheat interference, service restarts, USB
disable/enable or driver installation. Software reopen is not an electrical reset.
The eventual virtual output lifetime must be independent of physical input.

Compile Debug and Release, run meaningful tests, and update PROJECT.md after every
important change. Preserve the existing GPL-3.0 LICENSE. Third-party code retains its
own license. Never report hardware interactions or Battlefield testing not performed.
