# Kitchen Tester — Test Plan

Log results in the tables at the end of each test. Firmware serial console runs
at 115200; type `help` for commands.

**Precondition for any driving:** both learning TODOs implemented —
`mecanum::mix()` (unit tests green via `pio test -e native`) and
`evaluate_failsafe()` (robot arms). The failsafe stub intentionally returns
STOP, so wheels will not turn until the policy is written.

---

## Test 1 — Power-Up and Motor Sanity (~30 min)

Bench test **before** chassis assembly. One L298N + LiPo + ESP32; motors clipped
to the bench, wheels free.

Per motor, from the serial console:
1. `m fl 60` → wheel spins "forward" — mark the rotation direction on tape.
2. `m fl -60` → spins the other way.
3. `m fl 0` → stops.
4. Repeat for `fr`, `rl`, `rr`.

- [ ] All 4 motors spin both directions on command
- [ ] Any backwards motor fixed by swapping its IN1/IN2 in `src/robot/pins.h`
- [ ] 5V rail measured under motor load: ______ V (should stay ≥ 4.75V)
- [ ] `batt` reading vs multimeter at the pack: console ______ V / meter ______ V
- [ ] LiPo cool to the touch after the session

**LiPo care:** 1000mAh @ 35C can source big current but drains fast — ~2h at
500mA, ~30min at 2A. Never leave it charging unattended; stop discharging at
6.8V (firmware warns and then stops the motors — that's the failsafe policy).

## Test 2 — Full Drive on Kitchen Floor (1–2 hrs)

Full assembly, USB serial tether (or type commands, unplug, watch — `demo` has a
3s countdown).

Run `demo` — the scripted sequence does each leg for 2s with pauses:
forward, back, strafe right, strafe left, rotate CW, rotate CCW, diagonal.

- [ ] Forward/backward tracks straight (note any pull: ______)
- [ ] Strafe right/left is clean sideways travel on tile
- [ ] Rotate in place stays centered
- [ ] Diagonal moves at ~45°
- [ ] Rubber mat behavior noted (more slip is expected): ______
- [ ] Wheel slip / vibration notes: ______

Pull to one side = motor speed mismatch — expected open-loop; the production
build fixes it with encoders. Record which way it pulls for reference.

## Test 3 — ESP-NOW Control from Second ESP32 (1–2 hrs)

Flash `pio run -e transmitter -t upload` on the second ESP32. Join its Wi-Fi AP
(`cartbot-tx` / password in `src/transmitter/config.h`) from your phone, open
`http://192.168.4.1`, drive with the touch joystick.

- [ ] Robot responds to joystick with imperceptible latency
- [ ] Kill transmitter power mid-drive → robot stops within ~500ms (watchdog)
- [ ] `stats` on robot console after 5 min driving: packets ______, drops ______
- [ ] Robot MAC hardcoded in transmitter config (or broadcast mode used): ______

Pass = command receipt reliably within one 20ms motor-loop cycle. That's the
green light for the two-robot lift-sync approach.

## Test 4 — Sustained Run and Battery Life

15 minutes of continuous active driving (loop `demo` or joystick).

- [ ] Motor temps by hand after run: warm OK / hot NOT OK — notes: ______
- [ ] L298N heatsink temp: ______
- [ ] Pack voltage at end (console `batt`): ______ V (stop below 6.8V)
- [ ] Approx run time to 6.8V from full charge: ______ min

This baseline sets expectations for production 4S 18650 pack sizing.
