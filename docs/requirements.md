# PROJECT: X52 Battlefield Mapper

Build a professional native Windows C++ application called:

**X52 Battlefield Mapper**

The application is a hardware compatibility, diagnostics, and controller-remapping utility designed initially for the original Saitek X52 Flight Control System, model PS28.

The primary test games are:

- Battlefield 3
- Battlefield 4

The purpose is to determine exactly how the physical Saitek X52 controls are exposed by Windows, compare that with how Battlefield interprets those controls, and provide a clean remapping layer capable of translating the X52 into a conventional virtual game controller where necessary.

This is NOT a cheat, game modification, DLL injector, memory editor, input hook, or anti-cheat bypass.

The application MUST remain external to the game process.

---

# 1. TARGET HARDWARE

Primary device:

Saitek X52 Flight Control System

Known USB information:
```text
Model:              PS28
Manufacturer:       Saitek
VID:                0x06A3
PID:                0x075C

USB:
bcdUSB:             0x0200
Bus Speed:          Full Speed
Device Class:       Interface-defined
Configuration:      Bus Powered
Declared MaxPower:  100 mA

HID:
HID Version:        1.11
Interface Class:    0x03 HID
Endpoint:           0x81 Interrupt IN
Max Packet Size:    16 bytes
Polling Interval:   10 ms
Report Descriptor:  0x77 bytes
```

Do NOT assume X52 Pro mappings.

The X52 Pro is a different device/protocol and must not be silently treated as equivalent to PS28.

---

# 2. DEVELOPMENT REQUIREMENTS

Language:

**C++**

Use modern, professional C++.

Preferred standard:
```text
C++20
```

Platform:
```text
Windows 10
Windows 11
x64
```

IDE/project:
```text
Visual Studio
Native C++
```

Use:
```text
Win32 API
Windows HID API
SetupAPI
Raw Input where appropriate
DirectInput only where useful for comparison/testing
XInput only for virtual-output compatibility/testing
```

Avoid unnecessary frameworks.

Do NOT use:
```text
Electron
.NET
C#
Java
Python
Qt unless absolutely unavoidable
web-based UI
embedded browser UI
```

The application should be genuinely native.

---

# 3. ARCHITECTURAL RULE

Maintain a strict separation between:
```text
PHYSICAL INPUT
      |
      v
DEVICE/HID LAYER
      |
      v
NORMALIZED X52 STATE
      |
      v
MAPPING ENGINE
      |
      v
NORMALIZED OUTPUT STATE
      |
      v
VIRTUAL CONTROLLER BACKEND
```

Battlefield itself MUST NOT be hooked.

Forbidden approaches include:
```text
DLL injection
Battlefield executable patching
process-memory modification
DirectInput API hooking inside the game
XInput DLL proxying inside the game directory
anti-cheat bypasses
kernel manipulation intended to hide the mapper
code injection
packet manipulation
```

The program should behave like legitimate controller-remapping/accessibility software.

---

# 4. SOLUTION STRUCTURE

Create a clean Visual Studio solution.

Suggested structure:
```text
X52BattlefieldMapper/
|
+-- README.md
+-- PROJECT.md
+-- AGENTS.md
+-- LOCASKILLS.md
+-- LICENSE
|
+-- docs/
|   +-- architecture.md
|   +-- x52-ps28.md
|   +-- hid-research.md
|   +-- battlefield-mapping.md
|   +-- virtual-controller.md
|   +-- troubleshooting.md
|
+-- profiles/
|   +-- battlefield3.json
|   +-- battlefield4.json
|   +-- default.json
|
+-- src/
|   |
|   +-- app/
|   +-- device/
|   +-- hid/
|   +-- input/
|   +-- mapping/
|   +-- output/
|   +-- profiles/
|   +-- diagnostics/
|   +-- ui/
|   +-- util/
|   |
|   +-- main.cpp
|
+-- tests/
|
+-- tools/
|
+-- X52BattlefieldMapper.sln
```

Adapt this structure where technically justified, but maintain clear subsystem separation.

---

# 5. DOCUMENTATION FILES

## PROJECT.md

This is the canonical project record.

Maintain:

- purpose
- architecture
- hardware information
- design decisions
- known PS28 information
- discovered HID mappings
- Battlefield findings
- build configuration
- dependencies
- known issues
- completed milestones
- future work

