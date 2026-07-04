// Kitchen tester — robot firmware.
//
// Task layout (docs/project-brief.md):
//   Core 0: motor task — 20ms loop: failsafe gate → kinematics → PWM
//   Core 1: Arduino loopTask — serial console; ESP-NOW callbacks arrive on the
//           Wi-Fi task (also core 1 by default) and only touch SetpointStore.

#include <Arduino.h>

#include "battery.h"
#include "comms.h"
#include "console.h"
#include "failsafe.h"
#include "kinematics.h"
#include "motors.h"
#include "setpoint.h"
#include "webtune.h"

namespace {

Motors g_motors;
Battery g_battery;
SetpointStore g_setpoint;

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
        const DriveGate gate = evaluate_failsafe(
            {sp.age_ms, g_battery.volts()});
        if (gate != last_gate) {
            Serial.printf("[failsafe] gate -> %s\n",
                          gate == DriveGate::kNormal ? "NORMAL"
                          : gate == DriveGate::kLimp ? "LIMP" : "STOP");
            last_gate = gate;
        }

        if (gate == DriveGate::kStop) {
            g_motors.stop();
            continue;
        }
        mecanum::WheelSpeeds w =
            sp.direct ? sp.wheels : mecanum::mix(sp.vx, sp.vy, sp.omega);
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
    comms_begin(&g_setpoint);
    webtune_begin(&g_setpoint, &g_battery);
    console_begin(&g_setpoint, &g_battery);

    xTaskCreatePinnedToCore(motor_task, "motor", 4096, nullptr,
                            /*priority=*/3, nullptr, /*core=*/0);
}

void loop() {
    console_poll();
    webtune_poll();
    delay(5);
}
