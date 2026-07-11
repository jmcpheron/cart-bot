#pragma once

// Yaw heading control — pure math, no Arduino dependencies, so it runs in
// host unit tests (pio test -e native) like lib/kinematics.
//
// Sign conventions match mecanum::mix() (docs/project-brief.md): yaw and
// omega are CLOCKWISE-positive viewed from above. The Imu driver applies
// kYawSourceSign so the yaw it publishes already follows this convention.
//
// Yaw angles here are CONTINUOUS (unwrapped): 3 CW turns read +1080°, and
// the hold error is deliberately not folded into ±180 — after a >180°
// disturbance the robot must unwind the way it came, not "shortcut" through
// the wrong side. wrap180() exists for display only.

namespace heading {

// Fold an angle into [-180, 180) for display.
float wrap180(float deg);

struct YawHoldConfig {
    float kp;            // omega units per degree of error
    float ki;            // omega units per degree-second of error
    float i_clamp;       // |integrator| ceiling, omega units
    float out_clamp;     // |trim| ceiling, omega units
    float deadband_deg;  // error treated as zero below this (kills dither)
};

// PI loop that trims omega to hold the heading captured when the robot
// starts translating with no commanded rotation. Call update() every
// control tick; it manages capture/release itself.
class YawHold {
public:
    void configure(const YawHoldConfig& c) { cfg_ = c; }

    // Returns the omega trim to add to the commanded omega.
    //   enabled      gyro healthy and gate allows driving
    //   cmd_omega    operator-commanded rotation (-100..100)
    //   translating  vx or vy nonzero
    //   yaw_deg      continuous yaw from the IMU
    //   dt_s         tick period in seconds
    // Any tick where hold conditions don't apply resets the loop and
    // returns 0; the first tick where they do captures yaw_deg as target.
    int update(bool enabled, int cmd_omega, bool translating, float yaw_deg,
               float dt_s);

    void reset();
    bool holding() const { return holding_; }
    float targetYaw() const { return target_; }

private:
    YawHoldConfig cfg_{2.0f, 0.5f, 15.0f, 30.0f, 0.5f};
    bool holding_ = false;
    float target_ = 0.0f;
    float integ_ = 0.0f;
};

// True once the yaw travelled from start_yaw covers target_delta_deg
// (signed, CW-positive), less stop_early_deg of coasting allowance.
// Overshooting past the target also counts as complete.
bool turnComplete(float start_yaw, float now_yaw, float target_delta_deg,
                  float stop_early_deg);

}  // namespace heading
