#include "imu.h"

#include <Wire.h>

#include "pins.h"

namespace {

constexpr uint8_t kRegSmplrtDiv = 0x19;
constexpr uint8_t kRegConfig = 0x1A;
constexpr uint8_t kRegGyroConfig = 0x1B;
constexpr uint8_t kRegAccelConfig = 0x1C;
constexpr uint8_t kRegAccelXoutH = 0x3B;  // start of the 14-byte burst
constexpr uint8_t kRegGyroXoutH = 0x43;   // X/Y/Z burst for bias calibration
constexpr uint8_t kRegSignalPathReset = 0x68;
constexpr uint8_t kRegPwrMgmt1 = 0x6B;
constexpr uint8_t kRegWhoAmI = 0x75;

// Mid-run failure policy: this many consecutive bad reads marks the sensor
// dead (yaw features off), then a re-init is attempted every 5s in case a
// loose wire got re-seated.
constexpr int kMaxConsecFails = 10;
constexpr uint32_t kReinitIntervalMs = 5000;

}  // namespace

bool Imu::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(kImuAddr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool Imu::readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(kImuAddr);
    Wire.write(reg);
    if (Wire.endTransmission(/*sendStop=*/false) != 0) return false;
    if (Wire.requestFrom(kImuAddr, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

bool Imu::initChip() {
    uint8_t who = 0;
    if (!readRegs(kRegWhoAmI, &who, 1)) return false;
    if (who != 0x68) {
        // Some GY-521 clones answer with odd IDs; log what we saw so a
        // working clone can be whitelisted later instead of guessed at.
        Serial.printf("[imu] WHO_AM_I=0x%02X, expected 0x68 — disabled\n", who);
        return false;
    }
    bool ok = writeReg(kRegPwrMgmt1, 0x80);  // device reset
    delay(100);
    ok &= writeReg(kRegPwrMgmt1, 0x01);  // wake, CLKSEL=1: PLL from X gyro
    ok &= writeReg(kRegSignalPathReset, 0x07);
    delay(100);
    ok &= writeReg(kRegConfig, 0x03);       // DLPF 44Hz — cuts motor vibration
    ok &= writeReg(kRegSmplrtDiv, 9);       // 1kHz/(1+9) = 100Hz, 2× loop rate
    ok &= writeReg(kRegGyroConfig, 0x00);   // ±250 dps → 131 LSB/dps
    ok &= writeReg(kRegAccelConfig, 0x00);  // ±2g (future tilt-abort)
    return ok;
}

bool Imu::calibrate() {
    // All three gyro axes: X drives yaw in this mounting (see MOUNTING MAP),
    // Y/Z feed the debug panel but read a few dps of offset without bias.
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    int16_t mn = INT16_MAX, mx = INT16_MIN;  // at-rest spread, yaw axis (X)
    int got = 0;
    for (int i = 0; i < kImuCalibSamples; i++) {
        uint8_t b[6];
        if (readRegs(kRegGyroXoutH, b, 6)) {
            const int16_t gx = static_cast<int16_t>((b[0] << 8) | b[1]);
            const int16_t gy = static_cast<int16_t>((b[2] << 8) | b[3]);
            const int16_t gz = static_cast<int16_t>((b[4] << 8) | b[5]);
            sum_x += gx;
            sum_y += gy;
            sum_z += gz;
            mn = min(mn, gx);
            mx = max(mx, gx);
            got++;
        }
        delay(2);
    }
    if (got < kImuCalibSamples / 2) {
        Serial.printf("[imu] bias calibration got %d/%d reads — disabled\n",
                      got, kImuCalibSamples);
        return false;
    }
    bias_x_raw_ = static_cast<float>(sum_x) / got;
    bias_y_raw_ = static_cast<float>(sum_y) / got;
    bias_raw_ = static_cast<float>(sum_z) / got;
    const float spread_dps = (mx - mn) / kGyroLsbPerDps;
    Serial.printf("[imu] yaw (gyro-X) bias %.2f dps (spread %.2f dps, %d samples)\n",
                  bias_x_raw_ / kGyroLsbPerDps, spread_dps, got);
    if (spread_dps > kImuCalibMaxSpreadDps) {
        Serial.println("[imu] WARNING: bias spread high — robot not at rest?");
    }
    return true;
}

void Imu::begin() {
    if (ok()) return;  // already healthy (console `imu init` on a live gyro)
    Wire.begin(kPinImuSda, kPinImuScl, 400000);
    if (!initChip()) {
        Serial.println("[imu] not found — yaw features disabled");
        return;
    }
    Serial.println("[imu] WHO_AM_I=0x68 ok");
    if (!calibrate()) return;
    last_us_ = micros();
    // Publish last: once ok_ is true the motor task starts sampling, so the
    // chip and bias must be fully ready before this flips.
    portENTER_CRITICAL(&mux_);
    consec_fails_ = 0;
    calibrated_ = true;
    ok_ = true;
    portEXIT_CRITICAL(&mux_);
}

void Imu::sample(bool commanded_still) {
    if (!ok_) {
        // Re-init only makes sense after a mid-run drop: a boot-time bias is
        // required, and we can't recalibrate while the robot may be moving.
        const uint32_t now = millis();
        if (calibrated_ && now - last_reinit_ms_ >= kReinitIntervalMs) {
            last_reinit_ms_ = now;
            if (initChip()) {
                Serial.println("[imu] re-init ok — reusing boot bias");
                consec_fails_ = 0;
                last_us_ = micros();
                portENTER_CRITICAL(&mux_);
                ok_ = true;
                portEXIT_CRITICAL(&mux_);
            }
        }
        return;
    }

    uint8_t raw[14];
    const uint32_t now_us = micros();
    const bool read_ok = readRegs(kRegAccelXoutH, raw, sizeof(raw));
    if (!read_ok) {
        consec_fails_++;
        still_ticks_ = 0;  // can't confirm stillness without data
        const bool dead = consec_fails_ >= kMaxConsecFails;
        if (dead) Serial.println("[imu] lost — yaw features disabled");
        portENTER_CRITICAL(&mux_);
        errors_++;
        if (dead) ok_ = false;
        portEXIT_CRITICAL(&mux_);
        last_us_ = now_us;  // don't integrate across the gap on recovery
        return;
    }
    consec_fails_ = 0;

    // Burst layout: accel x/y/z, temp, gyro x/y/z — all big-endian int16.
    const int16_t ax = static_cast<int16_t>((raw[0] << 8) | raw[1]);
    const int16_t ay = static_cast<int16_t>((raw[2] << 8) | raw[3]);
    const int16_t az = static_cast<int16_t>((raw[4] << 8) | raw[5]);
    const int16_t tr = static_cast<int16_t>((raw[6] << 8) | raw[7]);
    const int16_t gx = static_cast<int16_t>((raw[8] << 8) | raw[9]);
    const int16_t gy = static_cast<int16_t>((raw[10] << 8) | raw[11]);
    const int16_t gz = static_cast<int16_t>((raw[12] << 8) | raw[13]);
    // Yaw comes from chip gyro-X in this mounting (MOUNTING MAP, imu.h).
    const float dps = kYawSourceSign * (gx - bias_x_raw_) / kGyroLsbPerDps;

    // Debug-panel values, remapped chip → body: up=+X, fwd=+Y, right=+Z.
    // Accel is EMA-smoothed; the pitch/roll derived from it is the GRAVITY
    // direction, so real acceleration shows up as false tilt — fine for a
    // display, never feed it to control.
    ax_f_ += kAccelEmaAlpha * (ax - ax_f_);
    ay_f_ += kAccelEmaAlpha * (ay - ay_f_);
    az_f_ += kAccelEmaAlpha * (az - az_f_);
    const float a_fwd = kFwdSign * ay_f_ / kAccelLsbPerG;
    const float a_right = kRightSign * az_f_ / kAccelLsbPerG;
    const float a_up = kUpSign * ax_f_ / kAccelLsbPerG;
    constexpr float kRad2Deg = 57.29578f;
    const float pitch =
        atan2f(-a_fwd, sqrtf(a_right * a_right + a_up * a_up)) * kRad2Deg;
    const float roll = atan2f(a_right, a_up) * kRad2Deg;
    const float roll_rate = kRollRateSign * (gy - bias_y_raw_) / kGyroLsbPerDps;
    const float pitch_rate = kPitchRateSign * (gz - bias_raw_) / kGyroLsbPerDps;
    const float temp_c = tr / 340.0f + 36.53f;
    float dt = (now_us - last_us_) * 1e-6f;
    last_us_ = now_us;
    if (dt < 0.0f || dt > 0.05f) dt = 0.02f;  // wrap/hiccup: assume one tick

    // Stationary re-zeroing: commanded still AND the rate is small enough to
    // be bias, sustained one second. A hand-spin while idle exceeds the
    // threshold and drops us straight back to normal integration.
    if (commanded_still && fabsf(dps) < kStillThresholdDps) {
        if (still_ticks_ < kStillConfirmTicks) still_ticks_++;
    } else {
        still_ticks_ = 0;
    }
    const bool still = still_ticks_ >= kStillConfirmTicks;
    // Re-learn the YAW-source bias (gyro-X in this mounting).
    if (still) bias_x_raw_ += kBiasAlpha * (gx - bias_x_raw_);

    portENTER_CRITICAL(&mux_);
    gyro_z_dps_ = dps;
    if (!still) yaw_deg_ += dps * dt;  // yaw frozen while confirmed still
    still_ = still;
    gyro_x_dps_ = roll_rate;
    gyro_y_dps_ = pitch_rate;
    accel_x_g_ = a_fwd;
    accel_y_g_ = a_right;
    accel_z_g_ = a_up;
    pitch_deg_ = pitch;
    roll_deg_ = roll;
    temp_c_ = temp_c;
    portEXIT_CRITICAL(&mux_);
}

Imu::Snapshot Imu::get() {
    Snapshot s;
    portENTER_CRITICAL(&mux_);
    s.yaw_deg = yaw_deg_;
    s.gyro_z_dps = gyro_z_dps_;
    s.gyro_x_dps = gyro_x_dps_;
    s.gyro_y_dps = gyro_y_dps_;
    s.accel_x_g = accel_x_g_;
    s.accel_y_g = accel_y_g_;
    s.accel_z_g = accel_z_g_;
    s.pitch_deg = pitch_deg_;
    s.roll_deg = roll_deg_;
    s.temp_c = temp_c_;
    s.ok = ok_;
    s.still = still_;
    s.errors = errors_;
    portEXIT_CRITICAL(&mux_);
    return s;
}

bool Imu::ok() {
    portENTER_CRITICAL(&mux_);
    const bool v = ok_;
    portEXIT_CRITICAL(&mux_);
    return v;
}

void Imu::zeroYaw() {
    portENTER_CRITICAL(&mux_);
    yaw_deg_ = 0.0f;
    portEXIT_CRITICAL(&mux_);
}
