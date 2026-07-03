#pragma once
#include <cstdint>

class SetpointStore;

struct CommsStats {
    uint32_t packets = 0;
    uint32_t drops = 0;      // gaps detected via seq numbers
    uint32_t bad_len = 0;    // frames that weren't sizeof(robot_cmd_t)
    uint16_t last_seq = 0;
};

// Bring up Wi-Fi STA + ESP-NOW on proto::kEspNowChannel and route incoming
// drive commands into the setpoint store. Prints this robot's MAC (the
// transmitter needs it, unless it stays on broadcast).
void comms_begin(SetpointStore* store);
CommsStats comms_stats();
