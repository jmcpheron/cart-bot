# Kitchen Tester — Build Guide

## 1. Chassis plate

180–200mm square, either:
- 1/4" plywood offcut (fastest — drill holes to match printed brackets), or
- printed plate: `cad/openscad/chassis-plate.scad` → PETG, 4mm (see `cad/README.md`).

Keep the deck low; the mecanums are only 48mm diameter, so ground clearance is
~15mm with brackets mounted under the plate.

## 2. Mecanum X-pattern — check this three times

Viewed **from above**, the roller axes must form an **X**:

```
        FRONT
   FL ⟍     ⟋ FR
   RL ⟋     ⟍ RR
        REAR
```

Practical check: on each wheel, look at the roller that is touching the ground.
Its axis should point toward the **center of the robot**. FL and RR are one
handedness, FR and RL the other — mecanum wheels come in left/right pairs, so
each pair splits across a diagonal.

If strafing makes the robot rotate or crab diagonally instead of moving sideways,
two wheels are swapped. If strafing does nothing (wheels spin, robot vibrates in
place), the X is inverted (O-pattern) — swap left and right wheels on both axles.

## 3. Motor mounting

- Print 4× `tt-motor-bracket` (PLA). **Print one first** and check fit against an
  actual DORHEA motor before committing to all four — TT clones vary by ±0.5mm.
- Motors bolt into brackets with M3×30 screws through the motor's mounting holes;
  brackets screw to the plate with M3 hardware (or wood screws into plywood).
- Mount motors with output shafts outboard, gearbox inboard. All four shafts on
  the same axle line front/rear.
- Press mecanum wheels onto the D-shafts. Grub screw (if the hub has one) onto
  the flat of the D.

## 4. Electronics placement

- Both L298N boards centered on the plate, heatsinks up.
- ESP32 near the front edge, USB port facing outward (you'll be plugging in a lot
  during Tests 1–2).
- LiPo velcro-strapped in the middle (mass centered = predictable rotation).
- Bulk capacitor across the screw terminals of Board A.
- Print a few `cable-clip`s and route wires away from the wheels — mecanum
  rollers eat loose jumper wires.

## 5. Wiring

Follow [`wiring.md`](wiring.md). Sequence: motors → drivers first, then logic
pins, battery last. Double-check LiPo polarity at the L298N terminals with a
multimeter before first connect — the screw terminals accept either orientation
and reverse polarity kills the board instantly.

## 6. First power-up

Go to [`test-plan.md`](test-plan.md), Test 1. Do not skip the bench test — it
catches swapped IN1/IN2 pairs and dead channels while everything is still easy
to probe.