Update PROJECT.md whenever an important discovery or architectural decision is made.

## AGENTS.md

Provide instructions for future coding agents.

Require:

- professional C++
- RAII
- no unsafe ownership
- no unexplained global state
- explicit error handling
- Windows API handles wrapped appropriately
- Unicode Windows APIs
- separation between hardware and UI
- no game injection/hooking
- no anti-cheat interference
- documentation of reverse-engineered findings

## LOCASKILLS.md

Record reusable technical knowledge discovered during development.

Examples:
```text
Windows HID enumeration
HID report parsing
SetupAPI device discovery
Raw Input
DirectInput comparison
XInput semantics
virtual controller backend
PS28 HID report format
Battlefield controller behaviour
```

Do not invent information.

Only record findings actually established during development.

---

# 6. DEVICE ENUMERATION

Implement a native Windows HID device enumerator.

Enumerate HID devices using the appropriate Windows APIs.

Identify the X52 using:
```cpp
constexpr USHORT SAITEK_VENDOR_ID = 0x06A3;
constexpr USHORT X52_PRODUCT_ID   = 0x075C;
```

Display:
```text
Manufacturer
Product
VID
PID
Device path
HID usage page
HID usage
Input report length
Output report length
Feature report length
Device connection state
```

Do not identify devices solely by product-name strings.

VID/PID are authoritative for the PS28 target.

---

# 7. HID INPUT MONITOR

Implement a dedicated HID reader.

The reader MUST NOT run on the UI thread.

Use an appropriate asynchronous mechanism such as:
```text
overlapped ReadFile
```

or another justified native Windows mechanism.

The reader should continuously capture input reports.

For development/debug mode provide:
```text
Timestamp
Report length
Raw hexadecimal report
Changed bytes
Changed bits
Decoded control
```

Example:
```text
14:32:16.442

RAW:
01 7F 82 40 33 00 00 10 00 00 00 00 00 00 00 00

CHANGED:
Byte 7
0x00 -> 0x10

Decoded:
Trigger Stage 1 = PRESSED
```

Never assume undocumented bits without evidence.

---

# 8. AUTOMATIC CONTROL DISCOVERY

Create an **Input Inspector**.

The user can press:
```text
Start Learn Mode
```

The application records a baseline HID state.

Then prompt:
```text
Press or move one control.
```

Detect the changed:

- byte
- bit
- axis
- hat
- button
- HID usage

Show the result.

Allow the user to assign a human-readable name.

Example:
```text
Detected change

Device:
Saitek X52 Flight Control System

VID/PID:
06A3:075C

Raw:
Byte 4, Bit 2

HID interpretation:
Button 1

Assigned control:
TRIGGER_STAGE_1
```

This facility is extremely important.

It should allow us to reverse engineer the entire physical controller without recompiling the program.

---

# 9. NORMALIZED X52 STATE

Create a strongly typed structure representing the controller independently from raw HID packets.

For example:
```cpp
struct X52State
{
    float stickX;
    float stickY;
    float rudder;
    float throttle;

    float rotary1;
    float rotary2;
    float slider;

    bool triggerStage1;
    bool triggerStage2;

    bool fire;
    bool buttonA;
    bool buttonB;
    bool buttonC;
    bool pinkie;

    bool toggle1;
    bool toggle2;
    bool toggle3;
    bool toggle4;
    bool toggle5;
    bool toggle6;

    // Additional controls as discovered.

    int povHat;
    int secondaryHat;
    int throttleHat;

    int mode;
};
```

This is illustrative.

Modify it based upon verified PS28 capabilities.

Do NOT blindly copy assumptions from X52 Pro documentation.

---

# 10. LIVE INPUT INSPECTOR UI

Build a native Win32 UI.

