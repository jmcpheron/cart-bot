#pragma once
#include <cstdint>

// Pin map — docs/kitchen-tester/wiring.md. Avoids ESP32 strap pins 0,2,5,12,15.
// A motor spinning backwards in Test 1? Swap its in1/in2 numbers here.

struct MotorPins {
    uint8_t in1, in2, en;
};

constexpr MotorPins kPinsFL{25, 26, 27};
constexpr MotorPins kPinsFR{16, 17, 4};
// RL roles found empirically with the tuning page's pin-role finder
// (2026-07-04): GPIO 14 lands on the module's ENA, 23/22 are the direction
// inputs. L298N headers read ENA,IN1,IN2 — enable FIRST — which is how the
// roles shifted during wiring. Direction oriented to match the other wheels.
constexpr MotorPins kPinsRL{23, 22, 14};
constexpr MotorPins kPinsRR{18, 19, 21};  // in1/in2 swapped vs wiring doc: RR ran reversed on first spin test

constexpr uint8_t kPinBatterySense = 34;  // input-only, ADC1_CH6
