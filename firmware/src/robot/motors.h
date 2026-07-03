#pragma once
#include "kinematics.h"

// L298N driver interface: per motor, IN1/IN2 pick direction, EN carries PWM.
// Uses ESP32 LEDC channels 0-3 at 20kHz / 8-bit (above audible whine).

class Motors {
public:
    void begin();
    void apply(const mecanum::WheelSpeeds& w);
    void stop();
};
