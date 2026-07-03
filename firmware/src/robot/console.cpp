#include "console.h"

#include <Arduino.h>
#include <WiFi.h>

#include "battery.h"
#include "comms.h"
#include "kinematics.h"
#include "setpoint.h"

namespace {

SetpointStore* g_store = nullptr;
Battery* g_battery = nullptr;
char g_line[64];
size_t g_len = 0;

void print_help() {
    Serial.println(
        "commands:\n"
        "  v <vx> <vy> <w>   drive: forward, strafe-right, rotate-CW (-100..100)\n"
        "  m <fl|fr|rl|rr> <pwm>   single motor, -255..255 (Test 1 bench mode)\n"
        "  stop              all motors off\n"
        "  demo              scripted Test 2 sequence (3s countdown first)\n"
        "  batt              pack voltage\n"
        "  stats             ESP-NOW packet stats\n"
        "  mac               this robot's MAC (for the transmitter config)\n"
        "  help              this text");
}

// Hold one motion for `ms` while refreshing the watchdog timestamp.
void hold(uint32_t ms) {
    const uint32_t end = millis() + ms;
    while (millis() < end) {
        g_store->touch();
        delay(50);
    }
}

void run_demo() {
    struct Step { const char* name; int8_t vx, vy, w; };
    static const Step kSteps[] = {
        {"forward",      60,   0,   0},
        {"backward",    -60,   0,   0},
        {"strafe right",  0,  60,   0},
        {"strafe left",   0, -60,   0},
        {"rotate CW",     0,   0,  60},
        {"rotate CCW",    0,   0, -60},
        {"diagonal FR",  50,  50,   0},
    };
    Serial.println("[demo] starting in 3s — wheels will move!");
    for (int i = 3; i > 0; i--) {
        Serial.printf("[demo] %d...\n", i);
        delay(1000);
    }
    for (const Step& s : kSteps) {
        Serial.printf("[demo] %s\n", s.name);
        g_store->setVelocity(s.vx, s.vy, s.w);
        hold(2000);
        g_store->setVelocity(0, 0, 0);
        hold(700);
    }
    g_store->setVelocity(0, 0, 0);
    Serial.println("[demo] done");
}

void cmd_motor(const char* which, int pwm) {
    mecanum::WheelSpeeds w{};
    int16_t clamped = static_cast<int16_t>(constrain(pwm, -255, 255));
    if      (strcmp(which, "fl") == 0) w.fl = clamped;
    else if (strcmp(which, "fr") == 0) w.fr = clamped;
    else if (strcmp(which, "rl") == 0) w.rl = clamped;
    else if (strcmp(which, "rr") == 0) w.rr = clamped;
    else { Serial.println("? motor must be fl|fr|rl|rr"); return; }
    g_store->setDirect(w);
    Serial.printf("[m] %s = %d (direct mode; `stop` or `v` to exit)\n", which, clamped);
}

void handle_line(char* line) {
    char* cmd = strtok(line, " ");
    if (!cmd) return;

    if (strcmp(cmd, "help") == 0) {
        print_help();
    } else if (strcmp(cmd, "v") == 0) {
        const char* a = strtok(nullptr, " ");
        const char* b = strtok(nullptr, " ");
        const char* c = strtok(nullptr, " ");
        if (!a || !b || !c) { Serial.println("? usage: v <vx> <vy> <w>"); return; }
        int vx = constrain(atoi(a), -100, 100);
        int vy = constrain(atoi(b), -100, 100);
        int w  = constrain(atoi(c), -100, 100);
        g_store->setVelocity(vx, vy, w);
        Serial.printf("[v] vx=%d vy=%d w=%d\n", vx, vy, w);
    } else if (strcmp(cmd, "m") == 0) {
        const char* which = strtok(nullptr, " ");
        const char* val = strtok(nullptr, " ");
        if (!which || !val) { Serial.println("? usage: m <fl|fr|rl|rr> <pwm>"); return; }
        cmd_motor(which, atoi(val));
    } else if (strcmp(cmd, "stop") == 0) {
        g_store->setVelocity(0, 0, 0);
        Serial.println("[stop]");
    } else if (strcmp(cmd, "demo") == 0) {
        run_demo();
    } else if (strcmp(cmd, "batt") == 0) {
        const float v = g_battery->volts();
        if (v < 0) Serial.println("[batt] sense not wired (kBatterySenseWired=false)");
        else       Serial.printf("[batt] %.2f V\n", v);
    } else if (strcmp(cmd, "stats") == 0) {
        const CommsStats s = comms_stats();
        Serial.printf("[stats] packets=%lu drops=%lu bad_len=%lu last_seq=%u\n",
                      (unsigned long)s.packets, (unsigned long)s.drops,
                      (unsigned long)s.bad_len, s.last_seq);
    } else if (strcmp(cmd, "mac") == 0) {
        Serial.printf("[mac] %s\n", WiFi.macAddress().c_str());
    } else {
        Serial.printf("? unknown: %s (try `help`)\n", cmd);
    }
}

}  // namespace

void console_begin(SetpointStore* store, Battery* battery) {
    g_store = store;
    g_battery = battery;
    print_help();
}

void console_poll() {
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (g_len > 0) {
                g_line[g_len] = '\0';
                handle_line(g_line);
                g_len = 0;
            }
        } else if (g_len < sizeof(g_line) - 1) {
            g_line[g_len++] = c;
        }
    }
}
