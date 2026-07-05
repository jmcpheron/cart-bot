#include "webtune.h"

#include <Arduino.h>
#include <WebServer.h>

#include "battery.h"
#include "dance.h"
#include "driveui.h"
#include "kinematics.h"
#include "motors.h"
#include "robot_state.h"
#include "setpoint.h"
#include "tuneui.h"

namespace {

WebServer g_server(80);
SetpointStore* g_store = nullptr;
Battery* g_battery = nullptr;
Motors* g_motors = nullptr;

// ---- RL pin-role finder (matches CFGS table in tuneui.h) ----
struct RlPerm { uint8_t in1, in2, en; };
const RlPerm kRlPerms[6] = {
    {14, 22, 23}, {14, 23, 22}, {22, 14, 23},
    {22, 23, 14}, {23, 14, 22}, {23, 22, 14},
};
constexpr uint8_t kRlLedcCh = 2;  // RL's LEDC channel
int g_rl_cfg = 0;                 // 0 = finder inactive
uint8_t g_rl_en = 0;
uint32_t g_rl_last_ms = 0;

void rl_stop_and_restore() {
    if (g_rl_cfg == 0) return;
    ledcWrite(kRlLedcCh, 0);
    ledcDetachPin(g_rl_en);
    g_rl_cfg = 0;
    g_rl_en = 0;
    g_motors->begin();            // restore standard pin/LEDC config
    g_motors->setSuspended(false);
    g_store->setVelocity(0, 0, 0);
}

void handle_index() {
    g_server.send_P(200, "text/html", kDriveHtml);
}

void handle_tune() {
    g_server.send_P(200, "text/html", kTuneHtml);
}

// /cmd?vx=..&vy=..&w=.. — velocity from the drive page. Marks a web-drive
// session so ESP-NOW input yields while the page is active.
void handle_cmd() {
    const int8_t vx = static_cast<int8_t>(constrain(g_server.arg("vx").toInt(), -100, 100));
    const int8_t vy = static_cast<int8_t>(constrain(g_server.arg("vy").toInt(), -100, 100));
    const int8_t w  = static_cast<int8_t>(constrain(g_server.arg("w").toInt(), -100, 100));
    if (dance::playing()) dance::stop();  // manual input preempts a dance
    g_store->noteWebDrive();
    g_store->setVelocity(vx, vy, w);

    char buf[96];
    const float v = g_battery ? g_battery->volts() : -1.0f;
    if (v < 0) {
        snprintf(buf, sizeof(buf), "gate %s · vx %d vy %d w %d",
                 gate_name(g_robot_gate), vx, vy, w);
    } else {
        snprintf(buf, sizeof(buf), "gate %s · %.2fV · vx %d vy %d w %d",
                 gate_name(g_robot_gate), v, vx, vy, w);
    }
    g_server.send(200, "text/plain", buf);
}

// /raw?fl=..&fr=..&rl=..&rr=..  — applies raw duties (no deadband remap).
// All-zero request = stop; page goes silent afterwards so the watchdog and
// ESP-NOW driving behave normally when the tuning page is idle.
void handle_raw() {
    auto arg = [&](const char* name) -> int16_t {
        return static_cast<int16_t>(
            constrain(g_server.arg(name).toInt(), -255, 255));
    };
    mecanum::WheelSpeeds w;
    w.fl = arg("fl"); w.fr = arg("fr"); w.rl = arg("rl"); w.rr = arg("rr");
    if (w.fl == 0 && w.fr == 0 && w.rl == 0 && w.rr == 0) {
        g_store->setVelocity(0, 0, 0);
    } else {
        g_store->setDirect(w, /*raw=*/true);
    }
    char buf[64];
    const float v = g_battery ? g_battery->volts() : -1.0f;
    if (v < 0) {
        snprintf(buf, sizeof(buf), "ok · fl %d fr %d rl %d rr %d",
                 w.fl, w.fr, w.rl, w.rr);
    } else {
        snprintf(buf, sizeof(buf), "ok · %.2fV · fl %d fr %d rl %d rr %d",
                 v, w.fl, w.fr, w.rl, w.rr);
    }
    g_server.send(200, "text/plain", buf);
}

// /rl?cfg=N&dir=f|r|s&duty=200 — self-paced pin-role finder. Takes exclusive
// ownership of RL's three GPIOs (motor task suspended) while active; a
// dead-man timer in webtune_poll() restores everything if the page vanishes.
void handle_rl() {
    const String dir_s = g_server.arg("dir");
    const char dir = dir_s.length() ? dir_s[0] : 's';
    const int cfg = g_server.arg("cfg").toInt();
    const int duty = constrain(
        g_server.hasArg("duty") ? g_server.arg("duty").toInt() : 200, 60, 255);

    if (dir == 's' || cfg < 1 || cfg > 6) {
        rl_stop_and_restore();
        g_server.send(200, "text/plain", "stopped");
        return;
    }

    const RlPerm& p = kRlPerms[cfg - 1];
    if (g_rl_cfg == 0) {
        g_store->setVelocity(0, 0, 0);
        g_motors->setSuspended(true);
        delay(30);  // let the motor task finish its current cycle
    }
    if (g_rl_cfg != cfg) {
        if (g_rl_en) { ledcWrite(kRlLedcCh, 0); ledcDetachPin(g_rl_en); }
        pinMode(p.in1, OUTPUT); digitalWrite(p.in1, LOW);
        pinMode(p.in2, OUTPUT); digitalWrite(p.in2, LOW);
        ledcAttachPin(p.en, kRlLedcCh);
        ledcWrite(kRlLedcCh, 0);
        g_rl_cfg = cfg;
        g_rl_en = p.en;
    }
    digitalWrite(p.in1, dir == 'f' ? HIGH : LOW);
    digitalWrite(p.in2, dir == 'r' ? HIGH : LOW);
    ledcWrite(kRlLedcCh, duty);
    g_rl_last_ms = millis();
    g_store->touch();  // keep the failsafe log quiet during the session

    char buf[80];
    snprintf(buf, sizeof(buf), "cfg %d · in1=%u in2=%u en=%u · %s @ %d",
             cfg, p.in1, p.in2, p.en, dir == 'f' ? "FWD" : "REV", duty);
    g_server.send(200, "text/plain", buf);
}

// ---- dance endpoints ----

void handle_dance_upload() {
    const int n = dance::setProgram(g_server.arg("plain").c_str());
    if (n < 0) {
        g_server.send(400, "text/plain", "bad program");
        return;
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "staged %d steps", n);
    g_server.send(200, "text/plain", buf);
}

void handle_dance_play() {
    if (g_server.hasArg("slot")) {
        if (dance::loadSlot(g_server.arg("slot").toInt()).isEmpty()) {
            g_server.send(404, "text/plain", "slot empty");
            return;
        }
    }
    g_store->noteWebDrive();  // pause ESP-NOW input while we spin up
    g_server.send(dance::play() ? 200 : 409, "text/plain",
                  dance::playing() ? "playing" : "nothing to play");
}

void handle_dance_stop() {
    dance::stop();
    g_store->setVelocity(0, 0, 0);
    g_server.send(200, "text/plain", "stopped");
}

void handle_dance_status() {
    char buf[48];
    if (dance::playing()) {
        snprintf(buf, sizeof(buf), "dancing %d/%d",
                 dance::currentStep() + 1, dance::stepCount());
    } else {
        snprintf(buf, sizeof(buf), "idle");
    }
    g_server.send(200, "text/plain", buf);
}

void handle_dance_save() {
    const bool ok = dance::saveSlot(g_server.arg("slot").toInt(),
                                    g_server.arg("name"));
    g_server.send(ok ? 200 : 400, "text/plain", ok ? "saved" : "save failed");
}

void handle_dance_load() {
    const int slot = g_server.arg("slot").toInt();
    const String prog = dance::loadSlot(slot);
    if (prog.isEmpty()) {
        g_server.send(404, "text/plain", "slot empty");
        return;
    }
    g_server.send(200, "text/plain", dance::slotName(slot) + "\n" + prog);
}

void handle_dance_list() {
    String out;
    for (int i = 0; i < dance::kSlots; i++) {
        if (i) out += "|";
        out += String(i) + ":" + dance::slotName(i);
    }
    g_server.send(200, "text/plain", out);
}

}  // namespace

void webtune_begin(SetpointStore* store, Battery* battery, Motors* motors) {
    g_store = store;
    g_battery = battery;
    g_motors = motors;
    g_server.on("/", handle_index);
    g_server.on("/tune", handle_tune);
    g_server.on("/cmd", handle_cmd);
    g_server.on("/raw", handle_raw);
    g_server.on("/rl", handle_rl);
    g_server.on("/dance", HTTP_POST, handle_dance_upload);
    g_server.on("/dance/play", handle_dance_play);
    g_server.on("/dance/stop", handle_dance_stop);
    g_server.on("/dance/status", handle_dance_status);
    g_server.on("/dance/save", handle_dance_save);
    g_server.on("/dance/load", handle_dance_load);
    g_server.on("/dance/list", handle_dance_list);
    g_server.begin();
}

void webtune_poll() {
    g_server.handleClient();
    // Dead-man: page stopped talking mid-run (closed tab, WiFi drop) —
    // stop the motor and hand the pins back.
    if (g_rl_cfg != 0 && millis() - g_rl_last_ms > 1500) {
        rl_stop_and_restore();
    }
}
