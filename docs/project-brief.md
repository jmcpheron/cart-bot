# Garage Kiva-Bot — Project Brief

## Project Summary

The goal is to build two small autonomous ground robots that work as a coordinated
pair to move loaded garage shelving units around the garage floor — inspired by the
Amazon Kiva/Robotics system. Each robot slides under a custom bottom skid attached
to a standard 2×4 ft metal garage shelf, lifts it 1–2 inches off the ground using a
lead screw mechanism, then repositions it using mecanum wheel drive. The two robots
communicate wirelessly (ESP-NOW) to synchronize their lift so the shelf stays level
during movement. When not in use, the robots return to an autonomous charging dock.

The full production system uses 12V geared motors with Hall encoders (JGB37-520),
BTS7960 high-current motor drivers, 4S 18650 battery packs, NEMA 17 steppers for
the lift, and MPU-6050 tilt sensing for safety. But before any of that gets built,
we validate the core ideas on a cheap kitchen-floor test platform first.

---

## Kitchen Tester — What It Is

The kitchen tester is a minimal, low-cost prototype that proves out the mecanum
drive geometry and ESP32 control code before committing to the full build. It is
not expected to lift anything. It is not expected to run for hours. It just needs
to drive in all eight directions, strafe cleanly, rotate in place, and respond to
commands from a second ESP32 over ESP-NOW.

### Hardware on Hand

| Item | Qty | Notes |
|---|---|---|
| DORHEA TT Motor + 69mm Wheel Kit | 4 sets | 3–6V, 1:48, ~200 RPM at 6V (kit wheels unused — mecanums replace them) |
| Eujgoov 48mm Mecanum Wheels | 2 pairs (4 wheels) | TT-shaft compatible, rubber rollers |
| BOJACK L298N Dual H-Bridge Driver | 4× (use 2) | Controls 2 motors each, 3.3V logic OK |
| URGENEX 7.4V 2S LiPo, 1000mAh, 35C | 2× (use 1) | JST connector, ~51g |
| ESP32 WROOM-32 (38-pin) | 2× from stash | Robot + transmitter; dual-core, ESP-NOW, PWM |

Two L298N boards cover all four motors (two motors per board). The LiPo feeds the
L298N motor supply directly at 7.4V; the L298N's onboard 5V regulator feeds the
ESP32's VIN pin. No separate power converter needed.

> **Power notes (corrections to the original draft):**
> - The L298N drops ~1.4–2.5V through its output transistors, so the 3–6V TT
>   motors see roughly 5–6V from the 7.4V pack. **This is a feature** — don't
>   "fix" it with a buck converter or the motors will be overdriven.
> - Keep the L298N's onboard 5V-regulator jumper **ON** (7.4V input gives the
>   78M05 enough headroom). Add a 470–1000µF electrolytic across the motor
>   supply near the drivers; motor stall transients otherwise cause ESP32
>   brownout resets. All grounds must be common.
> - A 100k/33k divider from pack + to GPIO 34 lets firmware watch pack voltage
>   and enforce the 6.8V (3.4V/cell) cutoff automatically.

### Chassis

A piece of 1/4-inch plywood or a printed plate, roughly 180–200mm square, with the
four TT motors mounted at the corners. Wheels must form the standard mecanum
X-pattern — viewed **from above**, the roller axes form an X:

```
FL ⟍   ⟋ FR
RL ⟋   ⟍ RR
```

Rollers on FL and RR lean one direction; FR and RL lean the other. This is what
enables sideways movement. **Getting this orientation wrong is the most common
first mistake — check it against `docs/kitchen-tester/build-guide.md` before
wiring anything.**

TT motors screw into printed brackets (`cad/`). The 48mm mecanum wheels press
directly onto the TT motor's D-shaft. No axle hardware needed for the tester.

---

## Wiring

See [`docs/kitchen-tester/wiring.md`](kitchen-tester/wiring.md) for the full pin
map and power tree. Key change from the original draft: the pin map avoids **all**
ESP32 strap pins (0, 2, 5, 12, 15) — the draft assigned GPIO 12, which selects
1.8V flash voltage if it reads high at boot and can prevent the board from booting.

---

## Firmware Structure

### Mecanum Kinematics

The core math takes three velocity inputs — forward (vx), strafe (vy), and turn
rate (ω) — and mixes them into four wheel speeds:

```
FL = vx + vy + ω
FR = vx − vy − ω
RL = vx − vy + ω
RR = vx + vy − ω
```

Sign convention: **vx+ = forward, vy+ = strafe right, ω+ = clockwise** (from
above). Each result is scaled and clamped to [−255, 255] and mapped to PWM duty +
direction pins. Open-loop at this stage — no encoders — the goal is validating
that the geometry produces the right movement. The math lives in
`firmware/lib/kinematics/` as a dependency-free library with host-run unit tests,
and is reused unchanged by the production robots.

### ESP32 Task Layout

Two FreeRTOS tasks on the dual-core ESP32:

- **Core 0 — Motor task:** reads the current velocity setpoint from a shared
  store, evaluates the failsafe policy, computes wheel speeds, updates PWM
  outputs every 20ms.
- **Core 1 — Comms task:** serial console + ESP-NOW receive. Updates the shared
  setpoint when a command arrives. Watchdog: no command for 500ms → failsafe.

### Command Protocol (ESP-NOW)

Packed struct, exactly 6 bytes (`firmware/lib/protocol/`):

```c
typedef struct __attribute__((packed)) {
    int8_t   vx;     // forward/back, -100..100
    int8_t   vy;     // strafe right/left, -100..100
    int8_t   omega;  // rotation CW/CCW, -100..100
    uint8_t  cmd;    // 0=drive, 1=stop, 2=ping
    uint16_t seq;    // sequence number for drop detection
} robot_cmd_t;
```

The transmitter sends this at ~10Hz; ESP-NOW frames carry their own CRC, so the
sequence number is only for measuring drops. The transmitter ESP32 hosts a Wi-Fi
AP with a phone-browser joystick page.

---

## Test Sequence

Full checklists with results logs: [`docs/kitchen-tester/test-plan.md`](kitchen-tester/test-plan.md).

1. **Power-up and motor sanity** — bench-test each motor/driver channel from the
   serial console before assembly.
2. **Full drive on kitchen floor** — forward/back, strafe, rotate, diagonal via
   the scripted `demo` sequence over USB serial.
3. **ESP-NOW control from second ESP32** — phone joystick → transmitter →
   robot; validate latency, watchdog, packet loss.
4. **Sustained run and battery life** — 15 min continuous, temperature and
   voltage logging; sets expectations for production 4S pack sizing.

---

## What This Proves (and What It Doesn't)

| Validated by kitchen tester | Deferred to production build |
|---|---|
| Mecanum drive geometry works | Torque under load (500+ lb shelf) |
| ESP32 mecanum firmware | Encoder closed-loop velocity control |
| ESP-NOW latency and reliability | Two-robot lift synchronization |
| Battery voltage/run-time baseline | Lead screw lift mechanism |
| L298N + ESP32 3.3V logic compatibility | BTS7960 high-current driver behavior |
| Basic obstacle response (manual) | Automated docking and charging |

---

## Printed Parts

Sources and print settings in [`cad/`](../cad/README.md):

- **TT motor brackets** — 4× corner cradles, PLA, non-structural.
- **Chassis plate** — optional printed alternative to plywood, PETG, 4mm.
- **Cable clips** — keep the LiPo lead and L298N jumpers out of the rollers.

---

## Next Steps After Kitchen Tests Pass

See [`docs/production/roadmap.md`](production/roadmap.md).