Main window should contain approximately:
```text
+----------------------------------------------------------+
| X52 Battlefield Mapper                                  |
+----------------------------------------------------------+
| Device                                                   |
|                                                          |
| Saitek X52 Flight Control System                         |
| PS28                                                     |
| VID 06A3 / PID 075C                                      |
| Status: CONNECTED                                        |
+----------------------------------------------------------+
| AXES                                                     |
|                                                          |
| Stick X       [==============|-------]  52%              |
| Stick Y       [===========|----------]  47%              |
| Rudder        [=============|--------]  50%              |
| Throttle      [===================|--]  82%              |
+----------------------------------------------------------+
| BUTTONS                                                  |
|                                                          |
| Trigger 1       PRESSED                                  |
| Trigger 2       RELEASED                                 |
| Fire            RELEASED                                 |
| A               RELEASED                                 |
| B               RELEASED                                 |
| C               RELEASED                                 |
+----------------------------------------------------------+
| RAW HID                                                  |
|                                                          |
| 01 7F 82 40 33 00 00 10 ...                             |
+----------------------------------------------------------+
| [Learn Input] [Mappings] [Output Test] [Diagnostics]     |
+----------------------------------------------------------+
```

Use standard native Windows controls where sensible.

The UI must remain responsive regardless of HID polling.

---

# 11. MAPPING ENGINE

Implement a generic mapping engine.

Do NOT hard-code Battlefield mappings into HID-reading code.

Use:
```text
X52 physical control
        ->
normalized input
        ->
mapping rule
        ->
normalized virtual controller control
```

Mappings must be data-driven.

Example:
```json
{
    "trigger_stage_1": "right_trigger",
    "fire": "right_bumper",
    "button_a": "button_a",
    "button_b": "button_b",
    "button_c": "button_x",
    "pinkie": "button_y"
}
```

Support:

- button -> button
- button -> trigger
- button -> axis value
- axis -> axis
- axis inversion
- dead zones
- sensitivity
- saturation
- axis curves
- axis splitting
- axis combination
- hat -> D-pad
- button combinations where appropriate

---

# 12. AXIS PROCESSING

Create a reusable axis pipeline.

Conceptually:
```cpp
raw
 -> normalize
 -> deadzone
 -> curve
 -> sensitivity
 -> inversion
 -> saturation
 -> output
```

Support independent settings for:
```text
Stick X
Stick Y
Rudder
Throttle
Rotaries
Slider
```

Do not introduce unnecessary latency.

---

# 13. PROFILE SYSTEM

Profiles must be stored outside the executable.

Use JSON.

Provide initially:
```text
default.json
battlefield3.json
battlefield4.json
```

Do not pretend that BF3/BF4 mappings are known until they are verified.

Profiles should contain metadata such as:
```json
{
    "name": "Battlefield 4",
    "device": {
        "vid": "06A3",
        "pid": "075C"
    }
}
```

Then mapping and axis configuration.

Validate profile input.

Malformed profiles must produce a useful error rather than crash.

---

# 14. BATTLEFIELD TEST MODE

Create a dedicated page:
```text
Battlefield Mapping Test
```

The purpose is to document the mismatch between X52 input and Battlefield's interpretation.

Allow the user to manually record:
```text
Physical X52 Control
Windows HID Control
Battlefield 3 Result
Battlefield 4 Result
Desired Result
```

Example:
```text
X52 Trigger

Windows:
Button 1

BF3:
Pad Left

BF4:
Pad Left

Desired:
Right Trigger
```

Store these observations.

Eventually we should be able to generate a corrected profile from them.

---

# 15. VIRTUAL CONTROLLER ABSTRACTION

Create an interface such as:
```cpp
class IVirtualController
{
public:
    virtual ~IVirtualController() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void SetButton(
        VirtualButton button,
        bool pressed) = 0;

    virtual void SetAxis(
        VirtualAxis axis,
        float value) = 0;

    virtual void Submit() = 0;
};
```

Do NOT tightly couple the mapping engine to one virtual-controller implementation.

Possible implementations can later include a supported virtual gamepad backend.

The first implementation may be:
```text
NullVirtualController
```

which logs the intended output without creating a virtual device.

This allows the entire input/mapping system to be tested before introducing a driver dependency.

---

# 16. VIRTUAL OUTPUT

After the diagnostic/input portion is stable, investigate a legitimate Windows virtual-controller solution.

Preferred result:
```text
X52
 ↓
Mapper
 ↓
Virtual standard game controller
 ↓
Battlefield
```

If an external signed virtual-controller driver/runtime is required, document this clearly.

Do NOT silently install drivers.

Do NOT create an unsigned kernel driver merely to complete the feature.

Keep driver/backend dependencies isolated.

---

# 17. OUTPUT MONITOR

Show both sides simultaneously.

