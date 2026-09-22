# Battlefield joystick / joypad plans

Use the existing SaitekX52Mapper Visual Studio project. In **Battlefield profiles**:

1. Import Battlefield 3 or 4. The app reads the Windows Documents location:
   `Battlefield 3/settings/PROF_SAVE_profile` or `Battlefield 4/settings/PROFSAVE_profile`.
2. The page shows only device-type 2 joystick/joypad records. Keyboard and mouse
   entries are excluded. Filter by game context (jet, heli, infantry, etc.).
3. Select an X52 mode, a physical input from your saved HID identification links,
   and an assigned Battlefield joystick binding. The input list includes identified
   buttons, hats and axes. Identify missing controls on Live inputs and reimport.
4. Assign / replace saves the plan. Removing an override removes that plan entry.
5. Export joystick plan writes a separate JSON file in `data/profiles` beside the
   executable. These plans do not yet produce joystick output and are not PR0 files.

The binding labels retain Battlefield's numeric axis/button codes and inversion.
A button code must not be assumed to be the corresponding X52 HID usage or button
number. Codes representing directional inputs are left numeric until verified.
For the BF3/BF4 format handled here, axis=24/button=60 is unassigned; button=60
with an axis below 24 is an axis binding, including inverted pitch. Unassigned
and unknown encodings remain visible but cannot be assigned as output targets.

Authoring files are `data/bf3-joystick-authoring.json` and
`data/bf4-joystick-authoring.json`. They store each mode and learned HID link plus
context, action, slot, type, axis, button and negate. Reimport resolves saved
binding identities against current game settings and reports missing/unsupported
entries. This plan format explicitly marks runtime output as unimplemented.
Mode labels are planning slots; no runtime mode inheritance is implied.

The game settings are never modified by importing, assigning or exporting plans.
No profiler or firmware profile is activated. Earlier keyboard/mouse authoring
files (`bfN-authoring.json`) and PR0 exports are left intact and are not loaded in
this joystick-only UI. The previous PR0 parser/export library remains available
for regression tests; a verified joystick-output encoding is still required
before joystick plans can become Logitech profiles.

Observed in the user's files on 2026-09-21: BF3 contains 91 joystick records;
BF4 contains 127. Both contain jet Fire button code 0 and inverted Pitch axis 7.
These are saved-file observations, not in-game verification or physical X52
control correspondences. The read-only integration check is:

```powershell
./out/x64/Debug/X52Tests.exe --joystick-profiles
```
