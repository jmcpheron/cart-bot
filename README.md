# cart-bot — Garage Kiva-Bot

Two small autonomous ground robots that work as a coordinated pair to move loaded
garage shelving units, inspired by Amazon's Kiva robots. Each robot slides under a
skid attached to a 2×4 ft metal shelf, lifts it 1–2 inches with a lead screw, and
repositions it on mecanum wheels. The pair synchronizes over ESP-NOW.

**Current phase: the kitchen tester** — a minimal mecanum platform that validates
drive geometry, ESP32 firmware, and the ESP-NOW control link before the production
build. See [docs/project-brief.md](docs/project-brief.md) for the full vision.

## Repo map

| Path | Contents |
|---|---|
| `docs/project-brief.md` | Full project brief (with hardware corrections applied) |
| `docs/kitchen-tester/` | Build guide, wiring/pin map, test plan for the tester |
| `docs/production/roadmap.md` | Next steps after the kitchen tests pass |
| `firmware/` | PlatformIO project: `robot` + `transmitter` targets, host-run unit tests |
| `cad/` | OpenSCAD sources + STLs for printed parts (motor brackets, clips, chassis) |
| `hardware/bom.md` | Bill of materials — on-hand tester parts and production shopping list |

## Firmware quickstart

```sh
cd firmware
pio test -e native          # kinematics unit tests, runs on your Mac, no hardware
pio run -e robot            # build robot firmware
pio run -e robot -t upload  # flash the robot ESP32 over USB
pio device monitor          # serial console (115200) — type `help`
pio run -e transmitter -t upload   # flash the second ESP32 (Test 3)
```

## Status

- [x] Repo scaffold, docs, firmware skeleton, printable part sources
- [ ] `mecanum::mix()` implemented (Jason — see `firmware/lib/kinematics/`)
- [x] Failsafe policy implemented — limp/stop ladder on command silence + battery
- [ ] Test 1 — bench motor sanity (FL verified on bench 2026-07-02; FR/RL/RR pending)
- [ ] Test 2 — full drive on kitchen floor (serial `demo` command)
- [ ] Test 3 — ESP-NOW remote control from second ESP32
- [ ] Test 4 — sustained run + battery baseline
