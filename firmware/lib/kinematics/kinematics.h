#pragma once
#include <cstdint>

// Mecanum drive kinematics — pure math, no Arduino dependencies, so it runs in
// host unit tests (pio test -e native) and is reused by the production robots.
//
// Sign conventions (viewed from above; see docs/project-brief.md):
//   vx    +100..-100   forward / backward
//   vy    +100..-100   strafe right / strafe left
//   omega +100..-100   rotate clockwise / counter-clockwise
//
// Wheel layout is the standard X-pattern:
//   FL = vx + vy + omega
//   FR = vx - vy - omega
//   RL = vx - vy + omega
//   RR = vx + vy - omega

namespace mecanum {

constexpr int16_t kMaxPwm = 255;

struct WheelSpeeds {
    int16_t fl = 0;  // each in [-kMaxPwm, +kMaxPwm]
    int16_t fr = 0;
    int16_t rl = 0;
    int16_t rr = 0;
};

// Mix velocity inputs (each -100..100) into four wheel PWM speeds.
// Expected behavior is pinned down by test/test_kinematics/ — full-scale pure
// motions map to ±kMaxPwm, sub-scale inputs stay proportional.
WheelSpeeds mix(int8_t vx, int8_t vy, int8_t omega);

}  // namespace mecanum
