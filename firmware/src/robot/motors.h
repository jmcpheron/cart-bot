#pragma once
#include "kinematics.h"

// L298N driver interface: per motor, IN1/IN2 pick direction, EN carries PWM.
// Uses ESP32 LEDC channels 0-3 at 20kHz / 8-bit (above audible whine).

class Motors {
public:
    void begin();
    void apply(const mecanum::WheelSpeeds& w);
    // Same, but WITHOUT the deadband remap — raw duty straight to the bridge.
    // For characterizing a motor's true stall threshold from the console.
    void applyRaw(const mecanum::WheelSpeeds& w);
    void stop();
    // While suspended, apply/applyRaw are no-ops. Diagnostics that take
    // direct ownership of motor pins (console `rltest`) use this so the
    // 20ms motor task can't fight their pin writes.
    void setSuspended(bool s) { suspended_ = s; }

private:
    volatile bool suspended_ = false;
};
