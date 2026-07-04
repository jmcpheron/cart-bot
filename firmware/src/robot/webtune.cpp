#include "webtune.h"

#include <Arduino.h>
#include <WebServer.h>

#include "battery.h"
#include "kinematics.h"
#include "setpoint.h"
#include "tuneui.h"

namespace {

WebServer g_server(80);
SetpointStore* g_store = nullptr;
Battery* g_battery = nullptr;

void handle_index() {
    g_server.send_P(200, "text/html", kTuneHtml);
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

}  // namespace

void webtune_begin(SetpointStore* store, Battery* battery) {
    g_store = store;
    g_battery = battery;
    g_server.on("/", handle_index);
    g_server.on("/raw", handle_raw);
    g_server.begin();
}

void webtune_poll() {
    g_server.handleClient();
}