Example:
```text
PHYSICAL X52                 VIRTUAL OUTPUT

Trigger 1     PRESSED   ->   Right Trigger   100%
Fire          RELEASED  ->   Right Bumper    RELEASED

Stick X       -0.142    ->   Left Stick X    -0.142
Stick Y        0.724    ->   Left Stick Y     0.701
Rudder         0.034    ->   Right Stick X     0.034
```

This will make debugging dramatically easier.

---

# 18. DEVICE DISCONNECTION HANDLING

This is especially important because this particular X52 is currently being diagnosed for intermittent stick resets.

Handle:
```text
device removal
device arrival
read failure
temporary HID interruption
USB reconnect
```

without crashing.

Log events with high-resolution timestamps.

Example:
```text
14:52:11.133 X52 input active
14:52:17.820 HID read timeout
14:52:17.821 input state changed unexpectedly
14:52:18.644 device responsive
```

Where possible distinguish:
```text
whole USB X52 disappeared
```

from:
```text
USB throttle remained enumerated but stick controls stopped updating
```

This diagnostic capability is valuable independently from Battlefield.

---

# 19. EVENT LOGGING

Implement structured logging.

At minimum:
```text
application start
application shutdown
device discovery
device opened
device closed
device removed
device reconnected
HID read error
profile loaded
profile changed
mapping activated
virtual controller connected
virtual controller disconnected
```

Debug mode may optionally log raw HID transitions.

Do NOT continuously write unchanged HID reports to disk.

---

# 20. LATENCY

Controller translation must introduce negligible latency.

Track:
```text
input timestamp
mapping completion timestamp
output submission timestamp
```

Provide diagnostics such as:
```text
Input -> Mapping:  0.04 ms
Mapping -> Output: 0.03 ms
Total Mapper:      0.07 ms
```

Do not make fake precision claims.

Use appropriate Windows high-resolution timing.

---

# 21. THREADING

Suggested architecture:
```text
UI THREAD
    |
    +--- renders state
    +--- handles configuration

HID INPUT THREAD
    |
    +--- overlapped input
    +--- parses reports

MAPPING THREAD / PIPELINE
    |
    +--- normalized state
    +--- mapping
    +--- output

LOGGER
    |
    +--- asynchronous event logging where justified
```

Avoid unnecessary thread proliferation.

State exchange must be thread safe.

Do not use volatile as a substitute for synchronization.

Prefer clear ownership and modern synchronization primitives.

---

# 22. C++ QUALITY REQUIREMENTS

Use:
```text
RAII
std::unique_ptr
std::shared_ptr only when shared ownership is genuinely required
std::array
std::vector
std::string
std::wstring where Windows Unicode APIs require it
std::optional
std::chrono
enum class
constexpr
strong types where useful
```

Avoid:
```text
raw owning pointers
manual new/delete
C-style casts
goto
global mutable state
unsafe sprintf
unbounded buffers
unchecked Windows API results
```

Every Windows handle must have clearly defined ownership.

Use Unicode APIs:
```text
CreateWindowExW
CreateFileW
etc.
```

Compile with strict warnings.

Aim for:
```text
/W4
/permissive-
```

and keep the project warning-clean.

---

# 23. ERROR HANDLING

Windows errors should produce meaningful messages.

Create utility functionality to translate:
```cpp
GetLastError()
```

into human-readable messages.

Bad:
```text
Device failed.
```

Good:
```text
Unable to open Saitek X52 HID interface.

CreateFileW failed.
Win32 error 5:
Access is denied.
```

Logging should retain the numeric error code.

---

# 24. NO MAGIC NUMBERS

HID offsets, masks and ranges must have names.

Bad:
```cpp
if (report[4] & 0x20)
```

Better:
```cpp
constexpr std::size_t ButtonByte = 4;
constexpr std::uint8_t TriggerMask = 0x20;

const bool triggerPressed =
    (report[ButtonByte] & TriggerMask) != 0;
```

Document where every reverse-engineered value came from.

---

# 25. REVERSE ENGINEERING DISCIPLINE

For PS28 HID discoveries maintain a table such as:
```text
Control
Report Byte
Bit/Mask
Range
Observed
Verified
Notes
```

Use statuses:
```text
UNKNOWN
OBSERVED
REPEATABLE
VERIFIED
```

Never promote an assumption to VERIFIED without testing.

---

# 26. FIRST MILESTONE

Do NOT begin with the virtual gamepad.

Milestone 1 is:

