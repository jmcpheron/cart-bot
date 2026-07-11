#pragma once

#include <cstdint>

#include "heading.h"

// The robot's own access point — used by the bench-tuning web page.
// Must share proto::kEspNowChannel so the transmitter link keeps working.
constexpr const char* kTuneApSsid = "cartbot-robot";
constexpr const char* kTuneApPass = "cartbot123";

// Home-LAN credentials, injected from ../secrets.env by load_secrets.py at
// build time. Empty SSID → AP-only, exactly the pre-tracker behavior.
#ifndef HOME_WIFI_SSID
#define HOME_WIFI_SSID ""
#endif
#ifndef HOME_WIFI_PASS
#define HOME_WIFI_PASS ""
#endif

// How long to wait for the home router before falling back to AP-only.
constexpr uint32_t kStaJoinTimeoutMs = 8000;

// --- MPU-6050 yaw features ---------------------------------------------
// Yaw-hold PI gains (heading::YawHold) — trims omega whenever the robot
// translates with no commanded rotation. Starting values; tune kp/ki on the
// kitchen floor with the A/B strafe test before trusting them.
constexpr heading::YawHoldConfig kYawHoldCfg{
    /*kp=*/2.0f, /*ki=*/0.5f, /*i_clamp=*/15.0f, /*out_clamp=*/30.0f,
    /*deadband_deg=*/0.5f};

// Gyro-terminated dance turns ("t,deg,w" steps): stall guard so a blocked
// robot can't spin forever, and a coast allowance to absorb stop overshoot
// (raise from 0 if measured turns consistently land long).
constexpr uint32_t kDanceTurnTimeoutMs = 6000;
constexpr float kTurnStopEarlyDeg = 0.0f;
