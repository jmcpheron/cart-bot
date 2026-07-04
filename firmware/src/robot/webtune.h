#pragma once

class SetpointStore;
class Battery;

// Bench-tuning web server: robot hosts an AP (config.h) with a per-motor
// raw-duty page at http://192.168.4.1. Requires comms_begin() to have set up
// WIFI_AP_STA first. Poll from loop().
void webtune_begin(SetpointStore* store, Battery* battery);
void webtune_poll();