**X52 Inspector**

It must:

1. Enumerate HID devices.
2. Locate VID 06A3 / PID 075C.
3. Open the X52.
4. Read input reports.
5. Display raw reports.
6. Display HID capabilities.
7. Detect changed bytes/bits.
8. Show axes.
9. Show buttons.
10. Support Learn Mode.
11. Log connection/disconnection events.
12. Remain stable during device removal/reconnection.

At the end of Milestone 1, build and test it.

Fix all compilation errors and obvious warnings before proceeding.

---

# 27. SECOND MILESTONE

Build:

**Normalized X52 Input Layer**

Map verified PS28 inputs into X52State.

Add automated tests for:
```text
axis normalization
dead zones
axis inversion
button transitions
POV conversion
mapping rules
```

---

# 28. THIRD MILESTONE

Build:

**Mapping/Profile Engine**

Implement:
```text
JSON profiles
BF3 profile
BF4 profile
axis transformations
button remapping
mapping UI
profile persistence
```

Initially use NullVirtualController.

---

# 29. FOURTH MILESTONE

Build:

**Virtual Controller Output**

Select a legitimate supported virtual-controller backend.

Document:

- dependency
- licensing
- installation
- device model presented to Windows
- limitations
- uninstall procedure

Do not bundle third-party binaries without checking licensing.

---

# 30. FIFTH MILESTONE

Perform Battlefield testing.

Test separately:
```text
Battlefield 3

Battlefield 4
```

Record what each game sees.

Create verified profiles based on actual observed behaviour.

Do NOT modify either game's executable.

Do NOT inject code.

Do NOT bypass or disable anti-cheat.

---

# 31. EXPECTED USER EXPERIENCE

Eventually the normal workflow should be:
```text
Start X52 Battlefield Mapper

        ↓

X52 PS28 detected

        ↓

Select:

Battlefield 4

        ↓

Mapper loads BF4 profile

        ↓

Physical X52
        ↓
normalized X52 input
        ↓
BF4 mapping
        ↓
virtual conventional controller
        ↓
Battlefield 4
```

The UI should clearly indicate:
```text
X52 Connected
Virtual Controller Connected
Profile: Battlefield 4
Mapping: ACTIVE
```

---

# 32. IMPORTANT DEVELOPMENT PRINCIPLE

Do not immediately write thousands of lines of speculative code.

Build this incrementally.

For every milestone:
```text
IMPLEMENT
    ↓
COMPILE
    ↓
TEST
    ↓
FIX
    ↓
DOCUMENT
    ↓
COMMIT-READY STATE
```

Do not continue while the solution is knowingly broken.

Do not leave placeholder implementations pretending to work.

If hardware behaviour is unknown, build diagnostics to measure it instead of guessing.

---

# 33. INITIAL TASK

Begin by:

1. Create the Visual Studio C++ solution.
2. Create the directory architecture.
3. Create PROJECT.md.
4. Create AGENTS.md.
5. Create LOCASKILLS.md.
6. Implement HID enumeration.
7. Detect VID `06A3`, PID `075C`.
8. Retrieve HID capabilities and strings.
9. Implement asynchronous input-report reading.
10. Implement a minimal native Win32 Input Inspector.
11. Display raw 16-byte input reports in real time.
12. Highlight bytes/bits that change.
13. Implement device arrival/removal detection.
14. Implement timestamped diagnostic logging.
15. Build the x64 Debug and Release configurations.
16. Correct compilation errors and warnings.
17. Update PROJECT.md with the actual implementation state.

Stop after Milestone 1 is genuinely operational.

Do not implement the Battlefield translation layer until we have verified the physical PS28 HID behaviour from actual hardware.

The immediate engineering objective is:

**Determine precisely what Windows receives from the Saitek X52 PS28 when every physical control is operated.**



# 34. X52 STICK DROPOUT DETECTION AND RECOVERY

This feature is REQUIRED for Milestone 1.

The physical PS28 currently has an intermittent throttle-to-stick Mini-DIN connection.

Observed behaviour:
```text
PC
 |
 | USB
 v
X52 THROTTLE
 |
 | 6-pin Mini-DIN
 |
 v
X52 STICK
```

The throttle may remain operational and enumerated by Windows while the joystick/stick momentarily disconnects or resets.

Therefore:

