# X52 USB power-management experiment

**Registry tweaks** reads the connected original X52 USB instance (`06A3:075C`).
Opening or refreshing the page only reads the registry. **Disable for this X52**
starts a short-lived copy of the application using Windows administrator approval.
The main inspector continues to run without elevation.

The helper changes just this DWORD to `0`:

```
HKLM\SYSTEM\CurrentControlSet\Enum\USB\VID_06A3&PID_075C\<instance>\Device Parameters
EnhancedPowerManagementEnabled
```

Before writing, it saves the original value (including whether it was absent) in
`data/registry/x52-power-<instance>.json` beside the executable. An existing backup
is validated and never overwritten. **Restore original value** restores that
backup, deleting only this value if it was originally absent. Both operations
verify registry readback. Keep the backup with the executable's data folder.

The selected instance must still be connected. Other devices, HID child keys,
global power plans, selective-suspend settings and registry permissions are not
changed. The application does not reset USB or restart Windows. After applying,
reconnect the X52 to the same port or restart Windows before evaluating results.
Moving USB ports or reinstalling drivers can produce a different instance or
reset settings; refresh the page to inspect the actual current value.

## Evidence and limits

The user supplied this [AVSIM discussion](https://www.avsim.com/forums/topic/467801-woohoo-windows-joystick-disconnects-conclusively-solved/)
as the proposed workaround. It describes the device-specific `1` to `0` change
and rebooting. Follow-up results are mixed.

The supplied [X52 Reddit thread](https://www.reddit.com/r/hotas/comments/fizpgu/x52_hota_setup_disconnects_joystick_occasionally/)
describes similar stick-only disconnects and recentering. One contributor reports
temporary improvement from the registry change, followed by recurrence; other
reports concern connectors and solder joints. These are user reports, not a
controlled diagnosis or a measurement of failure prevalence.

This option tests one possible cause. A successful registry write does not prove
the driver has reloaded the setting or that the dropout is fixed. Continue using
Connection health captures and manual dropout markers. Stick inactivity while
the throttle changes remains a log-only suspicion, not a confirmed fault.
