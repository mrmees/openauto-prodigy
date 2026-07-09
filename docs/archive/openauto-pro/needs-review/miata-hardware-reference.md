# Miata Head Unit — Hardware Reference

Extracted from `miataPy.py`, the Python control script that ran alongside OpenAuto Pro
on the Miata's Raspberry Pi 4. This documents the physical hardware, wiring, and
behavioral logic so it can be reimplemented as Prodigy features/plugins.

## GPIO Pin Map (BCM numbering)

| BCM Pin | Name | Direction | Purpose |
|---------|------|-----------|---------|
| 12 | IGN_PIN | Input | 12V switched ignition sense (HIGH = engine on) |
| 25 | EN_POWER_PIN | Output | Power latch — Pi holds itself on, controls own shutdown |
| 13 | ILLUM_PIN | Input | Headlight/illumination sense (HIGH = lights on) |
| 7 | REVERSE_PIN | Input | Reverse light sense (unused in script, wired) |
| 14 | INPUT1_PIN | — | Defined but unused |
| 17 | INPUT2_PIN | — | Defined but unused |
| 27 | OUTPUT0_PIN | Output | Amplifier power control (HIGH = amp on) |
| 22 | OUTPUT1_PIN | — | Defined but unused |
| 5 | DIMMER_PIN | Output (PWM) | Servo controlling screen brightness knob |
| 6 | DIMMER_FEEDBACK_PIN | Input | Servo position feedback (read but not acted on) |

### Power Latch Circuit

The Pi controls its own power via `EN_POWER_PIN` (BCM 25). On boot, the script sets
this HIGH immediately, latching power on. The Pi then monitors ignition state and
performs a graceful shutdown sequence before releasing the latch. This prevents the Pi
from being hard-killed when the car turns off.

## I2C Devices

| Address | Device | Library | Purpose |
|---------|--------|---------|---------|
| 0x27 | MCP23017 | `adafruit_mcp230xx` | I2C GPIO expander for toggle switches |
| 0x36 | Adafruit I2C QT Rotary Encoder | `adafruit_seesaw` | Rotary encoder with NeoPixel |

### I2C Bus

Standard Raspberry Pi I2C (SCL/SDA on GPIO 2/3). Both devices share the bus.

## Toggle Switches (via MCP23017 at 0x27)

Three toggle switches read through the I2C expander. Active high (switch on = pin reads 1).

| MCP Pin | Color | Toggle ON Action | Toggle OFF Action |
|---------|-------|------------------|-------------------|
| 0 | Orange | Screen off (uhubctl) | Screen on (uhubctl) |
| 1 | Green | Disable wireless AA hotspot | Enable wireless AA hotspot |
| 2 | Red | Amp off (OUTPUT0 LOW) | Amp on (OUTPUT0 HIGH) |

- **Poll interval:** 1 second (`TOGGLE_BUTTON_INTERVAL`)
- **Debounce:** 0.5 seconds per button
- State is tracked per-button (`PREVIOUS_STATE`) to detect transitions

## Rotary Encoder (Adafruit I2C QT at 0x36)

Adafruit I2C QT Rotary Encoder breakout with built-in NeoPixel LED.

- **Encoder:** Read via `rotaryio.IncrementalEncoder` on the seesaw
- **Button:** Seesaw pin 24, pulled high (LOW = pressed)
- **NeoPixel:** Seesaw pin 6, 1 pixel, brightness tracks dimmer level (halved)
- **Position is inverted** (`rotary_position = -rotary_position`) for UX

### Jitter Filtering

Large position jumps are discarded: if `abs(new - old) >= 30`, the change is ignored.
This prevents phantom rotations from electrical noise.

### Mode System

The rotary encoder has three modes, selected by hold duration:

| Mode | Hold Duration | LED Color | Timeout | Function |
|------|--------------|-----------|---------|----------|
| 0 | Default / >5s hold / timeout | Off (white flash on reset) | — | Volume control |
| 1 | 2–3 seconds | Red | 3 seconds | Screen dimmer adjustment |
| 2 | 3–5 seconds | Green | 15 seconds | Navigation (AA map scrolling) |

Holding >5 seconds cancels back to mode 0 (white LED flash for 2s).

**If the encoder is rotated while the button is held, mode selection is aborted.**
This allows hold+rotate for track skip without accidentally changing modes.

### Button Actions by Mode

| Action | Mode 0 | Mode 2 (Nav) |
|--------|--------|--------------|
| Short press (≤0.5s) | `B` key (play/pause) | `Enter` key |
| Long press (0.5–1.0s) | `Alt+Tab` (cycle apps) | `Escape` key (back) |

### Rotation Actions by Mode

| Mode | Rotate CW | Rotate CCW |
|------|-----------|------------|
| 0 (Volume) | `F8` × 3 (volume up) | `F7` × 3 (volume down) |
| 1 (Dimmer) | Brighten screen (servo) | Dim screen (servo) |
| 2 (Nav) | `2` key (rotate right) | `1` key (rotate left) |

### Hold + Rotate (any mode)