**Do NOT assume that a stick failure will generate a Windows USB device-removal event.**

The application must attempt to distinguish:
```text
A. Entire X52 USB device disconnected

B. Main throttle/USB controller reset

C. Stick-side Mini-DIN communication interrupted

D. Stick resets and subsequently becomes responsive again

E. Individual controls simply remain stationary
```

The implementation must be evidence-driven.

Do not invent PS28 protocol behaviour.

---

# 35. STICK HEALTH MONITOR

Create:
```cpp
enum class StickHealth
{
    Unknown,
    Healthy,
    SuspectedDropout,
    Disconnected,
    Recovering,
    Recovered
};
```

Implement a dedicated:
```cpp
class StickHealthMonitor;
```

The health monitor should inspect the stream of PS28 HID reports.

It must determine whether there are reliable observable signatures associated with the stick disconnecting from the throttle.

Potential evidence MAY include:
```text
stick axes suddenly returning to fixed values
stick buttons simultaneously changing state
stick hats suddenly becoming neutral
throttle controls continuing to update
specific HID bytes becoming constant
specific HID bits changing during stick reboot
temporary absence of stick-derived state
known/repeatable PS28 status bits
```

These are hypotheses only.

DO NOT hard-code them until experimentally verified.

---

# 36. LEARN DISCONNECT SIGNATURE

Add a diagnostic mode:
```text
Learn Stick Disconnect
```

This is specifically intended for the current faulty/long Mini-DIN connection.

Workflow:
```text
[ Start Disconnect Capture ]

        ↓

Record normal HID traffic

        ↓

User reproduces stick disconnect

        ↓

Stick dies/reboots

        ↓

Record HID behaviour

        ↓

Stick returns

        ↓

[ Stop Capture ]
```

Record at high resolution:
```text
timestamp
raw HID report
previous report
changed bytes
changed bits
decoded throttle state
decoded stick state
Windows device events
ReadFile status
Win32 errors
```

Generate a diagnostic report.

Example:
```text
STICK DROPOUT EVENT

Start:
14:42:16.381

Last known healthy report:
14:42:16.371

USB device:
06A3:075C

USB enumeration:
UNCHANGED

Throttle input:
ACTIVE

Stick X:
-0.134 -> 0.000

Stick Y:
+0.421 -> 0.000

Rudder:
-0.031 -> 0.000

Stick buttons:
ALL RELEASED

Throttle axis:
CONTINUED UPDATING

USB removal event:
NO

Duration:
842 ms

Recovery:
14:42:17.223

Classification:
LIKELY STICK-SIDE DROPOUT
```

Only classify an event when sufficient evidence exists.

---

# 37. PREVENT BAD INPUT REACHING THE GAME

This is critical.

When a stick dropout is detected, DO NOT forward corrupt, stale, or nonsensical values to the virtual controller.

Implement a configurable failsafe.

Default behaviour:
```text
STICK DROPOUT DETECTED

        ↓

Stop forwarding affected physical controls

        ↓

Immediately place affected virtual controls
into defined safe/neutral states

        ↓

Keep virtual controller itself alive

        ↓

Attempt physical-input recovery

        ↓

Validate recovered reports

        ↓

Resume mapped output
```

The virtual controller MUST NOT be destroyed merely because the physical X52 stick disappears temporarily.

The objective is for Battlefield to continue seeing:
```text
Virtual Controller = CONNECTED
```

even while:
```text
Physical X52 Stick = RECOVERING
```

This separation is extremely important.

---

# 38. SAFE STATE

On detected stick failure:
```text
Pitch       -> centre
Roll        -> centre
Yaw         -> centre

Stick buttons -> released
Stick hats    -> neutral
```

Do NOT automatically zero throttle-derived controls if the throttle remains demonstrably healthy.

Therefore, if possible:
```text
STICK FAILURE

Pitch       -> neutral
Roll        -> neutral
Yaw         -> neutral
Trigger     -> released
Stick hats  -> neutral

Throttle    -> CONTINUE
Throttle buttons -> CONTINUE
```

The mapping architecture must support health state per logical input group.

---

# 39. OPTIONAL HOLD-LAST-VALUE MODE

Provide:
```text
Dropout Behaviour

(*) Neutralise controls
( ) Hold last known state
```

Default:
```text
Neutralise controls
```

Holding the last aircraft control position can be dangerous during a dropout.

