#pragma once
#include <cstdint>

// Pin map — docs/kitchen-tester/wiring.md. Avoids ESP32 strap pins 0,2,5,12,15.
// A motor spinning backwards? Swap its in1/in2 numbers here.
//
// Corner mapping measured 2026-07-04 (slow spin test): the harness is
// rotated 180° relative to the original wiring doc — the board wired as
// "front" sits at the robot's REAR. Labels below are PHYSICAL corners;
// each owns the pin set that actually reaches it.
//
// FL's channel had its wire roles found empirically (tuning page pin-role
// finder): GPIO 14 lands on that module's ENA, 23/22 are direction inputs —
// L298N headers read ENA,IN1,IN2 (enable FIRST), which shifted the roles
// during wiring.

struct MotorPins {
    uint8_t in1, in2, en;
};

constexpr MotorPins kPinsFL{23, 22, 14};
constexpr MotorPins kPinsFR{19, 18, 21};  // in1/in2 flipped: floor test showed FR ran inverted
constexpr MotorPins kPinsRL{17, 16, 4};   // in1/in2 flipped: floor test showed RL ran inverted
constexpr MotorPins kPinsRR{25, 26, 27};

constexpr uint8_t kPinBatterySense = 34;  // input-only, ADC1_CH6
