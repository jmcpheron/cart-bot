# Kitchen Tester — Wiring

![Wiring diagram](img/wiring.svg)

Board **A** drives the front axle (FL + FR), board **B** the rear (RL + RR).
The pin map's source of truth is `firmware/src/robot/pins.h` — if this doc and
the code ever disagree, the code wins and this doc has a bug.

## Harness checklist (point-to-point)

Work top to bottom; tick each wire as it lands. Signals are Dupont jumpers;
power runs are 18–20 AWG silicone wire.

### Power

| ✓ | From | To | Wire |
|---|---|---|---|
| ☐ | LiPo + (JST red) | Board A `12V` terminal | 18–20 AWG red |
| ☐ | Board A `12V` | Board B `12V` (daisy-chain) | 18–20 AWG red |
| ☐ | LiPo − (JST black) | Board A `GND` terminal | 18–20 AWG black |
| ☐ | Board A `GND` | Board B `GND` | 18–20 AWG black |
| ☐ | Bulk cap 470–1000µF | across Board A `12V` ↔ `GND` — **stripe (−) to GND** | leads |
| ☐ | Board A `GND` | ESP32 `GND` pin | black Dupont |
| ☐ | Board A `5V` | ESP32 `VIN` — **untethered driving only, never with USB** | red Dupont |

### Front signals — ESP32 → Board A (remove ENA/ENB plastic jumpers)

| ✓ | ESP32 GPIO | Board A pin | Function |
|---|---|---|---|
| ☐ | 25 | IN1 | FL direction |
| ☐ | 26 | IN2 | FL direction |
| ☐ | 27 | ENA | FL speed (PWM) |
| ☐ | 16 | IN3 | FR direction |
| ☐ | 17 | IN4 | FR direction |
| ☐ | 4  | ENB | FR speed (PWM) |

### Rear signals — ESP32 → Board B (remove ENA/ENB plastic jumpers)

| ✓ | ESP32 GPIO | Board B pin | Function |
|---|---|---|---|
| ☐ | 33 | IN1 | RL direction |
| ☐ | 32 | IN2 | RL direction |
| ☐ | 23 | ENA | RL speed (PWM) |
| ☐ | 19 | IN3 | RR direction |
| ☐ | 18 | IN4 | RR direction |
| ☐ | 21 | ENB | RR speed (PWM) |

### Motors

Wire every motor the same way (e.g. red lead to the odd-numbered OUT); a
backwards wheel is fixed in software, not by re-soldering.

| ✓ | Motor | Terminals |
|---|---|---|
| ☐ | FL | Board A OUT1 / OUT2 |
| ☐ | FR | Board A OUT3 / OUT4 |
| ☐ | RL | Board B OUT1 / OUT2 |
| ☐ | RR | Board B OUT3 / OUT4 |

### Board jumper states (both boards)

- ENA / ENB plastic jumpers: **removed** (we PWM those pins)
- 5V-EN regulator jumper: **installed** (board logic runs from the LiPo)

## Pin map summary

| Motor | Board / channel | IN1 | IN2 | EN (PWM) |
|---|---|---|---|---|
| FL | A / OUT1-2 | 25 | 26 | 27 |
| FR | A / OUT3-4 | 16 | 17 | 4  |
| RL | B / OUT1-2 | 33 | 32 | 23 |
| RR | B / OUT3-4 | 19 | 18 | 21 |

Avoids all ESP32 strap pins (0, 2, 5, 12, 15). Free for future use: 13, 14, 22,
34 (reserved: battery sense), 35/36/39 (input-only).

## Power-up sequence (assembled robot)

1. **USB session (bench / flashing):** Board A `5V` → ESP32 `VIN` wire
   **disconnected**. Plug USB first, then connect the LiPo. Serial console
   drives everything.
2. **Untethered session (floor):** USB unplugged. Connect the `5V` → `VIN`
   wire, then the LiPo — the ESP32 boots from the L298N regulator, the
   transmitter/phone takes over.
3. **Never both.** USB power + the 5V link back-feed each other; most dev
   boards have a protection diode, but "most" is not a wiring plan.
4. Power-down is the reverse: LiPo off first, always.

## First spin & reversed wheels

After the harness is done, flash and run `spin` on the serial console — it
cycles FL → FR → RL → RR for 2s each, announcing each wheel. For any wheel
spinning backwards, swap that motor's `in1`/`in2` numbers in
`firmware/src/robot/pins.h` and reflash. Re-run until all four match their
labels and directions.

## Bench setup (single board, kept for reference)

```
Mac USB ─────────────► ESP32                    (power + serial console)
LiPo (+) ────────────► L298N 12V terminal
LiPo (−) ────────────► L298N GND ── jumper ──► ESP32 GND   (REQUIRED)
TT motor ────────────► L298N OUT1 / OUT2
GPIO 25 ► IN1    GPIO 26 ► IN2    GPIO 27 ► ENA (plastic jumper removed)
```

The common-ground jumper is not optional: IN pins are read *relative to the
L298N's ground* — without it the motor does nothing or twitches randomly.

## Battery sense divider (future add)

Two resistors from pack + to GPIO 34 arm the firmware's low-battery limp/stop:

```
LiPo (+) ──[100kΩ]──┬──[33kΩ]── GND
                    └────────── ESP32 GPIO 34
```

8.4V full pack → ~2.08V at the pin (safe for ADC, 11dB attenuation). When
wired: set `kBatterySenseWired = true` in `firmware/src/robot/battery.h`,
reflash, calibrate `kDividerRatio` against a multimeter using the `batt`
console command. Until then the firmware reports "sense not wired" and the
failsafe skips battery checks.

## Power notes

- The L298N drops ~1.4–2.5V, so the 3–6V TT motors see ~5–6V from the 7.4V
  pack — **intentional**, don't add a buck converter.
- The bulk capacitor is load-bearing: four motors stalling together sag the
  rail enough to brown-out the ESP32. Symptom: robot "reboots" when it hits
  a wall.
- PWM runs at 1kHz (`motors.cpp`) — the L298N's slow BJT bridge loses short
  pulses at higher frequencies; some whine under partial throttle is normal.