Example:
```text
Stick held full right
        ↓
connection fails
        ↓
last value remains
        ↓
aircraft continuously rolls
```

Therefore neutralisation is the recommended default.

---

# 40. RECOVERY MANAGER

Create:
```cpp
class X52RecoveryManager;
```

The manager controls software-side recovery.

Recovery stages:
```text
LEVEL 0
Normal operation

LEVEL 1
Detected suspicious stick state

LEVEL 2
Neutralise affected virtual outputs

LEVEL 3
Wait briefly for valid reports to return

LEVEL 4
If necessary, cancel/restart HID reads

LEVEL 5
If necessary, close and reopen the X52 HID handle

LEVEL 6
Rediscover VID 06A3 / PID 075C

LEVEL 7
Validate incoming reports

LEVEL 8
Restore mapped output
```

Do not perform aggressive recovery unnecessarily.

---

# 41. IMPORTANT HARDWARE LIMITATION

Do NOT claim that closing/reopening the Windows HID device electrically resets the physical joystick.

These are different operations:
```text
SOFTWARE:

CloseHandle()
CreateFileW()
restart overlapped reads
re-enumerate HID path
reset internal parser state
```

versus:
```text
HARDWARE:

power-cycle stick
reset stick MCU
reset throttle-to-stick communications
```

Only implement a physical/protocol reset if reverse engineering proves that the PS28 exposes a legitimate command for doing so.

Do NOT send arbitrary HID output or feature reports to the X52.

Unknown reports could change LEDs, configuration, display state, or other hardware behaviour.

---

# 42. FAST RECOVERY

The recovery path should be designed to complete quickly.

Example target:
```text
Stick dropout
     |
     | detection
     v
SAFE OUTPUT
     |
     | stick returns
     v
VALIDATE
     |
     | several valid reports
     v
RESTORE
```

Do not immediately restore output after receiving one apparently valid packet.

Require a configurable short validation period or consecutive-valid-report threshold.

Example:
```cpp
constexpr unsigned RecoveryValidationReports = 5;
```

The exact value should remain configurable.

---

# 43. RECOVERY STATE MACHINE

Use an explicit state machine.

Example:
```cpp
enum class RecoveryState
{
    Normal,
    DropoutSuspected,
    DropoutConfirmed,
    Neutralising,
    WaitingForHardware,
    ReopeningDevice,
    Validating,
    Restoring
};
```

Do not implement recovery using scattered boolean flags.

State transitions must be logged.

Example:
```text
14:52:41.133 NORMAL

14:52:41.142
NORMAL -> DROPOUT_SUSPECTED

14:52:41.162
DROPOUT_SUSPECTED -> DROPOUT_CONFIRMED

14:52:41.163
Virtual stick neutralised.

14:52:41.921
Valid stick traffic detected.

14:52:41.921
DROPOUT_CONFIRMED -> VALIDATING

14:52:41.972
5 consecutive valid reports received.

14:52:41.973
VALIDATING -> RESTORING

14:52:41.974
RESTORING -> NORMAL

Total interruption:
832 ms
```

---

# 44. RECONNECTION BLENDING

Do not necessarily jump immediately from neutral to the recovered physical stick position.

Provide an optional short interpolation/ramp.

For example:
```text
virtual centre
      |
      | 50-150 ms
      v
current physical position
```

This may prevent a sudden aircraft-control spike after reconnection.

Implement this generically in the axis pipeline.

Call it:
```text
Recovery Blend
```

Default initially:
```text
100 ms
```

Make it configurable and allow it to be disabled.

---

# 45. DEVICE WATCHDOG

Implement a lightweight watchdog based on actual input/report activity.

Track:
```cpp
struct DeviceHealthMetrics
{
    std::chrono::steady_clock::time_point lastReport;
    std::chrono::steady_clock::time_point lastValidStickReport;
    std::chrono::steady_clock::time_point lastValidThrottleReport;

    std::uint64_t reportsReceived;
    std::uint64_t readFailures;
    std::uint64_t suspectedDropouts;
    std::uint64_t confirmedDropouts;
    std::uint64_t successfulRecoveries;
};
```

Expose these metrics in Diagnostics.

---

# 46. DIAGNOSTICS UI

