// Kitchen tester — robot firmware.
//
// Task layout (docs/project-brief.md):
//   Core 0: motor task — 20ms loop: failsafe gate → kinematics → PWM
//   Core 1: Arduino loopTask — serial console; ESP-NOW callbacks arrive on the
//           Wi-Fi task (also core 1 by default) and only touch SetpointStore.

#include <Arduino.h>

#include "battery.h"
#include "comms.h"
#include "config.h"
#include "console.h"
#include "dance.h"
#include "failsafe.h"
#include "heading.h"
#include "imu.h"
#include "kinematics.h"
#include "motors.h"
#include "robot_state.h"
#include "setpoint.h"
#include "webtune.h"

volatile DriveGate g_robot_gate = DriveGate::kStop;
volatile bool g_yaw_holding = false;
volatile int8_t g_yaw_trim = 0;

namespace {

Motors g_motors;
Battery g_battery;
SetpointStore g_setpoint;
Imu g_imu;
heading::YawHold g_yawhold;

constexpr TickType_t kMotorPeriodTicks = pdMS_TO_TICKS(20);

mecanum::WheelSpeeds scale_half(mecanum::WheelSpeeds w) {
    w.fl /= 2; w.fr /= 2; w.rl /= 2; w.rr /= 2;
    return w;
}

void motor_task(void*) {
    TickType_t wake = xTaskGetTickCount();
    DriveGate last_gate = DriveGate::kNormal;
    for (;;) {
        vTaskDelayUntil(&wake, kMotorPeriodTicks);
        g_battery.sample();

        const SetpointStore::Snapshot sp = g_setpoint.get();
        // Commanded-still hint gates the gyro's stationary bias re-zeroing.
        g_imu.sample(!sp.direct && sp.vx == 0 && sp.vy == 0 && sp.omega == 0);
        const DriveGate gate = evaluate_failsafe(
            {sp.age_ms, g_battery.volts()});
        g_robot_gate = gate;
        if (gate != last_gate) {
            Serial.printf("[failsafe] gate -> %s\n", gate_name(gate));
            last_gate = gate;
        }

        if (gate == DriveGate::kStop) {
            g_motors.stop();
            g_yawhold.reset();
            g_yaw_holding = false;
            g_yaw_trim = 0;
            continue;
        }
        // Gyro yaw-hold: while translating with no commanded rotation, trim
        // omega so mecanum skid can't walk the heading. Degrades to trim=0
        // whenever the IMU is absent/unhealthy or wheels are driven direct.
        int trim = 0;
        if (!sp.direct) {
            const Imu::Snapshot im = g_imu.get();
            trim = g_yawhold.update(im.ok, sp.omega,
                                    sp.vx != 0 || sp.vy != 0, im.yaw_deg,
                                    0.020f);
        } else {
            g_yawhold.reset();
        }
        g_yaw_holding = g_yawhold.holding();
        g_yaw_trim = static_cast<int8_t>(trim);
        mecanum::WheelSpeeds w =
            sp.direct ? sp.wheels
                      : mecanum::mix(sp.vx, sp.vy,
                                     static_cast<int8_t>(constrain(
                                         sp.omega + trim, -100, 100)));
        if (gate == DriveGate::kLimp) w = scale_half(w);
        if (sp.direct && sp.raw) {
            g_motors.applyRaw(w);
        } else {
            g_motors.apply(w);
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== cart-bot kitchen tester ===");

    g_motors.begin();
    g_battery.begin();
    g_imu.begin();  // blocks ~1.3s for bias calibration — robot at rest
    g_yawhold.configure(kYawHoldCfg);
    comms_begin(&g_setpoint);
    webtune_begin(&g_setpoint, &g_battery, &g_motors, &g_imu);
    dance::begin(&g_setpoint, &g_imu);
    console_begin(&g_setpoint, &g_battery, &g_motors, &g_imu);

    xTaskCreatePinnedToCore(motor_task, "motor", 4096, nullptr,
                            /*priority=*/3, nullptr, /*core=*/0);
}

void loop() {
    console_poll();
    webtune_poll();
    delay(5);
}
