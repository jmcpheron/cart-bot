#include "kinematics.h"

#include <algorithm>
#include <cmath>

namespace mecanum {

WheelSpeeds mix(int8_t vx, int8_t vy, int8_t omega) {
    const float raw[4] = {
        static_cast<float>(vx) + vy + omega,  // FL
        static_cast<float>(vx) - vy - omega,  // FR
        static_cast<float>(vx) - vy + omega,  // RL
        static_cast<float>(vx) + vy - omega,  // RR
    };

    // Full-scale pure motion (±100 input) maps to full PWM (±255).
    float scale = static_cast<float>(kMaxPwm) / 100.0f;

    // Combined motions can overflow ±255. Rescale ALL four wheels by the same
    // factor rather than clamping each independently: the wheel-speed ratios —
    // and therefore the robot's heading — are preserved, it just moves slower
    // than asked. Heading accuracy matters more than speed once two robots
    // carry a shelf together.
    float max_abs = 0.0f;
    for (float r : raw) max_abs = std::max(max_abs, std::fabs(r));
    if (max_abs * scale > kMaxPwm) scale = kMaxPwm / max_abs;

    WheelSpeeds w;
    w.fl = static_cast<int16_t>(std::lround(raw[0] * scale));
    w.fr = static_cast<int16_t>(std::lround(raw[1] * scale));
    w.rl = static_cast<int16_t>(std::lround(raw[2] * scale));
    w.rr = static_cast<int16_t>(std::lround(raw[3] * scale));
    return w;
}

}  // namespace mecanum
