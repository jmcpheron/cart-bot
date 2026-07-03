#pragma once
#include <cstdint>

// Pin map — docs/kitchen-tester/wiring.md. Avoids ESP32 strap pins 0,2,5,12,15.
// A motor spinning backwards in Test 1? Swap its in1/in2 numbers here.

struct MotorPins {
    uint8_t in1, in2, en;
};

constexpr MotorPins kPinsFL{25, 26, 27};
constexpr MotorPins kPinsFR{16, 17, 4};
constexpr MotorPins kPinsRL{33, 32, 23};
constexpr MotorPins kPinsRR{19, 18, 21};

constexpr uint8_t kPinBatterySense = 34;  // input-only, ADC1_CH6
