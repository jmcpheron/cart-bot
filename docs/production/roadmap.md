# Production Roadmap — after the kitchen tests pass

## Next milestone: closed-loop tracking (kitchen scale)

Open-loop dances drift because the TT motors are inconsistent and the
mecanums skid — by design, position feedback is the fix, not better motors.
Plan, in increasing order of capability:

1. **Gyro heading-hold (cheap interim): BUILT — awaiting hardware bring-up.**
   MPU-6050 (GY-521) on I²C, SDA=GPIO13 SCL=GPIO33 (`src/robot/imu.{h,cpp}`,
   raw registers, no library). Yaw-hold PI (`lib/heading`) trims ω whenever
   the robot translates with w=0 — kills rotational drift, does nothing for
   position. Dances also gained gyro-terminated turns: `t,<deg>,<w>` steps
   (`lib/dancescript`) end at the measured angle, not a timer. Debug:
   `/imu` endpoint, serial `imu` command, "gyro square" preset on the drive
   page. NOTE before wiring: GPIO 33 was the ORIGINAL RL_IN1 — confirm no
   stray harness wire is still on it (same caveat as 32).
2. **Overhead camera + ArUco (the real thing): BUILT — see
   [tracker-setup.md](tracker-setup.md).** One downward-looking AI-Thinker
   ESP32-CAM (`pio run -e camera`) streams frames; `tools/tracker/` (uv
   project like cadlab) detects a printed ArUco marker on the robot's roof,
   maps it through a floor-marker homography, and sends `/cmd` corrections
   at 10 Hz. Waypoint following = `tracker goto` / `tracker route`.
   ESP32-CAMs never do vision themselves — they're too weak; they only
   stream.
3. **Multi-camera coverage:** only when one camera's view isn't enough
   (garage scale). Homographies + hand-off; genuinely advanced, defer.

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
