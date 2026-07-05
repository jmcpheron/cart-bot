#include "comms.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"
#include "protocol.h"
#include "setpoint.h"

namespace {

SetpointStore* g_store = nullptr;
portMUX_TYPE g_stats_mux = portMUX_INITIALIZER_UNLOCKED;
CommsStats g_stats;
bool g_seen_any = false;

// Runs in the Wi-Fi task — keep it short: copy, validate, store.
void on_receive(const uint8_t* /*mac*/, const uint8_t* data, int len) {
    if (len != static_cast<int>(sizeof(proto::robot_cmd_t))) {
        portENTER_CRITICAL(&g_stats_mux);
        g_stats.bad_len++;
        portEXIT_CRITICAL(&g_stats_mux);
        return;
    }
    proto::robot_cmd_t cmd;
    memcpy(&cmd, data, sizeof(cmd));

    portENTER_CRITICAL(&g_stats_mux);
    g_stats.packets++;
    if (g_seen_any) {
        const uint16_t expected = g_stats.last_seq + 1;
        g_stats.drops += static_cast<uint16_t>(cmd.seq - expected);
    }
    g_stats.last_seq = cmd.seq;
    g_seen_any = true;
    portEXIT_CRITICAL(&g_stats_mux);

    if (!g_store) return;
    // Robot-hosted web sessions (drive page or tuning) own the motors;
    // ESP-NOW resumes automatically ~1s after the page goes quiet.
    if (g_store->rawSessionActive() || g_store->webSessionActive()) return;
    switch (cmd.cmd) {
        case proto::kCmdDrive:
            g_store->setVelocity(cmd.vx, cmd.vy, cmd.omega);
            break;
        case proto::kCmdStop:
            g_store->setVelocity(0, 0, 0);
            break;
        case proto::kCmdPing:
            g_store->touch();
            break;
        default:
            break;
    }
}

}  // namespace

void comms_begin(SetpointStore* store) {
    g_store = store;
    // AP_STA: the STA side carries ESP-NOW from the transmitter; the AP side
    // hosts the bench-tuning web page. The softAP pins the shared channel.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(kTuneApSsid, kTuneApPass, proto::kEspNowChannel);
    Serial.printf("[comms] AP '%s' — drive: http://%s  tuning: /tune\n",
                  kTuneApSsid, WiFi.softAPIP().toString().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[comms] esp_now_init FAILED");
        return;
    }
    esp_now_register_recv_cb(on_receive);
    Serial.printf("[comms] ESP-NOW up on channel %u, MAC %s\n",
                  proto::kEspNowChannel, WiFi.macAddress().c_str());
}

CommsStats comms_stats() {
    portENTER_CRITICAL(&g_stats_mux);
    CommsStats s = g_stats;
    portEXIT_CRITICAL(&g_stats_mux);
    return s;
}
