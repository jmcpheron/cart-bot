#include "dance.h"

#include <Preferences.h>

#include "config.h"
#include "heading.h"
#include "imu.h"
#include "setpoint.h"

namespace dance {
namespace {

dancescript::Step g_steps[kMaxSteps];
int g_count = 0;
String g_text;  // staged program text, kept for saveSlot()
volatile int g_current = -1;
volatile bool g_abort = false;
volatile bool g_playRequest = false;
SetpointStore* g_store = nullptr;
Imu* g_imu = nullptr;
Preferences g_prefs;

// Gyro-terminated turn: rotate until the yaw delta is covered. Polls at
// 10ms (not the 50ms of timed steps) — at ~100°/s a 50ms poll would coast
// ~5° past the target. s.ms carries the stall timeout.
void run_turn_step(int i, const dancescript::Step& s) {
    Imu::Snapshot im = g_imu->get();
    if (!im.ok) {
        Serial.printf("[dance] step %d: turn skipped (no gyro)\n", i);
        return;
    }
    const float start = im.yaw_deg;
    g_store->setVelocity(0, 0, s.w);
    const uint32_t deadline = millis() + s.ms;
    bool done = false;
    while (millis() < deadline && !g_abort) {
        g_store->touch();  // feed the watchdog; failsafe stays armed
        im = g_imu->get();
        if (!im.ok) break;  // gyro died mid-turn: don't spin blind
        if (heading::turnComplete(start, im.yaw_deg, s.turn_deg,
                                  kTurnStopEarlyDeg)) {
            done = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    g_store->setVelocity(0, 0, 0);  // stop rotating before the next step
    Serial.printf("[dance] step %d: turned %.1f of %.1f deg%s\n", i,
                  static_cast<double>(g_imu->get().yaw_deg - start),
                  static_cast<double>(s.turn_deg),
                  done ? "" : (g_abort ? " (aborted)" : " (timeout/no gyro)"));
}

void dance_task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!g_playRequest) continue;
        g_playRequest = false;
        g_abort = false;
        Serial.printf("[dance] playing %d steps\n", g_count);
        for (int i = 0; i < g_count && !g_abort; i++) {
            g_current = i;
            const dancescript::Step& s = g_steps[i];
            if (s.turn) {
                run_turn_step(i, s);
                continue;
            }
            g_store->setVelocity(s.vx, s.vy, s.w);
            const uint32_t end = millis() + s.ms;
            while (millis() < end && !g_abort) {
                g_store->touch();  // feed the watchdog; failsafe stays armed
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        g_store->setVelocity(0, 0, 0);
        Serial.println(g_abort ? "[dance] aborted" : "[dance] done");
        g_current = -1;
    }
}

}  // namespace

void begin(SetpointStore* store, Imu* imu) {
    g_store = store;
    g_imu = imu;
    g_prefs.begin("dances", false);
    xTaskCreatePinnedToCore(dance_task, "dance", 4096, nullptr,
                            /*priority=*/1, nullptr, /*core=*/1);
}

int setProgram(const char* text) {
    if (playing()) stop();

    dancescript::Step parsed[kMaxSteps];
    const int n =
        dancescript::parse(text, parsed, kMaxSteps, kDanceTurnTimeoutMs);
    if (n < 0) return -1;

    memcpy(g_steps, parsed, sizeof(dancescript::Step) * n);
    g_count = n;
    g_text = text;
    return n;
}

bool play() {
    if (g_count == 0 || playing()) return false;
    g_playRequest = true;
    return true;
}

void stop() {
    g_abort = true;
    g_playRequest = false;
}

bool playing() { return g_current >= 0 || g_playRequest; }
int currentStep() { return g_current; }
int stepCount() { return g_count; }

static String key(const char* prefix, int slot) {
    return String(prefix) + String(slot);
}

bool saveSlot(int slot, const String& name) {
    if (slot < 0 || slot >= kSlots || g_text.isEmpty()) return false;
    g_prefs.putString(key("p", slot).c_str(), g_text);
    g_prefs.putString(key("n", slot).c_str(), name);
    return true;
}

String loadSlot(int slot) {
    if (slot < 0 || slot >= kSlots) return "";
    String prog = g_prefs.getString(key("p", slot).c_str(), "");
    if (!prog.isEmpty()) setProgram(prog.c_str());
    return prog;
}

String slotName(int slot) {
    if (slot < 0 || slot >= kSlots) return "";
    return g_prefs.getString(key("n", slot).c_str(), "");
}

}  // namespace dance
