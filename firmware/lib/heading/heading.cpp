#include "heading.h"

#include <cmath>

namespace heading {
namespace {

// std::clamp is C++17; the ESP32 Arduino core builds as gnu++11.
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

float wrap180(float deg) {
    float w = std::fmod(deg + 180.0f, 360.0f);
    if (w < 0.0f) w += 360.0f;
    return w - 180.0f;
}

void YawHold::reset() {
    holding_ = false;
    integ_ = 0.0f;
}

int YawHold::update(bool enabled, int cmd_omega, bool translating,
                    float yaw_deg, float dt_s) {
    if (!enabled || cmd_omega != 0 || !translating) {
        reset();
        return 0;
    }
    if (!holding_) {
        holding_ = true;
        target_ = yaw_deg;
        integ_ = 0.0f;
    }

    float e = target_ - yaw_deg;
    if (std::fabs(e) < cfg_.deadband_deg) e = 0.0f;

    // Conditional integration: while the output is saturated, accumulating
    // more integrator only delays recovery (windup), so freeze it.
    const float p = cfg_.kp * e;
    float u = p + integ_;
    if (std::fabs(u) < cfg_.out_clamp) {
        integ_ = clampf(integ_ + cfg_.ki * e * dt_s, -cfg_.i_clamp,
                        cfg_.i_clamp);
        u = p + integ_;
    }
    u = clampf(u, -cfg_.out_clamp, cfg_.out_clamp);
    return static_cast<int>(std::lround(u));
}

bool turnComplete(float start_yaw, float now_yaw, float target_delta_deg,
                  float stop_early_deg) {
    // Signed progress in the turn's own direction; overshoot still counts.
    const float dir = (target_delta_deg >= 0.0f) ? 1.0f : -1.0f;
    const float progress = (now_yaw - start_yaw) * dir;
    return progress >= std::fabs(target_delta_deg) - stop_early_deg;
}

}  // namespace heading
