#pragma once
#include <Arduino.h>

// MPU-6050 (GY-521 breakout) over I2C — raw register driver, no library.
// Wiring: VCC→3V3, GND→GND, SDA→kPinImuSda, SCL→kPinImuScl, AD0 floating.
//
// Ownership model mirrors Battery: begin() once from setup() (blocks ~1.3s
// for reset delays + bias calibration — robot must be AT REST), then
// sample() every 20ms motor tick. While ok() the motor task is the ONLY
// Wire user, so the bus needs no locking; cross-core readers use the
// spinlock Snapshot. begin() may be called again later (console `imu init`
// after a wiring fix): it publishes ok_ only after calibration completes,
// so the motor task can't start sampling a half-configured chip.
//
// Missing/broken sensor is not fatal: ok() goes false, yaw features degrade
// (trim off, turn steps skipped) and driving continues open-loop.

constexpr uint8_t kImuAddr = 0x68;              // AD0 low/floating
constexpr int kImuCalibSamples = 500;           // ~1s of bias averaging
constexpr float kImuCalibMaxSpreadDps = 2.0f;   // above this: "not at rest?"
constexpr float kGyroLsbPerDps = 131.0f;        // ±250 dps range

// Stationary re-zeroing (ZUPT): the boot bias goes stale as the chip warms
// (measured 2026-07-10: 0.25°/min drift cold → ~10°/min after warm-up, a
// phantom rotation the yaw-hold loop then "corrects" into a real arc). So
// whenever the robot is COMMANDED still and the gyro agrees it isn't
// rotating for kStillConfirmTicks, the bias is slowly re-learned (EMA,
// ~10s time constant) and yaw integration freezes.
constexpr float kStillThresholdDps = 1.5f;  // |rate| below this can be bias
constexpr int kStillConfirmTicks = 50;      // 1s at the 50Hz motor tick
constexpr float kBiasAlpha = 0.002f;        // per tick; tau = 1/(a*50) = 10s

// MOUNTING MAP v2 (2026-07-10). The GY-521 is mounted VERTICALLY on the
// cart, header pins toward the rear. Measured with the robot flat at rest:
// gravity reads +1g on chip X (accel reads +1g on the axis pointing UP),
// and a flat in-place spin registers on chip gyro-X — NOT gyro-Z, which the
// original flat-mount code integrated (yaw stood still through a full 360°
// hand spin; the bench check that set the old kGyroZSign was done before
// the board was installed vertically).
//
// Everything downstream (Snapshot, /imu JSON, the panel) speaks BODY frame:
//   body up      = chip +X   (measured)
//   body forward = chip +Y   (GUESS — verify below)
//   body right   = chip +Z   (GUESS — verify below)
// Conventions: yaw CW-positive looking down (kinematics.h); +pitch = nose
// up; +roll = right side down.
//
// Bench check on the drive page's gyroscope panel, robot flat on the floor:
//   1. spin CW in place        → yaw INCREASES, cart rotates CW
//   2. tilt the NOSE down      → pitch goes NEGATIVE
//   3. tilt the RIGHT side down→ roll goes POSITIVE
// A wrong direction: flip the matching sign below. Pitch and roll answering
// to the OTHER tilt: swap the ay/az roles in sample()'s remap block.
constexpr float kYawSourceSign = -1.0f;  // chip +X up ⇒ CW spin reads negative gx
constexpr float kFwdSign = 1.0f;         // body fwd from chip +Y
constexpr float kRightSign = 1.0f;       // body right from chip +Z
constexpr float kUpSign = 1.0f;          // body up from chip +X (measured)
constexpr float kRollRateSign = 1.0f;    // roll rate from chip gyro-Y
constexpr float kPitchRateSign = 1.0f;   // pitch rate from chip gyro-Z
constexpr float kAccelLsbPerG = 16384.0f;  // ±2g range (kRegAccelConfig=0x00)
// Display smoothing for accel (and the tilt derived from it): EMA per 20ms
// tick, tau ≈ 0.1s. Damps motor vibration without making the bubble laggy.
constexpr float kAccelEmaAlpha = 0.2f;

class Imu {
public:
    void begin();   // Wire + WHO_AM_I + config registers + bias calibration
    // Motor task (core 0) only. commanded_still: the setpoint is zero
    // velocity — enables stationary bias re-learning (see above).
    void sample(bool commanded_still);

    struct Snapshot {
        float yaw_deg;     // continuous/unwrapped, CW-positive
        float gyro_z_dps;  // bias-corrected yaw rate, CW-positive
        float gyro_x_dps;  // roll rate, bias-corrected, body frame
        float gyro_y_dps;  // pitch rate, bias-corrected, body frame
        float accel_x_g;   // body FORWARD, EMA-smoothed (see MOUNTING MAP)
        float accel_y_g;   // body RIGHT
        float accel_z_g;   // body UP — ~+1g at rest, level
        float pitch_deg;   // from smoothed accel — gravity direction, so
        float roll_deg;    //   acceleration reads as false tilt (display use)
        float temp_c;      // die temperature
        bool ok;
        bool still;        // confirmed stationary: re-zeroing, yaw frozen
        uint32_t errors;   // lifetime failed reads
    };
    Snapshot get();        // safe from any core

    bool ok();
    // Bias of the yaw-source axis (gyro-X in this mounting).
    float biasDps() const { return bias_x_raw_ / kGyroLsbPerDps; }
    void zeroYaw();

    // Push the periodic re-init attempt at least one interval away, so a
    // console bus scan can't collide with it (uint32 store is atomic).
    void deferReinit() { last_reinit_ms_ = millis(); }

private:
    bool writeReg(uint8_t reg, uint8_t val);
    bool readRegs(uint8_t reg, uint8_t* buf, uint8_t n);
    bool initChip();
    bool calibrate();

    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    // Published state (behind mux_):
    float yaw_deg_ = 0.0f;
    float gyro_z_dps_ = 0.0f;
    float gyro_x_dps_ = 0.0f;
    float gyro_y_dps_ = 0.0f;
    float accel_x_g_ = 0.0f;
    float accel_y_g_ = 0.0f;
    float accel_z_g_ = 1.0f;
    float pitch_deg_ = 0.0f;
    float roll_deg_ = 0.0f;
    float temp_c_ = 0.0f;
    bool ok_ = false;
    bool still_ = false;
    uint32_t errors_ = 0;
    // Motor-task-private state:
    float bias_raw_ = 0.0f;
    float bias_x_raw_ = 0.0f;
    float bias_y_raw_ = 0.0f;
    // Accel EMA state (raw CHIP-frame LSB); chip X is body-up in this
    // mounting, so X is seeded at 1g so the first published pitch/roll
    // isn't a wild swing from zero.
    float ax_f_ = kAccelLsbPerG;
    float ay_f_ = 0.0f;
    float az_f_ = 0.0f;
    bool calibrated_ = false;
    int still_ticks_ = 0;
    int consec_fails_ = 0;
    uint32_t last_us_ = 0;
    uint32_t last_reinit_ms_ = 0;
};
