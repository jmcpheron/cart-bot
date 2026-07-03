// Kitchen tester — transmitter firmware (the second ESP32).
//
// Hosts a Wi-Fi AP + joystick web page for the phone, and relays the current
// stick position to the robot over ESP-NOW at 10Hz. The softAP pins the Wi-Fi
// channel, which is why proto::kEspNowChannel must match on both ends.
//
// Safety chain: if the phone stops posting /cmd for >400ms the transmitter
// sends zeros (robot stops but stays "alive"); if the transmitter itself dies,
// the robot's own watchdog fires at kCmdTimeoutMs.

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_now.h>

#include "protocol.h"
#include "config.h"
#include "webui.h"

namespace {

WebServer g_server(80);
volatile int8_t g_vx = 0, g_vy = 0, g_w = 0;
volatile uint32_t g_last_web_ms = 0;
uint16_t g_seq = 0;
uint32_t g_sent = 0, g_send_fail = 0;

void handle_index() {
    g_server.send_P(200, "text/html", kIndexHtml);
}

void handle_cmd() {
    g_vx = static_cast<int8_t>(constrain(g_server.arg("vx").toInt(), -100, 100));
    g_vy = static_cast<int8_t>(constrain(g_server.arg("vy").toInt(), -100, 100));
    g_w  = static_cast<int8_t>(constrain(g_server.arg("w").toInt(), -100, 100));
    g_last_web_ms = millis();
    char buf[48];
    snprintf(buf, sizeof(buf), "seq %u · fail %lu", g_seq, (unsigned long)g_send_fail);
    g_server.send(200, "text/plain", buf);
}

void handle_stop() {
    g_vx = 0; g_vy = 0; g_w = 0;
    g_last_web_ms = millis();
    g_server.send(200, "text/plain", "stopped");
}

void send_tick() {
    proto::robot_cmd_t cmd{};
    const bool web_alive = (millis() - g_last_web_ms) < 400;
    cmd.vx = web_alive ? g_vx : 0;
    cmd.vy = web_alive ? g_vy : 0;
    cmd.omega = web_alive ? g_w : 0;
    cmd.cmd = web_alive ? proto::kCmdDrive : proto::kCmdStop;
    cmd.seq = g_seq++;
    if (esp_now_send(kRobotMac, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd)) == ESP_OK) {
        g_sent++;
    } else {
        g_send_fail++;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== cart-bot transmitter ===");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid, kApPass, proto::kEspNowChannel);
    Serial.printf("[ap] SSID %s, join and open http://%s\n",
                  kApSsid, WiFi.softAPIP().toString().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[espnow] init FAILED");
        return;
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, kRobotMac, 6);
    peer.channel = proto::kEspNowChannel;
    peer.ifidx = WIFI_IF_AP;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[espnow] add_peer FAILED");
    }

    g_server.on("/", handle_index);
    g_server.on("/cmd", handle_cmd);
    g_server.on("/stop", handle_stop);
    g_server.begin();
}

void loop() {
    g_server.handleClient();
    static uint32_t last_send = 0;
    if (millis() - last_send >= 100) {  // 10Hz
        last_send = millis();
        send_tick();
    }
    delay(2);
}
