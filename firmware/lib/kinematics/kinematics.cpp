#include "kinematics.h"

namespace mecanum {

// ─────────────────────────────────────────────────────────────────────────────
// TODO(jason): implement the mecanum mixing function.
//
// The unit tests in test/test_kinematics/test_main.cpp define the contract —
// run `pio test -e native` and make them green. Roughly:
//
//   1. Compute the four raw sums from the equations in kinematics.h.
//      Raw values land in [-300, +300] (three inputs of ±100 each).
//   2. Scale so a full-scale pure motion (e.g. vx=100 alone) hits ±255.
//      That's a factor of 255/100.
//   3. Decide what happens when a *combined* motion overflows ±255
//      (e.g. vx=100, vy=100 → raw 200 → scaled 510). Two valid choices:
//
//      a) CLAMP each wheel to ±255 independently. Simple, but when some wheels
//         saturate and others don't, the wheel-speed *ratios* change — the
//         robot's actual direction of travel bends away from what was asked.
//
//      b) NORMALIZE: if the largest |scaled| value exceeds 255, divide ALL four
//         by (largest/255). Ratios — and therefore heading — are preserved;
//         the robot just moves slower than asked.
//
//      The tests accept either. Kiva-style choreography cares about heading
//      accuracy, so think about which behavior you want when the production
//      robots are carrying a shelf together.
//
// Keep it integer-or-float as you like; return values must fit int16_t.
// ─────────────────────────────────────────────────────────────────────────────
WheelSpeeds mix(int8_t vx, int8_t vy, int8_t omega) {
    (void)vx;
    (void)vy;
    (void)omega;
    return WheelSpeeds{};  // stub: all zeros — robot won't move until implemented
}

}  // namespace mecanum
