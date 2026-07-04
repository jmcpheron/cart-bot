# Kitchen Tester — Build Guide

## 1. Chassis

The chassis is Jason's own 3D-printed design: motors mounted vertically at the
four corners, wheels outboard on the TT D-shafts. (An unused alternative — a
flat printed plate with a hole grid — lives in `cad/openscad/chassis-plate.scad`
if a rebuild is ever needed. Drop the actual chassis STL/source into `cad/stl/`
to version it with the project.)

Whatever the mount style, two geometric requirements matter to the software:

1. **All four wheel axles parallel**, pointing left-right, wheels at the
   corners of a rectangle. The mixing math assumes this.
2. **The mecanum X-pattern** — see the next section, and check it *after*
   every wheel-swap or rebuild.

## 2. Mecanum X-pattern — check this three times

This is the highest-risk assembly step, doubly so with a custom mount
orientation. Viewed **from above**, the roller axes must form an **X**
(also drawn in the [wiring diagram](img/wiring.svg)):

```
        FRONT
   FL \     / FR
   RL /     \ RR
        REAR
```

Practical check per wheel: look at the roller **touching the ground** — its
axis must point at the **center of the robot**. Mecanum wheels come in left-
and right-handed pairs; each pair splits across a *diagonal* (FL+RR one
handedness, FR+RL the other).

Symptoms of getting it wrong:
- Strafe command → robot **rotates or crabs** instead of moving sideways:
  two wheels are swapped.
- Strafe command → wheels spin, robot **vibrates in place**: the pattern is
  an O instead of an X — swap left and right wheels on both axles.

## 3. Motor mounting

- Motors bolt through their gearbox holes with M3×30 screws. With vertical
  mounts, double-check the output shafts all sit on the same axle line
  front-to-back and the same height left-to-right — a tilted wheel loads one
  roller edge and ruins strafing.
- Press mecanum wheels onto the D-shafts; seat the hub grub screw (if present)
  on the flat of the D.

## 4. Electronics placement

- **Board A (front)** between the front motors, **Board B (rear)** between the
  rear motors — keeps motor leads short; heatsinks up.
- ESP32 mid-deck, USB port reachable (you'll flash and bench-drive a lot).
- LiPo velcro-strapped at the center of mass — centered mass makes rotation
  predictable.
- Bulk capacitor across Board A's supply terminals.
- Clip or zip-tie every wire away from the wheels (`cad/openscad/cable-clip.scad`
  prints in minutes) — mecanum rollers eat loose Dupont leads.

## 5. Wiring

Follow the harness checklist in [`wiring.md`](wiring.md) — every wire is a
row you can tick off, and the diagram shows the full layout. Sequence: motor
leads first, then logic, battery last. Meter-check LiPo polarity at the screw
terminals before first connect; reverse polarity kills an L298N instantly.

## 6. Four-wheel bring-up

1. Flash (`pio run -e robot -t upload`), open the console
   (`pio device monitor`).
2. Run `spin` — each wheel spins in turn (FL → FR → RL → RR, 2s each, labeled).
   Wrong wheel moves = crossed board/channel wiring; wheel spins backwards =
   swap that motor's `in1`/`in2` in `firmware/src/robot/pins.h`, reflash.
3. Then the [test plan](test-plan.md): Test 2 (`demo` on the floor) onward.
