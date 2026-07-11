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

// MPU-6050 IMU (GY-521) — the ESP32's default I2C pins 21/22 are taken by
// motors above; the GPIO matrix lets Wire run on any output-capable pins.
// GPIO 32 deliberately avoided: it was the old RL_IN2 and may still have a
// stray wire (see console.cpp pin diagnostic).
// As-built 2026-07-10: SDA landed on 33 and SCL on 13 (found with `imu
// scan`); swapping HERE beats resoldering — either pin drives I2C fine.
constexpr uint8_t kPinImuSda = 33;
constexpr uint8_t kPinImuScl = 13;
