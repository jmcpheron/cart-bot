#pragma once
#include <cstdint>

// Phone connects to this AP, then browses to http://192.168.4.1
constexpr const char* kApSsid = "cartbot-tx";
constexpr const char* kApPass = "cartbot123";  // min 8 chars for WPA2

// Robot's MAC address. Broadcast (all 0xFF) works out of the box with no
// pairing — fine for one robot. For the two-robot production system, run
// `mac` on each robot's console and give each transmitter slot a real MAC
// (unicast also enables per-packet delivery ACKs).
constexpr uint8_t kRobotMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
