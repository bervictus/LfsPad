#pragma once
#include <atomic>
#include <cstdint>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <windows.h>
#include <winsock2.h>
#include "controllers.hpp"
#include "rumble.hpp"
namespace pad {
extern HMODULE self;
extern std::atomic<bool> captureButtons;
extern std::atomic<bool> captureRaceAxes;
extern std::atomic<unsigned> captureQuickInput;
extern std::atomic<bool> monitorInput;
extern std::atomic<int> pedalSlot;
extern std::atomic<std::uint64_t> playStationPedals;
extern std::atomic<bool> stopping;
std::wstring path(const wchar_t *name);
void log(const char *fmt, ...);
bool supportedBuild();
void installGraphicsHook();
void frame(IDXGISwapChain *swap);
void releaseTarget();
void observeOutSim(const char *data, int size);
DrivingTelemetry drivingTelemetry();
DrivingTelemetry pollInSimRumble(bool enabled);
void driveMotors(ControllerId slot, MotorLevels levels, HWND window, bool diagnostic);
void stopMotors();
struct DrivingInputSample {
  ULONGLONG tick = 0;
  float steering = 0, triggers = 0, throttle = 0, brake = 0;
  bool steeringAvailable = false, triggersAvailable = false, povAvailable = false;
  bool splitPedals = false;
  bool buttonsFiltered = false, axesFiltered = false, ambiguous = false;
  std::uint32_t pov = 0xffffffff;
  std::uint16_t buttonsAvailable = 0, buttonsDown = 0, buttonsSuppressed = 0;
};
DrivingInputSample drivingInputSample(ULONGLONG since);
struct SimState {
  bool known = false;
  unsigned short flags = 0;
  unsigned mode = 0, subMode = 0;
  unsigned localPlayer = 0, viewedPlayer = 0;
};
struct ReplayState {
  bool active = false, multiplayer = false, paused = false, seeking = false;
  bool confirmedAbsent = false;
  unsigned position = 0, duration = 0;
};
SimState pollSim();
ReplayState replayState();
bool seekReplay(int direction);
bool simCommand(const char *command);
void simKey(const char *key);
void localKey(WORD virtualKey);
} // namespace pad
