#include "motors.h"

#include <Arduino.h>

#include "pins.h"

namespace {

// The L298N's slow BJT bridge can't switch cleanly at 20kHz — partial duties
// deliver far less than their nominal voltage and the motor only moves near
// 100%. 1kHz gives usable proportional control at the cost of audible whine.
constexpr uint32_t kPwmFreqHz = 1000;
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

// Measured on the bench (2026-07-03, unloaded TT motor, 1kHz, 2S pack): below
// duty 100 the motor hums but can't break the 1:48 gearbox's static friction.
// Remap all nonzero commands into the band that actually moves, so "slow"
// means "slowest real speed" instead of "stall and whine". Expect to retune
// once the robot carries its own weight; encoders make this obsolete in the
// production build.
constexpr uint32_t kMinDuty = 100;

void drive_one(const Channel& ch, int16_t speed, bool remap) {
    const bool forward = speed >= 0;
    digitalWrite(ch.pins.in1, forward ? HIGH : LOW);
    digitalWrite(ch.pins.in2, forward ? LOW : HIGH);
    uint32_t duty = static_cast<uint32_t>(forward ? speed : -speed);
    if (duty > mecanum::kMaxPwm) duty = mecanum::kMaxPwm;
    if (remap && duty > 0)
        duty = kMinDuty + duty * (mecanum::kMaxPwm - kMinDuty) / mecanum::kMaxPwm;
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
    if (suspended_) return;
    drive_one(kChannels[0], w.fl, true);
    drive_one(kChannels[1], w.fr, true);
    drive_one(kChannels[2], w.rl, true);
    drive_one(kChannels[3], w.rr, true);
}

void Motors::applyRaw(const mecanum::WheelSpeeds& w) {
    if (suspended_) return;
    drive_one(kChannels[0], w.fl, false);
    drive_one(kChannels[1], w.fr, false);
    drive_one(kChannels[2], w.rl, false);
    drive_one(kChannels[3], w.rr, false);
}

void Motors::stop() {
    apply(mecanum::WheelSpeeds{});
}
