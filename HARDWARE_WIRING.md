# Hardware Wiring - MG995 Base Servo Build

Fork of [EbenKouao/arduino-robot-arm](https://github.com/EbenKouao/arduino-robot-arm) (original by Smartbuilds.io). This document describes the modified hardware: the NEMA 17 base stepper is replaced by an **MG995 positional servo on PCA9685 CH0**.

Wiring overview: [`block-diagrams/mg995-base-servo-wiring.svg`](block-diagrams/mg995-base-servo-wiring.svg) (hand-drawn schematic; the original Fritzing diagrams still show the old stepper build).

> **Status:** the firmware compiles for an Arduino Uno (checked against a stub of the library API). **Nothing here has been tested on physical hardware.** Treat all calibration values as unverified until you have run the calibration sketch.

## 1. Overview

An Arduino Uno drives seven servos through a PCA9685 (I2C) and receives single-character commands from an HC-05 Bluetooth module (hand-gesture glove or phone app).

## 2. Original vs. modified

| | Original | This build |
|---|---|---|
| Base rotation | NEMA 17 + A4988, STEP D5 / DIR D4 | MG995 on PCA9685 CH0 |
| Base motion | relative, 120 steps per command | absolute position, incremental, software limits |
| Base power | 12 V jack to driver | external 5-6 V supply (shared servo rail) |
| Servo power | 3.7 V LiPo on PCA9685 V+ | external regulated 5-6 V on V+ |
| Arduino pins D4, D5 | stepper | unused |
| Servo channels | CH0-5 | CH0-6 (re-mapped, see 4) |

## 3. Components

- Arduino Uno
- PCA9685 16-channel PWM driver (Hobby Components HCMODU0097 / compatible, address 0x40)
- HCPCA9685 Arduino library (Hobby Components). Install it before compiling.
- MG995 servo (base) + 6 other servos (gripper, wrist x2, shoulder, elbow x2)
- HC-05 Bluetooth module (breakout with onboard regulator)
- 1 kΩ and 2 kΩ resistors (HC-05 RXD divider)
- Regulated 5-6 V DC supply, **3 A minimum for the MG995 alone**; size it for the whole arm (all servos moving at once draws much more; 5 A or more is a sensible target)
- 470-1000 µF electrolytic capacitor across the servo power rail (recommended)
- Jumper wires, breadboard, USB cable

## 4. PCA9685 channel mapping (this build)

| Channel | Servo | Commands |
|---|---|---|
| CH0 | **MG995 base rotation** | `S`/`T` left, `O`/`t` right |
| CH1 | Gripper | `F` open, `f` close |
| CH2 | Wrist 2 (rotation) | `R` CW, `L` CCW |
| CH3 | Wrist 1 (up/down) | `U` up, `G` down |
| CH4 | Shoulder | `C` up, `c` down |
| CH5 | Elbow servo 2 (**mirrored**) | `P` up, `p` down |
| CH6 | Elbow servo 1 (direct) | `P` up, `p` down |

Original → new: gripper 5→1, wrist 2 4→2, wrist 1 3→3, shoulder 2→4, elbow R (mirrored) 1→5, elbow L (direct) 0→6. The elbow pair still moves together: CH6 gets the position value, CH5 gets `180 − value` (same mirrored logic as the original). *Assumption:* "elbow servo 1" (CH6) is the original direct servo and "elbow servo 2" (CH5) the mirrored one. If the elbow moves its two halves against each other, swap `CH_ELBOW_1` / `CH_ELBOW_2` at the top of the sketch.

Channel numbers are constants at the top of `arduino_robot_arm_code.ino`.

## 5. MG995 wiring

| MG995 wire | Connect to |
|---|---|
| Signal (orange/yellow) | PCA9685 CH0 signal pin |
| Red | CH0 V+ pin (the servo rail, fed from the external 5-6 V supply) |
| Brown/Black | CH0 GND pin / external supply GND |

## 6. PCA9685 wiring

| PCA9685 | Connect to |
|---|---|
| VCC | Arduino 5V (logic only) |
| GND | Arduino GND (and external supply GND) |
| SDA | Arduino A4 |
| SCL | Arduino A5 |
| V+ | External servo supply positive |
| OE | not connected |

VCC (logic) and V+ (servo power) are **separate** connections.

## 7. HC-05 wiring

| HC-05 | Connect to |
|---|---|
| VCC | Arduino 5V |
| GND | Arduino GND |
| TXD | Arduino D0 (RX) |
| RXD | Arduino D1 (TX) **through the divider below** |

Baud rate: 4800 (the HC-05 must be set to 4800 via AT mode, as in the original project).

## 8. Voltage divider (HC-05 RXD)

```
Arduino D1 (TX)
      |
     1 kΩ
      |
      +------ HC-05 RXD
      |
     2 kΩ
      |
     GND
```

The HC-05 RXD pin is a 3.3 V logic input; the Arduino TX is 5 V. The divider gives 5 V × 2/3 ≈ 3.3 V. HC-05 TXD → D0 needs no divider (3.3 V is read as HIGH by the Uno).

## 9. External power requirements

- Regulated **5-6 V** DC, 3 A or more. Do not exceed 6 V unless you have confirmed the rating of your PCA9685 board and every servo (not verified for this build: read the servo and board labels).
- External + → PCA9685 V+. External − → PCA9685 GND **and** Arduino GND (common ground is mandatory).
- **Never power the MG995 from the Arduino 5V pin.** The Arduino 5V is only for the PCA9685 VCC and the HC-05.
- Because V+ is shared, all other servos now also run from this supply. Check that your gripper/wrist/shoulder/elbow servos are rated for the voltage you choose.
- The old 12 V adapter, A4988 driver and NEMA 17 are no longer used.

## 10. Bluetooth commands (4800 baud)

| Char | Action | Char | Action |
|---|---|---|---|
| `S` | Base left | `U` / `G` | Wrist 1 up / down |
| `O` | Base right | `R` / `L` | Wrist 2 CW / CCW |
| `T` | Base left (glove thumb flex above threshold) | `F` / `f` | Gripper open / close |
| `t` | Base right (glove thumb flex below threshold) | `P` / `p` | Elbow up / down |
| `C` / `c` | Shoulder up / down | | |

`T`/`t` were already sent by the glove sketch (commented "thumb 1 - Base Rotation") but the original arm sketch ignored them. They collide with no other command, so the arm now treats them like `S`/`O`; `robotic_glove_code.ino` is unchanged. Each command moves the base by `servo_base_pos_increment` units, clamped to the limits.

## 11. MG995 calibration

The HCPCA9685 `Servo()` call takes a **position value (0-480), not degrees**. The real library source was not available when this was written, so the exact value → pulse-width mapping is unverified. Calibrate empirically:

1. Remove the servo horn / free the base so nothing can jam. Disconnect the HC-05 from D0/D1.
2. Power the external supply, upload `code/test-code/mg995_base_calibration`, open Serial Monitor at 9600 baud (no line ending).
3. The servo starts at 240. Press `d`/`a` (±10) or `w`/`s` (±1) to creep toward each end. Stop and back off ~10 units when the servo hums or reaches its stop.
4. Note the **min**, **max** and the value that points the base straight ahead.
5. Edit these in `arduino_robot_arm_code.ino`:

| Setting | Default (uncalibrated) | Meaning |
|---|---|---|
| `servo_base_parking_pos` | 240 | startup position |
| `servo_base_min_pos` / `servo_base_max_pos` | 140 / 340 | software limits |
| `servo_base_pos_increment` | 10 | units per command |
| `servo_base_step_delay` | 5 | ms per unit (higher = slower) |
| `servo_base_left_direction` | +1 | set −1 if `S` turns the wrong way |

The firmware clamps every base move to min/max, and the startup position to the limits too.

## 12. Upload instructions

1. Install the **HCPCA9685** library (Hobby Components) in the Arduino IDE.
2. **Disconnect the HC-05 from D0/D1** (it shares the USB serial lines) before uploading.
3. Select *Arduino Uno*, open `code/arduino_robot_arm_code/arduino_robot_arm_code.ino`, upload.
4. Reconnect the HC-05, power the servo supply, pair the HC-05 and send commands.

At power-up the base servo moves to `servo_base_parking_pos`; with no position feedback it can jump from wherever it was. Keep clear of the arm and power the servo supply with the arm in a safe pose. The other joints behave as in the original (they move on their first command).

## 13. Safety

- Keep fingers and cables out of the arm's reach when powering on.
- Use a supply with current limiting or a fuse; a stalled MG995 can draw 1 A or more.
- Do not exceed 6 V on V+ without verifying your board and servo ratings.
- Do not force servo horns against end stops; stop calibrating when the servo buzzes.
- Always join all grounds; never connect the supply with reversed polarity.
- The HC-05 RXD pin is not 5 V tolerant; keep the divider in place.
