#include "failsafe.h"

#include <algorithm>

// Policy (jason, 2026-07): graceful degradation on command silence — limp
// between kCmdTimeoutMs and 3×, hard stop after (at 10Hz that's ~5 dropped
// packets before limp, ~15 before stop). Battery mirrors the same ladder
// between warn and cutoff, skipped while the sense divider is unwired
// (volts < 0). Most restrictive gate wins.
DriveGate evaluate_failsafe(const FailsafeInputs& in) {
    DriveGate cmd_gate = DriveGate::kNormal;
    DriveGate battery_gate = DriveGate::kNormal;
    
    // Command timeout with grace period
    if (in.ms_since_last_cmd >= 3 * kCmdTimeoutMs) {
        cmd_gate = DriveGate::kStop;
    } else if (in.ms_since_last_cmd >= kCmdTimeoutMs) {
        cmd_gate = DriveGate::kLimp;
    }
    
    // Battery gate (only if sensor wired; -1 = not yet)
    if (in.battery_volts >= 0) {
        if (in.battery_volts < kBatteryCutoffVolts) {
            battery_gate = DriveGate::kStop;
        } else if (in.battery_volts < kBatteryWarnVolts) {
            battery_gate = DriveGate::kLimp;
        }
    }
    
    return std::max(cmd_gate, battery_gate);
}