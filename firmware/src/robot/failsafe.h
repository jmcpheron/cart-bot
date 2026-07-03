#pragma once
#include <cstdint>

// Failsafe policy — evaluated by the motor task every 20ms loop, BEFORE wheel
// speeds are applied. Whatever this returns gates all motion, including the
// serial console's direct motor commands.

// No fresh command for this long → treat the operator as gone.
constexpr uint32_t kCmdTimeoutMs = 500;
// 2S LiPo limits (3.4V/cell hard floor, 3.5V/cell warning).
constexpr float kBatteryCutoffVolts = 6.8f;
constexpr float kBatteryWarnVolts   = 7.0f;

enum class DriveGate : uint8_t {
    kNormal,  // drive as commanded
    kLimp,    // drive at half the commanded speed (motor task scales by 50%)
    kStop,    // all motors off
};

struct FailsafeInputs {
    uint32_t ms_since_last_cmd;  // age of the newest command (serial or ESP-NOW)
    float battery_volts;         // smoothed pack voltage; -1.0f = sense not wired
};

DriveGate evaluate_failsafe(const FailsafeInputs& in);
