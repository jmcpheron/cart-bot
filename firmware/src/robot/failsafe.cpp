#include "failsafe.h"

// ─────────────────────────────────────────────────────────────────────────────
// TODO(jason): implement the failsafe policy.
//
// This is a safety-behavior decision, and it carries straight into the
// production robots (where "stop" means a 500 lb shelf stops moving). Things
// to decide:
//
//   * Command silence: past kCmdTimeoutMs the operator is presumed gone.
//     Hard kStop? Or a grace band (e.g. kLimp between 500ms and 1s, then
//     kStop) so a single dropped packet burst doesn't slam the wheels?
//     Note ESP-NOW sends at 10Hz — 500ms is five missed packets in a row.
//
//   * Low battery: below kBatteryCutoffVolts, keep driving, limp, or stop?
//     A hard stop protects the LiPo but strands the robot mid-floor; kLimp
//     halves current draw, which also lets the sagging voltage recover —
//     but a limping robot can hide the problem. Where between
//     kBatteryWarnVolts and kBatteryCutoffVolts do you start intervening?
//
//   * Unknown battery: battery_volts < 0 means the sense divider isn't wired
//     yet. Trusting an unwired sensor would keep the robot permanently
//     stopped — you almost certainly want to skip battery checks in that case.
//
//   * Precedence: if both conditions fire, which wins? (Hint: the most
//     restrictive gate should always win.)
//
// The stub below returns kStop unconditionally, so the robot will not move —
// even on the bench — until you replace it. That's deliberate.
// ─────────────────────────────────────────────────────────────────────────────
DriveGate evaluate_failsafe(const FailsafeInputs& in) {
    (void)in;
    return DriveGate::kStop;
}
