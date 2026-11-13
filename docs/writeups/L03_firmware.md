# Level 3 — heat, fire, panic and fail-safe door

The Level 3 safety logic is in `firmwares/level_1/level_1.ino`; its
hardware-independent status, trend and heat-index logic is under
`firmwares/level_1/src/core/`.

## Safety behavior

- Valid DHT11 samples are limited to 0–50 °C and 20–90% RH. A failed or
  out-of-range sample sets quality to `bad`, clears the displayed sample, and
  is excluded from status and trend calculations. The last accepted heat
  status is retained.
- Warning starts at 32 °C or 85% RH; Danger starts at 40 °C. Each transition
  must remain a candidate for 3 seconds. Danger falls to Warning at 38 °C;
  Warning falls to Safe only at 30 °C or below and 80% RH or below.
- Heat index uses the NOAA Rothfusz regression in Fahrenheit, converted to
  Celsius, with NOAA's low/high humidity adjustments. Trend compares the first
  and last of the ten most recent valid temperatures; it never affects status,
  light, buzzer or door.
- A flame continuously detected for 2 seconds sets the NVS `fire_latched` flag
  before changing the device state, unlocks the door and raises FIRE. A storage
  failure is reported, but the live device still enters the fail-safe state.
  If the saved latch cannot be read at boot, the device also defaults to the
  latched/unlocked state. The latch keeps the door unlocked over power loss;
  the Teacher tag can silence the buzzer only after the flame is gone. Reset
  refuses while flame is present and clears the latch in NVS before relocking.
- Touch input is calibrated at boot. A touch held for 2 seconds raises PANIC
  and runs the red/blue and buzzer pattern for 10 seconds. Short touches do
  not raise PANIC.

The onboard firmware can enroll a Teacher tag from Serial Monitor with
`teacher enroll`, then tapping the tag within 30 seconds. `teacher add <uid>`
and `teacher list` are available for setup. `reset_fire` is the current
physical-console fallback; the firmware in this repository does not yet have
the Level 2 Wi-Fi client or Level 4 device-command WebSocket needed for a
dashboard reset.

## Verification status

`test/test_status_machine.cpp` and `test/test_heat.cpp` cover thresholds,
hysteresis, three-second holds, bad readings, timer wraparound, trend, and heat
index. The device-side local web page and cloud event/telemetry delivery still
depend on the not-yet-implemented Level 2 firmware; those end-to-end checks,
the touch calibration on the actual board, NVS power-cut behavior, and the
judge videos remain to be verified on hardware.
