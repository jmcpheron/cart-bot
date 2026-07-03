#pragma once
#include <Arduino.h>

#include "kinematics.h"

// Velocity setpoint shared between the comms/console producers (core 1) and
// the motor task consumer (core 0). Guarded by a spinlock — the critical
// sections are a few instructions, so a mutex would be overkill.
//
// Two modes:
//  * velocity (normal): vx/vy/omega fed through mecanum::mix()
//  * direct (Test 1 bench mode): raw per-wheel speeds, bypassing kinematics
//    so motors can be exercised before mix() is implemented

class SetpointStore {
public:
    void setVelocity(int8_t vx, int8_t vy, int8_t omega) {
        portENTER_CRITICAL(&mux_);
        vx_ = vx; vy_ = vy; omega_ = omega;
        direct_ = false;
        stamp_ms_ = millis();
        portEXIT_CRITICAL(&mux_);
    }

    void setDirect(const mecanum::WheelSpeeds& w) {
        portENTER_CRITICAL(&mux_);
        wheels_ = w;
        direct_ = true;
        stamp_ms_ = millis();
        portEXIT_CRITICAL(&mux_);
    }

    // Refresh the timestamp without changing the setpoint (console `demo`
    // holds a motion while continuing to feed the watchdog).
    void touch() {
        portENTER_CRITICAL(&mux_);
        stamp_ms_ = millis();
        portEXIT_CRITICAL(&mux_);
    }

    struct Snapshot {
        int8_t vx, vy, omega;
        mecanum::WheelSpeeds wheels;
        bool direct;
        uint32_t age_ms;
    };

    Snapshot get() {
        Snapshot s;
        portENTER_CRITICAL(&mux_);
        s.vx = vx_; s.vy = vy_; s.omega = omega_;
        s.wheels = wheels_;
        s.direct = direct_;
        s.age_ms = millis() - stamp_ms_;
        portEXIT_CRITICAL(&mux_);
        return s;
    }

private:
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    int8_t vx_ = 0, vy_ = 0, omega_ = 0;
    mecanum::WheelSpeeds wheels_{};
    bool direct_ = false;
    uint32_t stamp_ms_ = 0;
};
