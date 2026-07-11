#pragma once
#include "failsafe.h"

// Last failsafe gate applied by the motor task (core 0); read by the web
// server for the drive page's status line. Single writer, plain reads.
extern volatile DriveGate g_robot_gate;

// Yaw-hold loop state from the same tick, same single-writer pattern:
// whether a heading is being held and the omega trim last applied.
extern volatile bool g_yaw_holding;
extern volatile int8_t g_yaw_trim;

inline const char* gate_name(DriveGate g) {
    switch (g) {
        case DriveGate::kNormal: return "NORMAL";
        case DriveGate::kLimp:   return "LIMP";
        default:                 return "STOP";
    }
}
