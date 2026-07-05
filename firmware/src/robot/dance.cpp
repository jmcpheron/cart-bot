#include "dance.h"

#include <Preferences.h>

#include "setpoint.h"

namespace dance {
namespace {

struct Step {
    int8_t vx, vy, w;
    uint32_t ms;
};

Step g_steps[kMaxSteps];
int g_count = 0;
String g_text;  // staged program text, kept for saveSlot()
volatile int g_current = -1;
volatile bool g_abort = false;
volatile bool g_playRequest = false;
SetpointStore* g_store = nullptr;
Preferences g_prefs;

void dance_task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!g_playRequest) continue;
        g_playRequest = false;
        g_abort = false;
        Serial.printf("[dance] playing %d steps\n", g_count);
        for (int i = 0; i < g_count && !g_abort; i++) {
            g_current = i;
            const Step& s = g_steps[i];
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

void begin(SetpointStore* store) {
    g_store = store;
    g_prefs.begin("dances", false);
    xTaskCreatePinnedToCore(dance_task, "dance", 4096, nullptr,
                            /*priority=*/1, nullptr, /*core=*/1);
}

int setProgram(const char* text) {
    if (playing()) stop();
    if (!text || !*text) return -1;

    char buf[1600];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    Step parsed[kMaxSteps];
    int n = 0;
    uint32_t total = 0;
    char* save = nullptr;
    for (char* tok = strtok_r(buf, ";", &save); tok;
         tok = strtok_r(nullptr, ";", &save)) {
        if (n >= kMaxSteps) return -1;
        int vx, vy, w;
        long ms;
        if (sscanf(tok, "%d,%d,%d,%ld", &vx, &vy, &w, &ms) != 4) return -1;
        ms = constrain(ms, 50L, 15000L);
        total += static_cast<uint32_t>(ms);
        if (total > kMaxTotalMs) return -1;
        parsed[n].vx = static_cast<int8_t>(constrain(vx, -100, 100));
        parsed[n].vy = static_cast<int8_t>(constrain(vy, -100, 100));
        parsed[n].w  = static_cast<int8_t>(constrain(w, -100, 100));
        parsed[n].ms = static_cast<uint32_t>(ms);
        n++;
    }
    if (n == 0) return -1;

    memcpy(g_steps, parsed, sizeof(Step) * n);
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
