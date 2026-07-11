#include "comms.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"
#include "dance.h"
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
    // Robot-hosted web sessions (drive page or tuning) and running dances
    // own the motors; ESP-NOW resumes ~1s after they go quiet.
    if (g_store->rawSessionActive() || g_store->webSessionActive() ||
        dance::playing()) return;
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
    // AP_STA: the STA side joins the home LAN when secrets.env bakes in
    // credentials (the overhead-camera tracker sends /cmd over it) and also
    // carries ESP-NOW from the transmitter; the AP side hosts the bench
    // pages and remains the fallback when there is no LAN.
    WiFi.mode(WIFI_AP_STA);
    // No modem power-save: the tracker's 10 Hz /cmd stream needs low latency,
    // and ESP-NOW receive windows suffer under sleep too.
    WiFi.setSleep(false);

    uint8_t channel = proto::kEspNowChannel;
    if (strlen(HOME_WIFI_SSID) > 0) {
        Serial.printf("[comms] joining '%s'", HOME_WIFI_SSID);
        WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASS);
        const uint32_t deadline = millis() + kStaJoinTimeoutMs;
        while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            // The STA link fixes the shared radio channel — the softAP must
            // follow it or the AP silently stops beaconing where clients look.
            channel = static_cast<uint8_t>(WiFi.channel());
            Serial.printf("[comms] LAN http://%s  (mDNS http://cartbot.local)\n",
                          WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[comms] home WiFi timed out — AP-only fallback");
        }
    }

    WiFi.softAP(kTuneApSsid, kTuneApPass, channel);
    Serial.printf("[comms] AP '%s' — drive: http://%s  tuning: /tune\n",
                  kTuneApSsid, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin("cartbot")) {
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("[comms] mDNS failed — use the printed IP");
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("[comms] esp_now_init FAILED");
        return;
    }
    esp_now_register_recv_cb(on_receive);
    Serial.printf("[comms] ESP-NOW up on channel %u, MAC %s\n",
                  channel, WiFi.macAddress().c_str());
    if (channel != proto::kEspNowChannel) {
        Serial.printf("[comms] WARNING: radio follows the router on channel "
                      "%u, not %u — the transmitter link is down until the "
                      "router (or transmitter) moves to this channel\n",
                      channel, proto::kEspNowChannel);
    }
}

CommsStats comms_stats() {
    portENTER_CRITICAL(&g_stats_mux);
    CommsStats s = g_stats;
    portEXIT_CRITICAL(&g_stats_mux);
    return s;
}
