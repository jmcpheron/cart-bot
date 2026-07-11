// Overhead tracking camera — a dumb streamer, per the roadmap: "ESP32-CAMs
// never do vision themselves; they only stream." All detection happens in
// tools/tracker on the host.
//
// Endpoints:
//   :80/jpg     freshest single JPEG frame (the tracker polls this)
//   :80/status  JSON health: framesize, heap, RSSI, frames served
//   :80/set     runtime tuning: ?framesize=svga&quality=12&aec=1&aec_value=300&agc=1
//   :81/stream  MJPEG multipart — for aiming the camera from a browser
//
// The stream lives on its own httpd instance (port 81) so a browser left
// watching it can never starve the tracker's /jpg polls on port 80.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include "camera_pins.h"

#ifndef HOME_WIFI_SSID
#define HOME_WIFI_SSID ""
#endif
#ifndef HOME_WIFI_PASS
#define HOME_WIFI_PASS ""
#endif

namespace {

constexpr const char* kHostname = "cartbot-cam";
constexpr uint32_t kWifiRebootMs = 15000;  // disconnected this long → restart

httpd_handle_t g_ctrl_httpd = nullptr;
httpd_handle_t g_stream_httpd = nullptr;
uint32_t g_frames_served = 0;
uint32_t g_last_connected_ms = 0;

bool camera_init() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    // SVGA @ quality 12 is the detection sweet spot from the mount height;
    // tools/tracker can push it to UXGA at runtime via /set if markers are
    // marginal. GRAB_LATEST: /jpg always returns the freshest frame, so
    // tracker staleness is bounded to ~1 frame.
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (!psramFound()) {
        // No PSRAM → UXGA is impossible and 2 buffers won't fit.
        config.frame_size = FRAMESIZE_VGA;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        Serial.println("[cam] WARNING: no PSRAM — capped at VGA, single buffer");
    }
    return esp_camera_init(&config) == ESP_OK;
}

esp_err_t handle_jpg(httpd_req_t* req) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    // The robot's drive page (different origin) embeds this feed.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t res = httpd_resp_send(req, reinterpret_cast<const char*>(fb->buf),
                                    fb->len);
    esp_camera_fb_return(fb);
    if (res == ESP_OK) g_frames_served++;
    return res;
}

esp_err_t handle_stream(httpd_req_t* req) {
    static const char* kBoundary =
        "\r\n--cartbotframe\r\n"
        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
    httpd_resp_set_type(req,
                        "multipart/x-mixed-replace;boundary=cartbotframe");
    char part[96];
    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) return ESP_FAIL;
        int len = snprintf(part, sizeof(part), kBoundary,
                           static_cast<unsigned>(fb->len));
        esp_err_t res = httpd_resp_send_chunk(req, part, len);
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(
                req, reinterpret_cast<const char*>(fb->buf), fb->len);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) return res;  // client went away
        g_frames_served++;
    }
}

