# Identify HID controls using the physical X52

Double-click any row on **Live inputs**, select it and press Enter, or use
**Identify selected input**. In **Learn controls**, start the baseline, operate one
physical control, choose its changed HID usage, and click **Identify candidate**.
Both paths open the same native popup. Reports continue on the input worker.

Selecting a physical control adds a small red circle over its location in the
photo: a 3-pixel antialiased outline with no fill. Stick X/Y/twist circle the grip;
buttons, hats and toggles use smaller areas. Markers follow the image crop and
scale, update when the selection/photo changes, and disappear when no control is
selected. Trigger/pinkie selections choose a side photograph. Hidden controls
are labelled as hidden or as approximate far-side areas rather than presented as
visible buttons. These markers identify physical locations, not proven HID links.

The popup offers actual manufacturer photographs, a hardware-unit filter, a type
filter and a named physical-control list. It includes both trigger stages, the
covered fire button, A/B/C, pinkie, two grip hats, mode selector, T1..T6, throttle,
two rotaries, slider, D/E/I, throttle hat, mouse controls, wheel and MFD buttons.
Choose a direction/position for hats or switches exposed as separate HID buttons.
Choose Whole hat for an aggregate hat/scalar value. This does not redefine its
descriptor decoding or guess the direction-value encoding.

The throttle's rear scroll wheel has separate **Scroll wheel up**, **Scroll wheel
down**, and **Scroll wheel click / right mouse button (RMB)** choices in both the
Button and Mouse / scroll filters (and All control types). Selecting one fixes its
direction automatically. Existing `throttle.scroll` links keep their original
`Wheel up`, `Wheel down`, or `Wheel press` identity, so changing filters cannot
create duplicate links. The whole-wheel scalar choice remains under Mouse / scroll.
The photo circle marks the rear wheel's area; the wheel is hidden on the far side.

The live value, observed range and latest transition help check that the selected
row responds to the operated control. Short button transitions are retained by
the worker even when they occur between UI refreshes. Some mouse/MFD features may
be handled by the device or Saitek software and never appear in this collection.

Save link records a stable physical-control ID, position, display name and logical
Stick/Throttle group. Learning-backed links are OBSERVED; other manual links are
USER_ASSIGNED. Neither means VERIFIED. Existing free-text learned names remain
loadable. Duplicate links to the same physical control/position are rejected with
the existing HID identifier. Clear an incorrect existing link from its own popup,
then assign the intended row. Cancelling does not queue a save.

References (physical inventory, not HID-number mappings):

- [Manufacturer X52 guide, product tour pp. 3-4](https://www.logitech.com/assets/65328/2/x52-hotas.pdf)
- [Original Saitek X52 product description](https://www.saitek.com/uk/prod/x52.html)
- [Manufacturer product gallery](https://www.logitech.com/en-us/shop/p/x52-space-flight-simulator-controller)

Photo provenance and rights are documented in assets/README.md. No X52 Pro mapping
or image is used. The catalog's location-based hat/rotary names intentionally avoid
equating a printed/driver numbering convention with a specific HID usage.
