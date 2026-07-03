# Production Roadmap — after the kitchen tests pass

1. **Order drivetrain hardware** — JGB37-520 12V gearmotors with Hall encoders,
   BTS7960 drivers, 4S 18650 pack + BMS.
2. **Design production chassis** in Onshape — 2020 aluminum extrusion or bent
   sheet metal; link the Onshape doc here and commit exported STEP/STL to `cad/`.
3. **Closed-loop velocity control** — encoder ISRs + PID per wheel; reuse
   `lib/kinematics` unchanged, the mix output becomes a velocity setpoint instead
   of a PWM duty.
4. **Safety** — MPU-6050 tilt sensing, tilt-abort on lift; extend the failsafe
   policy from the tester (`src/robot/failsafe.cpp` is the seed).
5. **Lift sub-assembly** — NEMA 17 + lead screw; 1,000-cycle stress test rig.
6. **Shelf skid frame** — square steel tube, H-shape, for the 2×4 ft shelves.
7. **Two-robot coordination** — extend `lib/protocol` from one-way drive
   commands to a leader/follower lift-sync handshake (the seq/ack groundwork is
   validated by kitchen Test 3).
8. **Docking + charging** — autonomous return to dock; contact pads.

## Open questions to resolve before ordering

- Shelf + contents worst-case weight → wheel count/size and motor torque margin.
- Garage floor surface (sealed concrete?) → mecanum roller compound choice.
- Lift point geometry on the 2×4 shelves → skid design drives robot footprint.
