#pragma once

class SetpointStore;
class Battery;
class Motors;
class Imu;

// Line-based serial console (115200). Type `help` for the command list.
// Poll from loop(); reads are non-blocking except `demo`, which runs its
// scripted sequence in place while feeding the watchdog.
void console_begin(SetpointStore* store, Battery* battery, Motors* motors,
                   Imu* imu);
void console_poll();
