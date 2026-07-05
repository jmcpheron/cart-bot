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

    void setDirect(const mecanum::WheelSpeeds& w, bool raw = false) {
        portENTER_CRITICAL(&mux_);
        wheels_ = w;
        direct_ = true;
        raw_ = raw;
        stamp_ms_ = millis();
        portEXIT_CRITICAL(&mux_);
    }

    // Web drive session: while the robot-hosted drive page is actively
    // commanding, ESP-NOW input is ignored (same racing-streams problem as
    // the raw tuning session below).
    void noteWebDrive() {
        portENTER_CRITICAL(&mux_);
        web_stamp_ms_ = millis();
        portEXIT_CRITICAL(&mux_);
    }

    bool webSessionActive() {
        portENTER_CRITICAL(&mux_);
        const bool active = (millis() - web_stamp_ms_) < 1000;
        portEXIT_CRITICAL(&mux_);
        return active;
    }

    // True while a raw tuning command is fresh. Comms uses this to drop
    // ESP-NOW packets during a tuning session — otherwise the transmitter's
    // 10Hz stop keep-alives interleave with the tuning page's 10Hz raw
    // commands and the motors stutter as the two streams race.
    bool rawSessionActive() {
        portENTER_CRITICAL(&mux_);
        const bool active = direct_ && raw_ && (millis() - stamp_ms_) < 1000;
        portEXIT_CRITICAL(&mux_);
        return active;
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
        bool raw;  // direct mode without deadband remap (tuning)
        uint32_t age_ms;
    };

    Snapshot get() {
        Snapshot s;
        portENTER_CRITICAL(&mux_);
        s.vx = vx_; s.vy = vy_; s.omega = omega_;
        s.wheels = wheels_;
        s.direct = direct_;
        s.raw = raw_;
        s.age_ms = millis() - stamp_ms_;
        portEXIT_CRITICAL(&mux_);
        return s;
    }

private:
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    int8_t vx_ = 0, vy_ = 0, omega_ = 0;
    mecanum::WheelSpeeds wheels_{};
    bool direct_ = false;
    bool raw_ = false;
    uint32_t stamp_ms_ = 0;
    uint32_t web_stamp_ms_ = 0;
};
