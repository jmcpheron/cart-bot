#pragma once
#include "failsafe.h"

// Last failsafe gate applied by the motor task (core 0); read by the web
// server for the drive page's status line. Single writer, plain reads.
extern volatile DriveGate g_robot_gate;

inline const char* gate_name(DriveGate g) {
    switch (g) {
        case DriveGate::kNormal: return "NORMAL";
        case DriveGate::kLimp:   return "LIMP";
        default:                 return "STOP";
    }
}
