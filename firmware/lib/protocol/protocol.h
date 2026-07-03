#pragma once
#include <cstdint>

// Wire protocol shared by robot and transmitter. Header-only on purpose — the
// ESP-NOW glue differs per side and lives in each target's comms code.

namespace proto {

// Both ends must sit on the same Wi-Fi channel for ESP-NOW to work. The
// transmitter's softAP pins the channel; the robot sets it explicitly.
constexpr uint8_t kEspNowChannel = 1;

enum Cmd : uint8_t {
    kCmdDrive = 0,
    kCmdStop  = 1,
    kCmdPing  = 2,
};

typedef struct __attribute__((packed)) {
    int8_t   vx;     // forward/back, -100..100
    int8_t   vy;     // strafe right/left, -100..100
    int8_t   omega;  // rotate CW/CCW, -100..100
    uint8_t  cmd;    // Cmd enum
    uint16_t seq;    // rolling sequence number for drop detection
} robot_cmd_t;

static_assert(sizeof(robot_cmd_t) == 6, "robot_cmd_t must be exactly 6 bytes");

}  // namespace proto
