#pragma once

class SetpointStore;
class Battery;

// Line-based serial console (115200). Type `help` for the command list.
// Poll from loop(); reads are non-blocking except `demo`, which runs its
// scripted sequence in place while feeding the watchdog.
void console_begin(SetpointStore* store, Battery* battery);
void console_poll();
