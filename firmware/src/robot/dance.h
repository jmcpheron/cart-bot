#pragma once
#include <Arduino.h>

class SetpointStore;

// Dance sequencer: plays a list of timed velocity steps ON the robot, so a
// program keeps running if the phone sleeps. Program text format (no JSON):
//   "vx,vy,w,ms;vx,vy,w,ms;..."   velocities -100..100, duration ms.
// The executor feeds the watchdog itself; the failsafe stays fully armed.
namespace dance {

constexpr int kMaxSteps = 64;
constexpr uint32_t kMaxTotalMs = 120000;  // hard cap per program
constexpr int kSlots = 4;                 // NVS-persisted programs

void begin(SetpointStore* store);

// Parse + stage a program. Returns step count, or -1 on parse/limit error.
int setProgram(const char* text);
bool play();      // start the staged program (false if empty or playing)
void stop();      // abort; motors zeroed by the executor
bool playing();
int currentStep();  // 0-based while playing, -1 idle
int stepCount();

// Flash slots (survive reboot). saveSlot stores the currently staged text.
bool saveSlot(int slot, const String& name);
String loadSlot(int slot);   // stages it too; "" if slot empty
String slotName(int slot);

}  // namespace dance
