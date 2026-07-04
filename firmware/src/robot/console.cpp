#include "console.h"

#include <Arduino.h>
#include <WiFi.h>

#include "battery.h"
#include "comms.h"
#include "kinematics.h"
#include "motors.h"
#include "setpoint.h"

namespace {

SetpointStore* g_store = nullptr;
Battery* g_battery = nullptr;
Motors* g_motors = nullptr;
char g_line[64];
size_t g_len = 0;

void print_help() {
    Serial.println(
        "commands:\n"
        "  v <vx> <vy> <w>   drive (-100..100); holds until Enter\n"
        "  m <fl|fr|rl|rr> <pwm>   single motor, -255..255; holds until Enter\n"
        "  raw <fl|fr|rl|rr> <duty>   same but NO deadband remap (tuning)\n"
        "  stop              all motors off\n"
        "  demo              scripted Test 2 sequence (3s countdown first)\n"
        "  spin [pwm]        wiring check: each wheel in turn, 2s each (default 100)\n"
        "  pindiag           probe RL/RR direction-pin nets (electrical diag)\n"
        "  rltest [duty] [s] sweep all 6 pin-role configs on RL (default 200, 4s)\n"
        "  batt              pack voltage\n"
        "  stats             ESP-NOW packet stats\n"
        "  mac               this robot's MAC (for the transmitter config)\n"
        "  help              this text");
}

// Hold the current motion — feeding the watchdog — until the user presses
// Enter. Without this, a strict failsafe (correctly) kills a one-shot command
// after kCmdTimeoutMs, which would make bench testing miserable.
void hold_until_enter() {
    Serial.println("    (running — press Enter to stop)");
    while (Serial.available()) Serial.read();  // drain leftovers
    for (;;) {
        if (Serial.available()) {
            const char c = static_cast<char>(Serial.read());
            if (c == '\n' || c == '\r') break;
        }
        g_store->touch();
        delay(50);
    }
    g_store->setVelocity(0, 0, 0);
    Serial.println("[stop]");
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

// Wiring-verification sequence: each wheel in turn, labeled, so a full
// four-motor harness check is one command. Wrong wheel moving = crossed
// board/channel; backwards = swap that motor's in1/in2 in pins.h.
void run_spin(int pwm) {
    const int16_t p = static_cast<int16_t>(constrain(pwm, -255, 255));
    static const char* kNames[] = {"FL", "FR", "RL", "RR"};
    Serial.printf("[spin] cycling FL -> FR -> RL -> RR at %d, 2s each\n", p);
    for (int i = 0; i < 4; i++) {
        mecanum::WheelSpeeds w{};
        if (i == 0) w.fl = p;
        else if (i == 1) w.fr = p;
        else if (i == 2) w.rl = p;
        else w.rr = p;
        Serial.printf("[spin] %s\n", kNames[i]);
        g_store->setDirect(w);
        hold(2000);
        g_store->setDirect(mecanum::WheelSpeeds{});
        hold(600);
    }
    g_store->setVelocity(0, 0, 0);
    Serial.println("[spin] done");
}

// Electrical self-probe: read each direction-pin net with internal pulls to
// see what the OUTSIDE world does to it. RR's pins (working channel, same
// module) are the reference fingerprint. A net that reads HIGH under
// pulldown is being driven high externally — e.g. a wire landed on 3V3/EN.
void run_pindiag() {
    struct P { uint8_t pin; const char* label; };
    static const P kPins[] = {
        {33, "RL_IN1"},
        {22, "RL_IN2"},
        {32, "old RL_IN2 (should be unwired)"},
        {19, "RR_IN1 (reference)"},
        {18, "RR_IN2 (reference)"},
    };
    g_store->setVelocity(0, 0, 0);
    delay(100);  // let the motor task settle everything to zero
    Serial.println("[pindiag] sensing each net with internal pulls:");
    for (const P& p : kPins) {
        pinMode(p.pin, INPUT_PULLDOWN);
        delay(20);
        const int down = digitalRead(p.pin);
        pinMode(p.pin, INPUT_PULLUP);
        delay(20);
        const int up = digitalRead(p.pin);
        pinMode(p.pin, OUTPUT);
        digitalWrite(p.pin, LOW);
        Serial.printf("  GPIO %2u  %-28s pulldown->%d  pullup->%d  %s\n",
                      p.pin, p.label, down, up,
                      down == 1 ? "<<< DRIVEN HIGH EXTERNALLY"
                      : up == 0 ? "<<< driven/pulled low externally"
                                : "");
    }
    Serial.println("[pindiag] RL rows should match the RR reference rows.");
}

// Wait `ms` while keeping the watchdog fed (motors are suspended during
// rltest, but this keeps the failsafe gate from spamming the log).
void quiet_wait(uint32_t ms) {
    const uint32_t end = millis() + ms;
    while (millis() < end) {
        g_store->touch();
        delay(100);
    }
}

// Empirical role-finder for RL's three wires: try every assignment of the
// three GPIOs to (IN1, IN2, EN) and drive both bridge states. The config
// that moves the wheel in BOTH directions reveals which module pin each
// wire actually landed on — no multimeter, no wire moves.
void run_rltest(int duty_arg, int secs_arg) {
    const int16_t duty = static_cast<int16_t>(constrain(duty_arg, 60, 255));
    const uint32_t hold_ms = constrain(secs_arg, 1, 15) * 1000UL;
    static const uint8_t kRlPins[3] = {14, 22, 23};
    static const uint8_t kPerm[6][3] = {
        // indices into kRlPins as {in1, in2, en}
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
    };
    const uint8_t kLedcCh = 2;  // RL's LEDC channel, already configured

    Serial.printf("[rltest] sweeping 6 pin-role configs, duty=%d, %lus per direction\n",
                  duty, hold_ms / 1000);
    g_store->setVelocity(0, 0, 0);
    g_motors->setSuspended(true);
    delay(50);

    for (int c = 0; c < 6; c++) {
        const uint8_t in1 = kRlPins[kPerm[c][0]];
        const uint8_t in2 = kRlPins[kPerm[c][1]];
        const uint8_t en  = kRlPins[kPerm[c][2]];
        Serial.printf("\n===== CONFIG %d:  in1=GPIO%u  in2=GPIO%u  en=GPIO%u =====\n",
                      c + 1, in1, in2, en);
        pinMode(in1, OUTPUT); digitalWrite(in1, LOW);
        pinMode(in2, OUTPUT); digitalWrite(in2, LOW);
        ledcAttachPin(en, kLedcCh);
        ledcWrite(kLedcCh, 0);

        Serial.printf("[rltest] config %d FORWARD...\n", c + 1);
        digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
        ledcWrite(kLedcCh, duty);
        quiet_wait(hold_ms);
        ledcWrite(kLedcCh, 0);
        digitalWrite(in1, LOW);
        quiet_wait(1000);

        Serial.printf("[rltest] config %d REVERSE...\n", c + 1);
        digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
        ledcWrite(kLedcCh, duty);
        quiet_wait(hold_ms);
        ledcWrite(kLedcCh, 0);
        digitalWrite(in2, LOW);
        quiet_wait(1000);

        ledcDetachPin(en);
        pinMode(en, OUTPUT); digitalWrite(en, LOW);
    }

    g_motors->begin();  // restore the standard pin/LEDC configuration
    g_motors->setSuspended(false);
    g_store->setVelocity(0, 0, 0);
    Serial.println("\n[rltest] done. Report the CONFIG number that spun BOTH directions.");
}

void cmd_motor(const char* which, int pwm, bool raw) {
    mecanum::WheelSpeeds w{};
    int16_t clamped = static_cast<int16_t>(constrain(pwm, -255, 255));
    if      (strcmp(which, "fl") == 0) w.fl = clamped;
    else if (strcmp(which, "fr") == 0) w.fr = clamped;
    else if (strcmp(which, "rl") == 0) w.rl = clamped;
    else if (strcmp(which, "rr") == 0) w.rr = clamped;
    else { Serial.println("? motor must be fl|fr|rl|rr"); return; }
    g_store->setDirect(w, raw);
    Serial.printf("[%s] %s = %d\n", raw ? "raw" : "m", which, clamped);
    if (clamped != 0) hold_until_enter();
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
        if (vx != 0 || vy != 0 || w != 0) hold_until_enter();
    } else if (strcmp(cmd, "m") == 0 || strcmp(cmd, "raw") == 0) {
        const bool raw = cmd[0] == 'r';
        const char* which = strtok(nullptr, " ");
        const char* val = strtok(nullptr, " ");
        if (!which || !val) { Serial.println("? usage: m|raw <fl|fr|rl|rr> <val>"); return; }
        cmd_motor(which, atoi(val), raw);
    } else if (strcmp(cmd, "stop") == 0) {
        g_store->setVelocity(0, 0, 0);
        Serial.println("[stop]");
    } else if (strcmp(cmd, "demo") == 0) {
        run_demo();
    } else if (strcmp(cmd, "pindiag") == 0) {
        run_pindiag();
    } else if (strcmp(cmd, "rltest") == 0) {
        const char* d = strtok(nullptr, " ");
        const char* t = strtok(nullptr, " ");
        run_rltest(d ? atoi(d) : 200, t ? atoi(t) : 4);
    } else if (strcmp(cmd, "spin") == 0) {
        const char* val = strtok(nullptr, " ");
        run_spin(val ? atoi(val) : 100);
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

void console_begin(SetpointStore* store, Battery* battery, Motors* motors) {
    g_store = store;
    g_battery = battery;
    g_motors = motors;
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