Add:
```text
+-------------------------------------------------------+
| X52 CONNECTION HEALTH                                 |
+-------------------------------------------------------+
| USB Device                 CONNECTED                  |
| Throttle                   HEALTHY                    |
| Stick                      HEALTHY                    |
| Last HID Report            3 ms ago                   |
|                                                       |
| Stick Dropouts             4                          |
| Successful Recoveries      4                          |
| Failed Recoveries          0                          |
|                                                       |
| Last Dropout                                         |
| 14:52:41.142                                         |
| Duration: 832 ms                                     |
|                                                       |
| Recovery Mode:             AUTOMATIC                  |
| Dropout Output:            NEUTRALISE                 |
| Recovery Blend:            100 ms                     |
|                                                       |
| [Capture Disconnect] [Export Diagnostic Log]          |
+-------------------------------------------------------+
```

---

# 47. KEEP THE VIRTUAL DEVICE ALIVE

Once virtual-controller support is implemented, this is a fundamental architectural requirement:
```text
          PHYSICAL
            X52
             |
             X   temporary failure
             |
        MAPPING APPLICATION
             |
             | remains alive
             |
       VIRTUAL GAMEPAD
             |
             | remains connected
             |
        BATTLEFIELD
```

Battlefield should NOT experience a virtual controller unplug/replug merely because the X52 stick temporarily fails.

Instead Battlefield should temporarily receive neutral controls.

This provides continuity while the physical hardware recovers.

---

# 48. EMERGENCY MANUAL RECOVERY

Provide a UI command:
```text
Reinitialize X52 Input
```

and optionally a configurable keyboard shortcut.

This operation should:
```text
1. Neutralise virtual output.
2. Cancel pending HID reads safely.
3. Close the HID handle.
4. Rediscover VID 06A3 / PID 075C.
5. Reopen the correct HID interface.
6. Reinitialise parser state.
7. Resume asynchronous reads.
8. Validate input.
9. Restore mapped output.
```

Do not terminate/restart the entire application.

Do not restart Windows services.

Do not disable/re-enable arbitrary USB devices.

---

# 49. EXPERIMENTAL PS28 PROTOCOL RECOVERY

Create an architectural placeholder:
```cpp
class IX52HardwareRecovery
{
public:
    virtual ~IX52HardwareRecovery() = default;

    virtual bool IsSupported() const noexcept = 0;
    virtual bool AttemptRecovery() = 0;
};
```

Initially implement:
```text
UnsupportedHardwareRecovery
```

which performs NO hardware commands.

Later, if reliable PS28 reverse engineering establishes a legitimate throttle-to-stick reinitialisation command, it may be implemented behind this interface.

Do not guess the command.

Do not brute-force HID output reports.

---

# 50. CURRENT HARDWARE TEST OBJECTIVE

The existing 5-metre Mini-DIN cable provides a temporary opportunity to capture the fault before it is replaced.

Use the current faulty configuration to determine:
```text
What exactly changes in the PS28 HID reports
when the stick loses communication with the throttle?
```

Capture several dropout events.

Compare them.

Determine whether there is a repeatable signature.

Once the new shorter DIN cable arrives, repeat the same test.

Compare:
```text
OLD 5 m CABLE

dropout frequency
HID behaviour
recovery time
errors


NEW SHORT CABLE

dropout frequency
HID behaviour
recovery time
errors
```

Store this comparison in:
```text
docs/x52-dropout-analysis.md
```

This evidence should determine the final watchdog logic.

---

# 51. UPDATED MILESTONE 1 COMPLETION CRITERIA

Milestone 1 is not complete until the application can:

1. Detect the PS28.
2. Read its HID reports reliably.
3. Display raw and decoded input.
4. Learn physical controls.
5. Monitor USB device arrival/removal.
6. Capture stick-side dropout events.
7. Determine whether throttle input continues during a stick dropout.
8. Log dropout/recovery timing.
9. Neutralise logically invalid stick state internally.
10. Reopen the HID interface if the Windows HID connection itself fails.
11. Recover automatically when valid input returns.
12. Provide manual "Reinitialize X52 Input".
13. Export a diagnostic report.
14. Survive repeated disconnect/reconnect tests without crashing.

The application must NOT claim to physically reset the joystick unless PS28 protocol research later demonstrates a supported mechanism.

The immediate priority is:

**Keep the application's controller state stable even when the physical X52 stick temporarily loses its Mini-DIN connection.**