#include "motors.h"

#include <Arduino.h>

#include "pins.h"

namespace {

constexpr uint32_t kPwmFreqHz = 20000;
constexpr uint8_t kPwmResolutionBits = 8;

struct Channel {
    MotorPins pins;
    uint8_t ledc;
};

const Channel kChannels[] = {
    {kPinsFL, 0},
    {kPinsFR, 1},
    {kPinsRL, 2},
    {kPinsRR, 3},
};

void drive_one(const Channel& ch, int16_t speed) {
    const bool forward = speed >= 0;
    digitalWrite(ch.pins.in1, forward ? HIGH : LOW);
    digitalWrite(ch.pins.in2, forward ? LOW : HIGH);
    uint32_t duty = static_cast<uint32_t>(forward ? speed : -speed);
    if (duty > mecanum::kMaxPwm) duty = mecanum::kMaxPwm;
    ledcWrite(ch.ledc, duty);
}

}  // namespace

void Motors::begin() {
    for (const Channel& ch : kChannels) {
        pinMode(ch.pins.in1, OUTPUT);
        pinMode(ch.pins.in2, OUTPUT);
        digitalWrite(ch.pins.in1, LOW);
        digitalWrite(ch.pins.in2, LOW);
        ledcSetup(ch.ledc, kPwmFreqHz, kPwmResolutionBits);
        ledcAttachPin(ch.pins.en, ch.ledc);
        ledcWrite(ch.ledc, 0);
    }
}

void Motors::apply(const mecanum::WheelSpeeds& w) {
    drive_one(kChannels[0], w.fl);
    drive_one(kChannels[1], w.fr);
    drive_one(kChannels[2], w.rl);
    drive_one(kChannels[3], w.rr);
}

void Motors::stop() {
    apply(mecanum::WheelSpeeds{});
}