| Direction | Action |
|-----------|--------|
| CW (forward) | `N` key (next track) |
| CCW (back) | `V` key (previous track) |

## Screen Dimmer (Servo)

A physical servo attached to the brightness knob on the back of the screen.
Controlled via pigpio PWM on BCM pin 5.

| Parameter | Value |
|-----------|-------|
| Min pulsewidth | 500 µs (dimmest) |
| Max pulsewidth | 2500 µs (brightest) |
| Step size | 200 µs per encoder click |
| Default | 2500 µs (full bright) |

### Auto-Dim Behavior

- **Headlights ON** (`ILLUM_PIN` HIGH): Servo moves to min (500 µs)
- **Headlights OFF** (`ILLUM_PIN` LOW): Servo moves to max (2500 µs)
- Manual adjustment via rotary mode 1 overrides within session

### Servo Power Management

After each move, the script waits for the servo to reach position, then sets pulsewidth
to 0 (disables PWM signal) to prevent servo buzz/jitter. Sleep times: 100ms for rotary
adjustments, 1000ms for headlight auto-dim.

## USB Hub Power Control

Screen and peripherals powered through a USB hub, controlled via `uhubctl`.
Hub location: `1-1.1`

| Port | Device | Command |
|------|--------|---------|
| 1 | Bluetooth adapter | `sudo uhubctl -l 1-1.1 -p 1 -a {0,1} -r 100` |
| 2 | Screen | `sudo uhubctl -l 1-1.1 -p 2 -a {0,1} -r 100` |
| 3 | Audio device | `sudo uhubctl -l 1-1.1 -p 3 -a {0,1} -r 100` |

## Power State Machine

### Ignition ON (steady state)
- All inputs polled (rotary, toggles, dimmer)
- Main loop rate: ~10 Hz (100ms sleep)

### Ignition OFF → Screen Off
- **Delay:** 1 second (`IGN_SCREEN_OFF`)
- Saves amp state, turns off amp, turns off screen via uhubctl
- Phone Bluetooth disconnected (commented out)

### Ignition OFF → Shutdown
- **Delay:** 1200 seconds / 20 minutes (`IGN_LOW_TIME`)
- Sends `wmctrl -c MM9900` to cleanly close TunerStudio
- Waits 15 seconds for wireless reconnection attempt
- If internet available: runs rclone sync (TunerStudio → OneDrive)
- `sudo shutdown -h -P now`

### Ignition ON (from off state)
- Restores saved amp state
- Turns screen back on via uhubctl
- Phone Bluetooth reconnected (commented out)

## Rclone Sync

On shutdown with internet, syncs TunerStudio project to OneDrive:

- **Local:** `/home/pi/TunerStudioProjects/MSPNPPro-MM9900`
- **Remote:** `OneDrive:Documents/TunerStudioProjects/Live_Miata_Info`
- **Flags:** `--ignore-checksum -v`

## Phone Bluetooth

Hardcoded MAC: `DC:DC:E2:F0:BC:B5` (connect/disconnect via `bluetoothctl block/unblock`).
Both commands are commented out in the current script.

## OAP Keyboard Mappings Used

These are the keystrokes the rotary encoder sends, mapped to OpenAuto Pro functions:

| Key | OAP Function |
|-----|-------------|
| `B` | Toggle play/pause |
| `N` | Next track |
| `V` | Previous track |
| `F7` | Volume down |
| `F8` | Volume up |
| `1` | Rotate left (navigation) |
| `2` | Rotate right (navigation) |
| `Enter` | Select/confirm (navigation) |
| `Escape` | Back/cancel (navigation) |
| `Alt+Tab` | Cycle between applications |

## Known Bugs in miataPy.py

Preserved here so they're not reimplemented:

1. **`keyboard.press_and_release('F7','F7','F7')`** — Likely only sends one keypress.
   The library takes a single hotkey string, not varargs.
2. **No GPIO cleanup on crash** — Pins stay in last state if script dies unexpectedly.
3. **Servo sleeps block the main loop** — 100ms–1000ms of missed input during servo moves.
4. **CPU spin in unknown state** — The `else` branch at the bottom has no sleep.
5. **Bare `except:` in internet check** — Catches KeyboardInterrupt and SystemExit.

## Prodigy Integration Notes

When reimplementing in Prodigy:

- **Ignition/power latch** — Should be a core system service, not a plugin. This is
  safety-critical (prevents data loss and SD card corruption).
- **Rotary encoder** — Good candidate for a configurable input device with mode support.
  The mode/hold/rotate interaction model is solid, just needs cleaner implementation.
- **Toggle switches** — Generic "I2C GPIO → action" mapping. Plugin territory.
- **Servo dimmer** — Hardware-specific brightness control. Could be a display driver
  plugin with a standard brightness API that Prodigy calls into.
- **USB hub power** — Part of power management service.
- **Rclone sync** — Shutdown hook system. User-configurable "run these commands on shutdown."
- **Keyboard emulation** — Should be replaced with direct Prodigy API calls instead of
  injecting fake keystrokes. The rotary encoder should talk to Prodigy's input system,
  not pretend to be a keyboard.
