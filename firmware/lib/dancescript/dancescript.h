#pragma once
#include <cstdint>

// Dance program parsing — pure, no Arduino dependencies, host-tested like
// lib/kinematics. dance.cpp keeps the executor/NVS; this owns the format.
//
// Step forms, semicolon-separated:
//   "vx,vy,w,ms"   timed velocity step (velocities -100..100, ms 50..15000)
//   "t,deg,w"      gyro-terminated turn: rotate deg degrees relative
//                  (signed, CW-positive, ±720) at omega magnitude w
//                  (10..100). Requires a working IMU; the executor ends the
//                  step when the yaw delta is reached. Its ms field holds
//                  the caller-supplied stall timeout, which counts toward
//                  kMaxTotalMs like any other step.

namespace dancescript {

constexpr int kMaxSteps = 64;
constexpr uint32_t kMaxTotalMs = 120000;  // hard cap per program

struct Step {
    int8_t vx = 0, vy = 0, w = 0;
    uint32_t ms = 0;        // duration (timed) or stall timeout (turn)
    bool turn = false;
    float turn_deg = 0.0f;  // signed target delta; turn steps only
};

// Parse a program into out[0..max_steps). Returns the step count, or -1 on
// any parse error / limit violation (mirrors the old dance::setProgram
// contract: all-or-nothing).
int parse(const char* text, Step* out, int max_steps,
          uint32_t turn_timeout_ms);

}  // namespace dancescript
