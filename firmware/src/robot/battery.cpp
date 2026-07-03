#include "battery.h"

#include <Arduino.h>

#include "pins.h"

namespace {
// Exponential moving average; ~1s time constant at a 20ms sample rate.
constexpr float kAlpha = 0.02f;
}

void Battery::begin() {
    if (!kBatterySenseWired) return;
    analogSetPinAttenuation(kPinBatterySense, ADC_11db);
    smoothed_mv_ = analogReadMilliVolts(kPinBatterySense);
}

void Battery::sample() {
    if (!kBatterySenseWired) return;
    const float mv = analogReadMilliVolts(kPinBatterySense);
    smoothed_mv_ += kAlpha * (mv - smoothed_mv_);
}

float Battery::volts() const {
    if (!kBatterySenseWired) return -1.0f;
    return smoothed_mv_ * kDividerRatio / 1000.0f;
}