esp_err_t handle_status(httpd_req_t* req) {
    sensor_t* s = esp_camera_sensor_get();
    char body[256];
    snprintf(body, sizeof(body),
             "{\"framesize\":%d,\"quality\":%d,\"heap\":%u,\"psram\":%u,"
             "\"rssi\":%d,\"frames\":%u,\"uptime_s\":%lu}",
             s ? s->status.framesize : -1, s ? s->status.quality : -1,
             static_cast<unsigned>(ESP.getFreeHeap()),
             static_cast<unsigned>(ESP.getFreePsram()), WiFi.RSSI(),
             static_cast<unsigned>(g_frames_served), millis() / 1000);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

framesize_t parse_framesize(const char* name) {
    if (!strcasecmp(name, "qvga")) return FRAMESIZE_QVGA;
    if (!strcasecmp(name, "vga")) return FRAMESIZE_VGA;
    if (!strcasecmp(name, "svga")) return FRAMESIZE_SVGA;
    if (!strcasecmp(name, "xga")) return FRAMESIZE_XGA;
    if (!strcasecmp(name, "sxga")) return FRAMESIZE_SXGA;
    if (!strcasecmp(name, "uxga")) return FRAMESIZE_UXGA;
    return FRAMESIZE_INVALID;
}

// /set?framesize=uxga&quality=12&aec=0&aec_value=300&agc=1 — lets the tracker
// (or curl) run exposure/resolution experiments without reflashing.
esp_err_t handle_set(httpd_req_t* req) {
    char query[128] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no sensor");
        return ESP_FAIL;
    }
    char val[16];
    if (httpd_query_key_value(query, "framesize", val, sizeof(val)) == ESP_OK) {
        framesize_t fs = parse_framesize(val);
        if (fs != FRAMESIZE_INVALID) s->set_framesize(s, fs);
    }
    if (httpd_query_key_value(query, "quality", val, sizeof(val)) == ESP_OK)
        s->set_quality(s, constrain(atoi(val), 4, 63));
    if (httpd_query_key_value(query, "aec", val, sizeof(val)) == ESP_OK)
        s->set_exposure_ctrl(s, atoi(val));  // 0 = manual (use aec_value)
    if (httpd_query_key_value(query, "aec_value", val, sizeof(val)) == ESP_OK)
        s->set_aec_value(s, constrain(atoi(val), 0, 1200));
    if (httpd_query_key_value(query, "agc", val, sizeof(val)) == ESP_OK)
        s->set_gain_ctrl(s, atoi(val));
    if (httpd_query_key_value(query, "led", val, sizeof(val)) == ESP_OK)
        digitalWrite(FLASH_LED_GPIO_NUM, atoi(val) ? HIGH : LOW);
    return handle_status(req);  // reply with the resulting state
}

void servers_begin() {
    httpd_config_t ctrl_cfg = HTTPD_DEFAULT_CONFIG();
    ctrl_cfg.server_port = 80;
    ctrl_cfg.ctrl_port = 32768;
    if (httpd_start(&g_ctrl_httpd, &ctrl_cfg) == ESP_OK) {
        static const httpd_uri_t jpg = {"/jpg", HTTP_GET, handle_jpg, nullptr};
        static const httpd_uri_t status = {"/status", HTTP_GET, handle_status,
                                           nullptr};
        static const httpd_uri_t set = {"/set", HTTP_GET, handle_set, nullptr};
        httpd_register_uri_handler(g_ctrl_httpd, &jpg);
        httpd_register_uri_handler(g_ctrl_httpd, &status);
        httpd_register_uri_handler(g_ctrl_httpd, &set);
    }
    httpd_config_t stream_cfg = HTTPD_DEFAULT_CONFIG();
    stream_cfg.server_port = 81;
    stream_cfg.ctrl_port = 32769;
    if (httpd_start(&g_stream_httpd, &stream_cfg) == ESP_OK) {
        static const httpd_uri_t stream = {"/stream", HTTP_GET, handle_stream,
                                           nullptr};
        httpd_register_uri_handler(g_stream_httpd, &stream);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);

    pinMode(FLASH_LED_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_LED_GPIO_NUM, LOW);

    if (!camera_init()) {
        Serial.println("[cam] esp_camera_init FAILED — check power (5V/2A) "
                       "and ribbon cable; rebooting in 5s");
        delay(5000);
        ESP.restart();
    }

    if (strlen(HOME_WIFI_SSID) == 0) {
        Serial.println("[cam] no HOME_WIFI_SSID baked in — create secrets.env "
                       "and reflash. Idling.");
        while (true) delay(1000);
    }

    WiFi.mode(WIFI_STA);
    // Modem power-save adds 100s of ms (sometimes seconds) per request —
    // deadly for a polled camera. Trade a little heat for low latency.
    WiFi.setSleep(false);
    // Reduced TX power keeps current bursts inside what a marginal 5V feed
    // (FTDI programmer, thin bench cable) can deliver. The overhead cam sits
    // a few meters from the router — link margin is not the constraint.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setHostname(kHostname);
    WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASS);
    Serial.printf("[cam] joining '%s'", HOME_WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    g_last_connected_ms = millis();

    if (MDNS.begin(kHostname)) MDNS.addService("http", "tcp", 80);
    servers_begin();
    Serial.printf("[cam] ready — frame: http://%s/jpg  stream: http://%s:81/stream\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.localIP().toString().c_str());
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        g_last_connected_ms = millis();
    } else if (millis() - g_last_connected_ms > kWifiRebootMs) {
        Serial.println("[cam] WiFi lost — restarting");
        ESP.restart();
    }
    delay(500);
}
