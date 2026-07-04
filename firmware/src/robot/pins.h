#pragma once
#include <cstdint>

// Pin map — docs/kitchen-tester/wiring.md. Avoids ESP32 strap pins 0,2,5,12,15.
// A motor spinning backwards in Test 1? Swap its in1/in2 numbers here.

struct MotorPins {
    uint8_t in1, in2, en;
};

constexpr MotorPins kPinsFL{25, 26, 27};
constexpr MotorPins kPinsFR{16, 17, 4};
// RL is wired 14/22/23 (as-built after the GPIO-33 saga and board swap);
// 32/33 are unused spares on the current board.
constexpr MotorPins kPinsRL{14, 22, 23};
constexpr MotorPins kPinsRR{18, 19, 21};  // in1/in2 swapped vs wiring doc: RR ran reversed on first spin test

constexpr uint8_t kPinBatterySense = 34;  // input-only, ADC1_CH6
