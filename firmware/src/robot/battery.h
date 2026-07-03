#pragma once

// Pack voltage via 100k/33k divider into GPIO 34 (docs/kitchen-tester/wiring.md).
// Flip kBatterySenseWired to true once the divider is soldered — with the pin
// floating the reading is noise, so until then volts() returns -1 ("unknown")
// and the failsafe policy should skip battery checks.

constexpr bool kBatterySenseWired = false;

// Divider ratio (Rtop+Rbottom)/Rbottom = 133/33. Calibrate against a multimeter
// in Test 1 by trimming this constant.
constexpr float kDividerRatio = 133.0f / 33.0f;

class Battery {
public:
    void begin();
    // Call periodically (motor task does, every 20ms) to feed the smoothing filter.
    void sample();
    // Smoothed pack voltage, or -1.0f while the sense divider isn't wired.
    float volts() const;

private:
    float smoothed_mv_ = 0;
};
